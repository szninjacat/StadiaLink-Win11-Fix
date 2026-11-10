#!/usr/bin/env python3
"""Probe the private WinRT GATT activation factory on the running Windows build.

StadiaLink's driver/private-gatt.cpp pins two ABI-verified device interface
IIDs (Windows 26200 and 22631). On any other build it validates runtime
candidates from the CreateAsync object's own GetIids() list, then caches the
winner. This tool asks the activation factory which interfaces it implements
on this machine, so a new build can be investigated without guessing.

Read-only: it activates the factory and calls IInspectable::GetIids only.
It never connects to a controller and never writes a GATT characteristic.
A 0x5xxx Data3 value is only a candidate: the driver additionally checks the
vtable, runs the whole HID GATT chain, isolates the call with SEH and
blacklists any IID that crashes the host. See docs/COMPATIBILITY.md.
With --deep it also inspects the factory vtable; it stops before calling into
a private interface when this process only received combase's generic
activation-factory proxy (the real facet is available inside the UMDF host).

Usage:  python tests/probe-private-gatt.py
        python tests/probe-private-gatt.py --deep
"""
import ctypes as C
import sys
import uuid
import winreg


class GUID(C.Structure):
    _fields_ = [
        ("Data1", C.c_uint32),
        ("Data2", C.c_uint16),
        ("Data3", C.c_uint16),
        ("Data4", C.c_ubyte * 8),
    ]


HRESULT = C.c_long
HSTRING = C.c_void_p

CLASS_NAME = "Microsoft.Bluetooth.Profiles.Gatt.Interface.GattClientDevice"
IID_INSPECTABLE = "af86e2e0-b12d-4c6a-9c5a-d7aa65101e90"
IID_ACTIVATION_FACTORY = "00000035-0000-0000-c000-000000000046"
IID_ASYNCINFO = "00000036-0000-0000-c000-000000000046"

# Values hard-coded in driver/private-gatt.cpp, verified on real machines.
IID_STATICS_26200 = "cbaa1a5f-dcef-508b-acc6-158f1e160c0f"
IID_DEVICE_26200 = "6bbede1f-797f-5c17-8ac7-3f342037cb59"
IID_DEVICE_22631 = "0799a6a6-e942-5218-8a0e-7c9b996e4158"

VERIFIED_DEVICE_IIDS = {
    IID_DEVICE_26200: "VERIFIED device IID (Windows 26200)",
    IID_DEVICE_22631: "VERIFIED device IID (Windows 22631 / 23H2)",
}

STD_IIDS = {
    IID_INSPECTABLE: "IInspectable",
    IID_ACTIVATION_FACTORY: "IActivationFactory",
    "00000000-0000-0000-c000-000000000046": "IUnknown",
    "94ea2b94-e9cc-49e0-c0ff-ee64ca8f5b90": "IAgileObject",
    "00000038-0000-0000-c000-000000000046": "IWeakReferenceSource",
    "96369f54-8eb6-48f0-abce-c1f211ab2c85": "IStringable",
}

KNOWN_HRESULTS = {
    0x00000000: "S_OK",
    0x80004002: "E_NOINTERFACE (interface not implemented by this object)",
    0x80040154: "REGDB_E_CLASSNOTREG (class not registered)",
    0x80040111: "CLASS_E_CLASSNOTAVAILABLE",
    0x80070005: "E_ACCESSDENIED",
    0x800401f0: "CO_E_NOTINITIALIZED",
}


def make_guid(text):
    return GUID.from_buffer_copy(uuid.UUID(text).bytes_le)


def guid_str(g):
    # Canonical 8-4-4-4-12 form. The old output omitted the dash inside Data4,
    # so standard IIDs never matched STD_IIDS and every 0x5xxx IID looked like
    # an unverified candidate - including the shared statics IID.
    d4 = "".join("%02X" % x for x in g.Data4)
    return "%08X-%04X-%04X-%s-%s" % (g.Data1, g.Data2, g.Data3, d4[:4], d4[4:])


def hr_text(value):
    code = value & 0xFFFFFFFF
    return "0x%08X %s" % (code, KNOWN_HRESULTS.get(code, ""))


import os

_system = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32")
# On Windows 10/11 the WinRT foundation exports live in combase.dll; the
# runtimeobject.lib import library resolves there, and runtimeobject.dll is
# not necessarily present as a real file.
ro = C.WinDLL(os.path.join(_system, "combase.dll"))
ole = C.WinDLL(os.path.join(_system, "ole32.dll"))

ro.RoInitialize.argtypes = [C.c_uint32]
ro.RoInitialize.restype = HRESULT
ro.WindowsCreateString.argtypes = [C.c_wchar_p, C.c_uint32, C.POINTER(HSTRING)]
ro.WindowsCreateString.restype = HRESULT
ro.WindowsDeleteString.argtypes = [HSTRING]
ro.WindowsDeleteString.restype = HRESULT
ro.RoGetActivationFactory.argtypes = [HSTRING, C.POINTER(GUID), C.POINTER(C.c_void_p)]
ro.RoGetActivationFactory.restype = HRESULT
ole.CoTaskMemFree.argtypes = [C.c_void_p]
ole.CoTaskMemFree.restype = None

GETIIDS = C.WINFUNCTYPE(HRESULT, C.c_void_p, C.POINTER(C.c_uint32), C.POINTER(C.POINTER(GUID)))
RELEASE = C.WINFUNCTYPE(C.c_ulong, C.c_void_p)


def vtable_slot(obj, index):
    vtbl = C.cast(obj, C.POINTER(C.c_void_p))[0]
    return C.cast(
        C.c_void_p(vtbl + C.sizeof(C.c_void_p) * index),
        C.POINTER(C.c_void_p),
    )[0]


def get_iids(obj):
    fn = GETIIDS(vtable_slot(obj, 3))  # IInspectable::GetIids
    count = C.c_uint32(0)
    array = C.POINTER(GUID)()
    hr = fn(obj, C.byref(count), C.byref(array))
    if hr != 0 or not array:
        return hr, []
    found = [array[i] for i in range(count.value)]
    ole.CoTaskMemFree(C.cast(array, C.c_void_p))
    return hr, found


def build_number():
    try:
        with winreg.OpenKey(
            winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Microsoft\Windows NT\CurrentVersion"
        ) as key:
            return "%s.%s" % (
                winreg.QueryValueEx(key, "CurrentBuild")[0],
                winreg.QueryValueEx(key, "UBR")[0],
            )
    except OSError:
        return "unknown"


def registry_info():
    path = r"SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId" + "\\" + CLASS_NAME
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, path) as key:
            values = {}
            i = 0
            while True:
                try:
                    name, data, _ = winreg.EnumValue(key, i)
                    values[name] = data
                    i += 1
                except OSError:
                    break
            return values
    except OSError as exc:
        return {"error": str(exc)}


class RemoteAddress(C.Structure):
    _fields_ = [
        ("Address", C.c_uint64),
        ("Type", C.c_uint32),
        ("Padding", C.c_uint32),
    ]


QI = C.WINFUNCTYPE(HRESULT, C.c_void_p, C.POINTER(GUID), C.POINTER(C.c_void_p))
CREATEASYNC = C.WINFUNCTYPE(
    HRESULT, C.POINTER(RemoteAddress), C.c_uint32, HSTRING, HSTRING,
    C.c_uint32, C.POINTER(C.c_void_p),
)
GETSTATUS = C.WINFUNCTYPE(HRESULT, C.c_void_p, C.POINTER(C.c_int32))
GETRESULTS = C.WINFUNCTYPE(HRESULT, C.c_void_p, C.POINTER(C.c_void_p))


def module_of(address):
    """Return the module name that owns a code address (best effort)."""
    import os

    kernel = C.WinDLL("kernel32.dll")
    kernel.GetModuleHandleExW.argtypes = [C.c_uint32, C.c_void_p, C.POINTER(C.c_void_p)]
    kernel.GetModuleFileNameW.argtypes = [C.c_void_p, C.c_wchar_p, C.c_uint32]
    handle = C.c_void_p()
    # GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | UNCHANGED_REFCOUNT
    if not kernel.GetModuleHandleExW(6, C.c_void_p(address), C.byref(handle)):
        return "?"
    name = C.create_unicode_buffer(260)
    kernel.GetModuleFileNameW(handle, name, 260)
    return os.path.basename(name.value)


def iid_note(text):
    """Explain an IID without ever implying a candidate is the device facet."""
    low = text.lower()
    notes = []
    standard = STD_IIDS.get(low)
    if standard:
        notes.append(standard)
    if low in VERIFIED_DEVICE_IIDS:
        notes.append(VERIFIED_DEVICE_IIDS[low])
    elif low == IID_STATICS_26200:
        notes.append("shared statics IID")
    elif (uuid.UUID(text).fields[2] & 0xF000) == 0x5000:
        notes.append("0x5xxx parameterized IID (candidate, NOT verified)")
    return " <- " + "; ".join(notes) if notes else ""


def dump_iids(obj, label):
    hr, iids = get_iids(obj)
    print("      %s GetIids : %s, %d interface(s)" % (label, hr_text(hr), len(iids)))
    for index, g in enumerate(iids):
        text = guid_str(g)
        print("        [%d] %s%s" % (index, text, iid_note(text)))
    return hr == 0


def deep_probe(address, resolve):
    """Walk the same private call chain as driver/private-gatt.cpp GetDevice()."""
    import time

    hs = HSTRING()
    if ro.WindowsCreateString(CLASS_NAME, len(CLASS_NAME), C.byref(hs)) != 0:
        print("  WindowsCreateString failed")
        return 1
    iid = make_guid(IID_STATICS_26200)
    factory = C.c_void_p()
    hr = ro.RoGetActivationFactory(hs, C.byref(iid), C.byref(factory))
    print("  [1] RoGetActivationFactory(statics) : %s" % hr_text(hr))
    if hr != 0 or not factory.value:
        return 1

    remote = RemoteAddress(address, 1, 0)
    operation = C.c_void_p()
    slot6 = vtable_slot(factory, 6)
    owner = module_of(slot6)
    print("  [2] statics slot 6 = 0x%X [%s]" % (slot6, owner))
    if owner.lower() == "combase.dll":
        print("      combase returned its generic activation-factory proxy, not the")
        print("      private statics vtable. Calling it here would be an ABI mismatch.")
        print("      The real facet is only reachable inside the UMDF driver host.")
        print("      Skipping CreateAsync; the IID comparison above still holds.")
        return 0
    create = CREATEASYNC(slot6)
    try:
        hr = create(C.byref(remote), 0, None, None, 0, C.byref(operation))
    except OSError as exc:
        print("  [2] CreateAsync raised %s (private ABI differs); stopping." % exc)
        return 1
    print("  [2] GattStatics::CreateAsync        : %s (operation=%s)" %
          (hr_text(hr), hex(operation.value or 0)))
    if hr != 0 or not operation.value:
        return 1
    dump_iids(operation, "operation")

    ai = make_guid(IID_ASYNCINFO)
    info = C.c_void_p()
    qhr = QI(vtable_slot(operation, 0))(operation, C.byref(ai), C.byref(info))
    print("  [3] operation QI(IAsyncInfo)       : %s" % hr_text(qhr))
    if qhr != 0 or not info.value:
        return 1

    status = C.c_int32(0)
    get_status = GETSTATUS(vtable_slot(info, 7))
    deadline = time.time() + 3.0
    while time.time() < deadline:
        if get_status(info, C.byref(status)) != 0:
            break
        if status.value != 0:  # Started = 0
            break
        time.sleep(0.005)
    print("  [4] IAsyncInfo::Status             : %d (0=Started, 1=Completed, 2=Canceled, 3=Error)" %
          status.value)

    if not resolve:
        print("  (stop before GetResults; pass --resolve to continue)")
        return 0
    if status.value != 1:
        print("  (async operation did not complete, skipping GetResults)")
        return 1

    created = C.c_void_p()
    get_results = GETRESULTS(vtable_slot(operation, 8))
    gr = get_results(operation, C.byref(created))
    print("  [5] operation::GetResults (slot 8) : %s (created=%s)" %
          (hr_text(gr), hex(created.value or 0)))
    if gr != 0 or not created.value:
        return 1
    dump_iids(created, "created")

    diid = make_guid(IID_DEVICE_26200)
    device = C.c_void_p()
    dhr = QI(vtable_slot(created, 0))(created, C.byref(diid), C.byref(device))
    print("  [6] created QI(device IID 26200)   : %s" % hr_text(dhr))
    return 0


def main():
    args = sys.argv[1:]
    ro.RoInitialize(1)  # RO_INIT_MULTITHREADED
    if "--deep" in args:
        address = 0xE5634CD38591
        for item in args:
            if item.startswith("--address="):
                address = int(item.split("=", 1)[1], 16)
        print("Private GATT call-chain probe (Windows build %s)" % build_number())
        print("  address=0x%012X resolve=%s" % (address, "--resolve" in args))
        return deep_probe(address, "--resolve" in args)

    print("Private GATT activation factory probe (Windows build %s)" % build_number())
    print("  class : %s" % CLASS_NAME)
    reg = registry_info()
    print("  registry ActivatableClassId : %s" % (reg if reg else "no values"))
    print("  WinRT apartment            : %s" % hr_text(ro.RoInitialize(1)))

    hs = HSTRING()
    hr = ro.WindowsCreateString(CLASS_NAME, len(CLASS_NAME), C.byref(hs))
    if hr != 0:
        print("WindowsCreateString failed: %s" % hr_text(hr))
        return 1

    probes = [
        ("shared statics IID (26200 / 22631)", IID_STATICS_26200),
        ("IActivationFactory (standard)", IID_ACTIVATION_FACTORY),
        ("IInspectable (standard)", IID_INSPECTABLE),
        ("VERIFIED device IID (Windows 26200)", IID_DEVICE_26200),
        ("VERIFIED device IID (Windows 22631 / 23H2)", IID_DEVICE_22631),
    ]

    for label, value in probes:
        iid = make_guid(value)
        factory = C.c_void_p()
        hr = ro.RoGetActivationFactory(hs, C.byref(iid), C.byref(factory))
        print()
        print("  %s" % label)
        print("    requested IID  : %s" % value.upper())
        print("    activation     : %s" % hr_text(hr))
        if hr == 0 and factory.value:
            hr2, iids = get_iids(factory)
            print("    GetIids        : %s, %d interface(s)" % (hr_text(hr2), len(iids)))
            for index, g in enumerate(iids):
                text = guid_str(g)
                note = iid_note(text)
                if text.lower() == value.lower():
                    note += " (requested)"
                print("      [%d] %s %s" % (index, text, note))
            RELEASE(vtable_slot(factory, 2))(factory)

    ro.WindowsDeleteString(hs)
    print()
    print("The two VERIFIED device IIDs are tried first on every build.")
    print("Device IIDs belong to the object returned by CreateAsync, not to the")
    print("activation factory, so E_NOINTERFACE for them above is expected.")
    print("A 0x5xxx parameterized IID is only a candidate. On an unknown build")
    print("the driver ranks such candidates, validates the vtable and the whole")
    print("HID GATT chain under an SEH guard, then caches the winner for later")
    print("boots. Candidates that fail are blacklisted; status report byte 62")
    print("shows the candidate count plus crash/cache flags.")
    print("See docs/COMPATIBILITY.md before adding an IID to DeviceIids[].")
    return 0


if __name__ == "__main__":
    sys.exit(main())
