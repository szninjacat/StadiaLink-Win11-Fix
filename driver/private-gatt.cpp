// Native Stadia rumble transport. Private ABI recovered from the installed
// Microsoft.Bluetooth.Service and HidOverGatt public symbols (Windows 26200).
//
// The private GattClientDevice device IID is not stable across Windows builds:
// 22631 uses 0799A6A6-..., 26200 uses 6BBEDE1F-..., while the statics IID is
// shared. Asking for the wrong device IID fails with E_NOINTERFACE, which is
// the "no vibration over Bluetooth" symptom.
//
// Resolution order on every build:
//   1. the two hardware-verified IIDs (unchanged behaviour on 22631 / 26200);
//   2. an IID that already passed full validation on an earlier boot (cached);
//   3. runtime candidates from IInspectable::GetIids, each of which must pass
//      vtable sanity checks AND the complete HID GATT chain before it is used.
//
// Blindly calling slots 11/14 on a guessed interface is unsafe. Unknown facets
// are therefore only used inside an SEH guard, and a persistent attempt marker
// turns a host crash into a blacklist entry on the next boot. See
// docs/COMPATIBILITY.md for the whole policy and the manual IID procedure.
#define WIN32_LEAN_AND_MEAN
#include "private-gatt.h"
#include "iid-policy.h"
#include <roapi.h>
#include <winstring.h>
#include <inspectable.h>
#include <asyncinfo.h>
#include <windows.storage.streams.h>
#include <robuffer.h>
#include <combaseapi.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
#include <new>
#include <memory>

// Compatibility-first default. Build with STADIALINK_AUTO_IID=0 to restore the
// strict fail-closed behaviour (never touch an unverified vtable).
#ifndef STADIALINK_AUTO_IID
#define STADIALINK_AUTO_IID 1
#endif
// Honour the old switch so an existing "0" build keeps its meaning.
#if defined(STADIALINK_ALLOW_UNVERIFIED_IID) && !STADIALINK_ALLOW_UNVERIFIED_IID
#undef STADIALINK_AUTO_IID
#define STADIALINK_AUTO_IID 0
#endif

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Wrappers::HStringReference;
using ABI::Windows::Storage::Streams::IBuffer;
using ABI::Windows::Storage::Streams::IBufferFactory;
using Windows::Storage::Streams::IBufferByteAccess;
template<typename T> static T Slot(IInspectable* object,size_t index) {
    return reinterpret_cast<T>((*reinterpret_cast<void***>(object))[index]);
}
struct RemoteAddress { UINT64 Address;UINT32 Type;UINT32 Padding; };
struct GattStatics:IInspectable {
    virtual HRESULT STDMETHODCALLTYPE CreateAsync(RemoteAddress,UINT32,HSTRING,HSTRING,UINT32,IInspectable**)=0;
};
// The exact ABI sizes and offsets were checked against Microsoft's call sites.
struct ServiceInfo { USHORT First,Last;GUID Uuid;UINT32 Flags;USHORT Handle,Padding; };
struct CharacteristicInfo { USHORT First,Last;GUID Uuid;UINT32 Properties;USHORT Handle,ValueHandle;UINT32 Padding; };
static_assert(sizeof(ServiceInfo)==28,"service ABI");
static_assert(sizeof(CharacteristicInfo)==32,"characteristic ABI");
static GUID BluetoothUuid(USHORT value) { return {value,0,0x1000,{0x80,0,0,0x80,0x5f,0x9b,0x34,0xfb}}; }
struct Apartment {
    HRESULT Status=RoInitialize(RO_INIT_MULTITHREADED);
    ~Apartment() {if(SUCCEEDED(Status)) RoUninitialize();}
    HRESULT Check() const {return Status==RPC_E_CHANGED_MODE?S_OK:Status;}
};
struct Session {
    ComPtr<IAgileReference> Device,Service;
    ComPtr<IInspectable> Characteristic;
    GUID CharacteristicIID={};
    STADIA_GATT_DIAGNOSTICS Diagnostics={};
};
static HRESULT Await(IInspectable* operation,IInspectable** result,ULONG* asyncStage=nullptr) {
    *result=nullptr;if(!operation) return E_UNEXPECTED;
    if(asyncStage) *asyncStage=STADIA_GATT_ASYNC_NONE;
    ComPtr<ABI::Windows::Foundation::IAsyncInfo> info;
    HRESULT hr=operation->QueryInterface(IID_PPV_ARGS(&info));
    if(FAILED(hr)) {if(asyncStage) *asyncStage=STADIA_GATT_ASYNC_INFO_QI;return hr;}
    AsyncStatus status=Started;DWORD start=GetTickCount();
    while(SUCCEEDED(hr=info->get_Status(&status)) && status==Started && GetTickCount()-start<2000) Sleep(2);
    if(FAILED(hr)) {if(asyncStage) *asyncStage=STADIA_GATT_ASYNC_STATUS;}
    else if(status==Started) {info->Cancel();hr=HRESULT_FROM_WIN32(ERROR_TIMEOUT);if(asyncStage) *asyncStage=STADIA_GATT_ASYNC_TIMEOUT;}
    else if(status==Completed) {
        hr=Slot<HRESULT (STDMETHODCALLTYPE *)(IInspectable*,IInspectable**)>(operation,8)(operation,result);
        if(FAILED(hr) && asyncStage) *asyncStage=STADIA_GATT_ASYNC_RESULTS;
    }
    else {
        // get_Status failures, cancellations and async errors used to be
        // collapsed into ERROR_TIMEOUT. Report them distinctly.
        HRESULT error=E_FAIL;info->get_ErrorCode(&error);
        hr=FAILED(error)?error:E_FAIL;
        if(asyncStage) *asyncStage=STADIA_GATT_ASYNC_ERROR;
    }
    info->Close();return hr;
}
static HRESULT CheckCommunication(IInspectable* result) {
    if(!result) return E_UNEXPECTED;
    UINT32 status=0;
    HRESULT hr=Slot<HRESULT (STDMETHODCALLTYPE *)(IInspectable*,UINT32*)>(result,6)(result,&status);
    if(FAILED(hr)) return hr;
    // Successful async completion alone does not prove a successful ATT operation.
    if(status==0) return S_OK;
    if(status==3) return E_ACCESSDENIED;
    return HRESULT_FROM_WIN32(ERROR_GEN_FAILURE);
}
static HRESULT ResultVector(IInspectable* result,IInspectable** vector,UINT32* count) {
    *vector=nullptr;*count=0;
    HRESULT hr=CheckCommunication(result);if(FAILED(hr)) return hr;
    hr=Slot<HRESULT (STDMETHODCALLTYPE *)(IInspectable*,IInspectable**)>(result,9)(result,vector);
    if(FAILED(hr) || !*vector) return FAILED(hr)?hr:E_UNEXPECTED;
    hr=Slot<HRESULT (STDMETHODCALLTYPE *)(IInspectable*,UINT32*)>(*vector,7)(*vector,count);
    if(SUCCEEDED(hr) && *count>128) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    return hr;
}

// Microsoft.Bluetooth.Profiles.Gatt.Interface.GattClientDevice device facet.
// Each IID was read from the inbox HidOverGatt.dll of its build, where it sits
// next to the statics IID that both builds share. The same values live in
// iid-policy.h so the exclusion policy is unit-testable.
static const GUID DeviceIids[] = {
    {0x6bbede1f,0x797f,0x5c17,{0x8a,0xc7,0x3f,0x34,0x20,0x37,0xcb,0x59}}, // Windows 26200 (verified)
    {0x0799a6a6,0xe942,0x5218,{0x8a,0x0e,0x7c,0x9b,0x99,0x6e,0x41,0x58}}, // Windows 22631 (verified)
};
#define DEVICE_IID_COUNT (sizeof(DeviceIids)/sizeof(DeviceIids[0]))

static HRESULT QueryDeviceFacet(IInspectable* created,const GUID& iid,IInspectable** device) {
    ComPtr<IInspectable> facet;
    HRESULT hr=created->QueryInterface(iid,(void**)facet.GetAddressOf());
    if(FAILED(hr) || !facet) return FAILED(hr)?hr:E_NOINTERFACE;
    *device=facet.Detach();
    return S_OK;
}

// ---------------------------------------------------------------------------
// Persistent, best-effort state under HKCU (the UMDF host's own hive).
// If the key cannot be opened every write is a no-op and auto-adaptation still
// works; only the crash circuit breaker is lost.
// ---------------------------------------------------------------------------
static const wchar_t* const kStorePath=L"SOFTWARE\\StadiaLink\\PrivateGatt";
static const size_t kMaxBlacklist=4;
static const size_t kMaxAutoAttempts=3;

struct IidStore {
    GUID verified={};bool hasVerified=false;
    GUID blacklist[kMaxBlacklist]={};size_t blacklistCount=0;
    bool crashDetected=false;GUID crashed={};
    bool available=false;
};
static HKEY OpenStoreKey(REGSAM access) {
    HKEY key=nullptr;
    LONG rc=RegCreateKeyExW(HKEY_CURRENT_USER,kStorePath,0,nullptr,
                            REG_OPTION_NON_VOLATILE,access,nullptr,&key,nullptr);
    return rc==ERROR_SUCCESS?key:nullptr;
}
static bool ReadGuid(HKEY key,const wchar_t* name,GUID* out) {
    DWORD size=sizeof(*out),type=0;
    return RegQueryValueExW(key,name,nullptr,&type,reinterpret_cast<BYTE*>(out),&size)==ERROR_SUCCESS
        && type==REG_BINARY && size==sizeof(*out);
}
static void WriteGuid(HKEY key,const wchar_t* name,const GUID& g) {
    RegSetValueExW(key,name,0,REG_BINARY,reinterpret_cast<const BYTE*>(&g),sizeof(g));
}
static size_t ReadList(HKEY key,const wchar_t* name,GUID* out,size_t cap) {
    BYTE buffer[16*8]={};DWORD size=sizeof(buffer),type=0;
    if(RegQueryValueExW(key,name,nullptr,&type,buffer,&size)!=ERROR_SUCCESS
       || type!=REG_BINARY || size%16!=0) return 0;
    size_t n=size/16;if(n>cap) n=cap;
    CopyMemory(out,buffer,n*16);
    return n;
}
static void WriteList(HKEY key,const wchar_t* name,const GUID* list,size_t count) {
    if(count) RegSetValueExW(key,name,0,REG_BINARY,reinterpret_cast<const BYTE*>(list),(DWORD)(count*16));
    else RegDeleteValueW(key,name);
}
static IidStore LoadIidStore() {
    IidStore s;
    HKEY key=OpenStoreKey(KEY_READ|KEY_WRITE);
    if(!key) return s; // no persistence: still allow auto-adaptation
    s.available=true;
    s.hasVerified=ReadGuid(key,L"Verified",&s.verified);
    s.blacklistCount=ReadList(key,L"Blacklist",s.blacklist,kMaxBlacklist);
    GUID attempt;
    if(ReadGuid(key,L"Attempt",&attempt)) {
        // The host died before the attempt marker was cleared. Never retry this
        // interface automatically; other candidates may still be tried.
        s.crashDetected=true;s.crashed=attempt;
        s.blacklistCount=StadiaIidListAdd(s.blacklist,s.blacklistCount,kMaxBlacklist,&attempt);
        WriteList(key,L"Blacklist",s.blacklist,s.blacklistCount);
        RegDeleteValueW(key,L"Attempt");
    }
    RegCloseKey(key);
    return s;
}
static IidStore& IidStoreState() {
    static IidStore state=LoadIidStore();
    return state;
}
static void SetAttempt(const GUID& g) {
    HKEY key=OpenStoreKey(KEY_WRITE);if(!key) return;
    WriteGuid(key,L"Attempt",g);RegCloseKey(key);
}
static void ClearAttempt() {
    HKEY key=OpenStoreKey(KEY_WRITE);if(!key) return;
    RegDeleteValueW(key,L"Attempt");RegCloseKey(key);
}
static void SetVerified(const GUID& g) {
    IidStore& s=IidStoreState();s.verified=g;s.hasVerified=true;
    HKEY key=OpenStoreKey(KEY_WRITE);if(!key) return;
    WriteGuid(key,L"Verified",g);RegCloseKey(key);
}
static void ClearVerified() {
    IidStore& s=IidStoreState();s.hasVerified=false;s.verified={};
    HKEY key=OpenStoreKey(KEY_WRITE);if(!key) return;
    RegDeleteValueW(key,L"Verified");RegCloseKey(key);
}
static void AddBlacklist(const GUID& g) {
    IidStore& s=IidStoreState();
    s.blacklistCount=StadiaIidListAdd(s.blacklist,s.blacklistCount,kMaxBlacklist,&g);
    HKEY key=OpenStoreKey(KEY_WRITE);if(!key) return;
    WriteList(key,L"Blacklist",s.blacklist,s.blacklistCount);RegCloseKey(key);
}

// ---------------------------------------------------------------------------
// Static vtable sanity check. Nothing is executed: we only verify that the
// unknown facet's method slots point into committed executable code, and rank
// candidates by how many of their methods live in the same image as the
// activation factory (i.e. the same private Windows component).
// ---------------------------------------------------------------------------
static bool IsExecutableCode(void* p) {
    if(!p) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if(!VirtualQuery(p,&mbi,sizeof(mbi)) || mbi.State!=MEM_COMMIT) return false;
    DWORD protect=mbi.Protect & 0xFF;
    return protect==PAGE_EXECUTE || protect==PAGE_EXECUTE_READ
        || protect==PAGE_EXECUTE_READWRITE || protect==PAGE_EXECUTE_WRITECOPY;
}
static HRESULT ScoreDeviceVtable(IInspectable* facet,void* reference,int* score) {
    *score=0;
    void** vt=*reinterpret_cast<void***>(facet);
    if(!vt) return E_UNEXPECTED;
    void* refBase=nullptr;MEMORY_BASIC_INFORMATION ref;
    if(reference && VirtualQuery(reference,&ref,sizeof(ref))) refBase=ref.AllocationBase;
    void* seen[9]={};int distinct=0;int sameImage=0;
    for(size_t i=6;i<=14;i++) {
        void* method=vt[i];
        if(!IsExecutableCode(method)) return HRESULT_FROM_WIN32(ERROR_INVALID_FUNCTION);
        bool duplicate=false;
        for(int j=0;j<distinct;j++) if(seen[j]==method) {duplicate=true;break;}
        if(!duplicate && distinct<9) seen[distinct++]=method;
        MEMORY_BASIC_INFORMATION mbi;
        if(refBase && VirtualQuery(method,&mbi,sizeof(mbi)) && mbi.AllocationBase==refBase) sameImage++;
    }
    // A real device interface has several distinct methods. A stub or an
    // unrelated interface that repeats one implementation is not usable.
    if(distinct<5) return HRESULT_FROM_WIN32(ERROR_INVALID_FUNCTION);
    *score=sameImage;
    return S_OK;
}

struct Candidate { GUID iid;ULONG order;int score; };
static HRESULT CollectCandidates(IInspectable* created,void* reference,
                                 Candidate* out,size_t cap,size_t* count,ULONG* plausible) {
    *count=0;*plausible=0;
    ULONG n=0;GUID* iids=nullptr;
    HRESULT hr=Slot<HRESULT (STDMETHODCALLTYPE *)(IInspectable*,ULONG*,GUID**)>(created,3)
        (created,&n,&iids);
    if(FAILED(hr)) return hr;
    if(!iids || !n || n>64) {
        if(iids) CoTaskMemFree(iids);
        return E_UNEXPECTED;
    }
    IidStore& store=IidStoreState();
    Candidate all[64]={};size_t total=0;
    for(ULONG i=0;i<n;i++) {
        if(!StadiaIidIsPlausible(&iids[i])) continue;
        if(*plausible<0x3F) (*plausible)++;
        if(i>0x1F) continue; // keep the diagnostic index inside 0x80..0x9F
        if(StadiaIidListContains(store.blacklist,store.blacklistCount,&iids[i])) continue;
        ComPtr<IInspectable> facet;
        hr=QueryDeviceFacet(created,iids[i],facet.GetAddressOf());
        if(FAILED(hr) || !facet) continue;
        int score=0;
        if(FAILED(ScoreDeviceVtable(facet.Get(),reference,&score))) continue;
        all[total].iid=iids[i];all[total].order=i;all[total].score=score;total++;
        if(total==64) break;
    }
    CoTaskMemFree(iids);
    // Prefer facets implemented in the factory's image; stable order otherwise.
    for(size_t a=1;a<total;a++) {
        Candidate key=all[a];size_t b=a;
        while(b>0 && all[b-1].score<key.score) {all[b]=all[b-1];b--;}
        all[b]=key;
    }
    *count=total<cap?total:cap;
    for(size_t a=0;a<*count;a++) out[a]=all[a];
    return S_OK;
}

static HRESULT CreateDeviceObject(UINT64 address,IInspectable** created,void** reference,Session& session) {
    *created=nullptr;*reference=nullptr;
    session.Diagnostics.Stage=STADIA_GATT_STAGE_ACTIVATION;
    ComPtr<GattStatics> factory;
    HRESULT hr=RoGetActivationFactory(
        HStringReference(L"Microsoft.Bluetooth.Profiles.Gatt.Interface.GattClientDevice").Get(),
        STADIA_IID_STATICS,(void**)factory.GetAddressOf());
    if(FAILED(hr)) return hr;
    *reference=Slot<void*>(reinterpret_cast<IInspectable*>(factory.Get()),6);

    session.Diagnostics.Stage=STADIA_GATT_STAGE_CREATE_ASYNC;
    RemoteAddress remote={address,1,0};ComPtr<IInspectable> operation;
    hr=factory->CreateAsync(remote,0,nullptr,nullptr,0,&operation);if(FAILED(hr)) return hr;

    session.Diagnostics.Stage=STADIA_GATT_STAGE_AWAIT_CREATE;
    ComPtr<IInspectable> createdObject;
    hr=Await(operation.Get(),&createdObject,&session.Diagnostics.AsyncStage);
    if(FAILED(hr)) return hr;
    if(!createdObject) return E_UNEXPECTED;
    *created=createdObject.Detach();
    return S_OK;
}

static HRESULT ReadReportReference(IInspectable* discovery,IInspectable* characteristic,const CharacteristicInfo& info,UCHAR* id,UCHAR* type,ULONG* asyncStage) {
    GUID reference=BluetoothUuid(0x2908);
    ComPtr<IInspectable> operation;
    HRESULT hr=Slot<HRESULT (STDMETHODCALLTYPE *)(IInspectable*,const CharacteristicInfo*,const GUID*,UINT32,IInspectable**)>(discovery,11)
        (discovery,&info,&reference,0,&operation);
    if(FAILED(hr)) return hr;
    ComPtr<IInspectable> result;hr=Await(operation.Get(),&result,asyncStage);if(FAILED(hr)) return hr;
    ComPtr<IInspectable> vector;UINT32 count=0;hr=ResultVector(result.Get(),&vector,&count);if(FAILED(hr)) return hr;
    if(count!=1) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    // Descriptor info is kept opaque; the exact value returned by discovery is
    // passed to the read method without inventing or modifying an attribute handle.
    alignas(16) BYTE descriptor[128]={};
    hr=Slot<HRESULT (STDMETHODCALLTYPE *)(IInspectable*,UINT32,void*)>(vector.Get(),6)(vector.Get(),0,descriptor);
    if(FAILED(hr)) return hr;
    operation.Reset();
    hr=Slot<HRESULT (STDMETHODCALLTYPE *)(IInspectable*,const void*,UINT32,UINT32,IInspectable**)>(characteristic,9)
        (characteristic,descriptor,0,0,&operation);
    if(FAILED(hr)) return hr;
    result.Reset();hr=Await(operation.Get(),&result,asyncStage);if(FAILED(hr)) return hr;
    hr=CheckCommunication(result.Get());if(FAILED(hr)) return hr;
    ComPtr<IBuffer> buffer;
    hr=Slot<HRESULT (STDMETHODCALLTYPE *)(IInspectable*,IBuffer**)>(result.Get(),8)(result.Get(),&buffer);
    if(FAILED(hr) || !buffer) return FAILED(hr)?hr:E_UNEXPECTED;
    UINT32 length=0;hr=buffer->get_Length(&length);if(FAILED(hr)) return hr;
    if(length!=2) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    ComPtr<IBufferByteAccess> access;hr=buffer.As(&access);if(FAILED(hr)) return hr;
    BYTE* bytes=nullptr;hr=access->Buffer(&bytes);if(FAILED(hr) || !bytes) return FAILED(hr)?hr:E_UNEXPECTED;
    *id=bytes[0];*type=bytes[1];return S_OK;
}

// Everything after the device facet is resolved. Its checks are also the ABI
// validator: exactly one HID service, matching service UUID, Report Reference
// 5/2. A wrongly guessed interface cannot pass all of them.
static HRESULT TryOpenWithDevice(IInspectable* device,Session& session) {
    session.Diagnostics.Stage=STADIA_GATT_STAGE_HID_SERVICE;
    GUID hid=BluetoothUuid(0x1812);ComPtr<IInspectable> operation;
    HRESULT hr=Slot<HRESULT (STDMETHODCALLTYPE *)(IInspectable*,const GUID*,UINT32,IInspectable**)>(device,11)(device,&hid,0,&operation);
    if(FAILED(hr)) return hr;
    ComPtr<IInspectable> result;hr=Await(operation.Get(),&result,&session.Diagnostics.AsyncStage);if(FAILED(hr)) return hr;
    ComPtr<IInspectable> vector;UINT32 count=0;hr=ResultVector(result.Get(),&vector,&count);if(FAILED(hr)) return hr;
    if(count!=1) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    ServiceInfo serviceInfo={};
    hr=Slot<HRESULT (STDMETHODCALLTYPE *)(IInspectable*,UINT32,ServiceInfo*)>(vector.Get(),6)(vector.Get(),0,&serviceInfo);
    if(FAILED(hr)) return hr;
    if(!IsEqualGUID(serviceInfo.Uuid,hid)) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    session.Diagnostics.Stage=STADIA_GATT_STAGE_SERVICE_OBJECT;
    ComPtr<IInspectable> service;
    hr=Slot<HRESULT (STDMETHODCALLTYPE *)(IInspectable*,const ServiceInfo*,UINT64,UINT32,IInspectable**)>(device,14)
        (device,&serviceInfo,0x0000000100000001ULL,0,&service);
    if(FAILED(hr) || !service) return FAILED(hr)?hr:E_UNEXPECTED;
    session.Diagnostics.Stage=STADIA_GATT_STAGE_DISCOVERY;
    ComPtr<IInspectable> discovery;
    hr=Slot<HRESULT (STDMETHODCALLTYPE *)(IInspectable*,IInspectable**)>(service.Get(),11)(service.Get(),&discovery);
    if(FAILED(hr) || !discovery) return FAILED(hr)?hr:E_UNEXPECTED;
    operation.Reset();hr=Slot<HRESULT (STDMETHODCALLTYPE *)(IInspectable*,UINT32,IInspectable**)>(discovery.Get(),8)(discovery.Get(),0,&operation);
    if(FAILED(hr)) return hr;
    result.Reset();hr=Await(operation.Get(),&result,&session.Diagnostics.AsyncStage);if(FAILED(hr)) return hr;
    vector.Reset();hr=ResultVector(result.Get(),&vector,&count);if(FAILED(hr)) return hr;
    session.Diagnostics.Stage=STADIA_GATT_STAGE_CHARACTERISTICS;
    GUID reportUuid=BluetoothUuid(0x2a4d);
    for(UINT32 i=0;i<count;i++) {
        CharacteristicInfo info={};
        hr=Slot<HRESULT (STDMETHODCALLTYPE *)(IInspectable*,UINT32,CharacteristicInfo*)>(vector.Get(),6)(vector.Get(),i,&info);
        if(FAILED(hr)) return hr;
        if(!IsEqualGUID(info.Uuid,reportUuid) || !(info.Properties&0x08)) continue;
        ComPtr<IInspectable> characteristic;
        hr=Slot<HRESULT (STDMETHODCALLTYPE *)(IInspectable*,const CharacteristicInfo*,IInspectable**)>(service.Get(),12)(service.Get(),&info,&characteristic);
        if(FAILED(hr) || !characteristic) return FAILED(hr)?hr:E_UNEXPECTED;
        session.Diagnostics.Stage=STADIA_GATT_STAGE_REPORT_REF;
        UCHAR id=0,type=0;hr=ReadReportReference(discovery.Get(),characteristic.Get(),info,&id,&type,&session.Diagnostics.AsyncStage);
        if(FAILED(hr)) return hr;
        if(id!=5 || type!=2) continue;
        hr=RoGetAgileReference(AGILEREFERENCE_DEFAULT,__uuidof(IInspectable),device,&session.Device);if(FAILED(hr)) return hr;
        hr=RoGetAgileReference(AGILEREFERENCE_DEFAULT,__uuidof(IInspectable),service.Get(),&session.Service);if(FAILED(hr)) return hr;
        // The native characteristic proxy implements FtmBase. Preserve the
        // original typed interface facet rather than resolving its COM identity.
        session.Characteristic=characteristic;
        session.Diagnostics.ValueHandle=info.ValueHandle;session.Diagnostics.ReportId=id;session.Diagnostics.ReportType=type;
        session.Diagnostics.Stage=STADIA_GATT_STAGE_READY;
        return S_OK;
    }
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}

// No C++ objects in this frame: SEH cannot unwind them. A bad candidate that
// faults becomes an HRESULT instead of killing WUDFHost.exe.
static HRESULT GuardedTryOpen(IInspectable* device,Session* session) {
    __try {
        return TryOpenWithDevice(device,*session);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        return HRESULT_FROM_WIN32(ERROR_INVALID_FUNCTION);
    }
}

// Only reached when neither hardware-verified IID matched.
static HRESULT AutoAdapt(IInspectable* created,void* reference,Session& session) {
    IidStore& store=IidStoreState();
    int cacheUsed=0;
#if STADIALINK_AUTO_IID
    // A previously validated IID is cheaper and safer than a fresh guess.
    if(store.hasVerified
       && !StadiaIidListContains(store.blacklist,store.blacklistCount,&store.verified)) {
        ComPtr<IInspectable> facet;
        HRESULT hr=QueryDeviceFacet(created,store.verified,facet.GetAddressOf());
        if(SUCCEEDED(hr) && facet) {
            cacheUsed=1;
            STADIA_GATT_DIAGNOSTICS saved=session.Diagnostics;
            session.Diagnostics.IidIndex=STADIA_IID_SOURCE_CACHE;
            hr=GuardedTryOpen(facet.Get(),&session);
            if(SUCCEEDED(hr)) return S_OK;
            // A Windows update can invalidate the cache; drop it and re-discover.
            ClearVerified();
            session.Diagnostics=saved;
        }
    }

    Candidate candidates[kMaxAutoAttempts]={};size_t count=0;ULONG plausible=0;
    HRESULT discovery=CollectCandidates(created,reference,candidates,kMaxAutoAttempts,&count,&plausible);
    session.Diagnostics.CandidateCount=StadiaEncodeCandidates(plausible,store.crashDetected,cacheUsed);
    if(FAILED(discovery)) {
        session.Diagnostics.Stage=STADIA_GATT_STAGE_IID_UNVERIFIED;
        return discovery;
    }
    HRESULT last=HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    for(size_t i=0;i<count;i++) {
        ComPtr<IInspectable> facet;
        HRESULT hr=QueryDeviceFacet(created,candidates[i].iid,facet.GetAddressOf());
        if(FAILED(hr) || !facet) {last=hr;continue;}
        STADIA_GATT_DIAGNOSTICS saved=session.Diagnostics;
        // Marker survives a host crash. On the next boot LoadIidStore() moves
        // this IID to the blacklist, so a bad guess can never crash-loop.
        SetAttempt(candidates[i].iid);
        hr=GuardedTryOpen(facet.Get(),&session);
        if(SUCCEEDED(hr)) {
            SetVerified(candidates[i].iid);
            ClearAttempt();
            session.Diagnostics.IidIndex=(UCHAR)(STADIA_IID_SOURCE_AUTO+candidates[i].order);
            return S_OK;
        }
        ClearAttempt();
        AddBlacklist(candidates[i].iid);
        session.Diagnostics=saved; // keep candidate count, drop failed stage
        last=hr;
    }
    session.Diagnostics.Stage=count?STADIA_GATT_STAGE_AUTO_FAILED:STADIA_GATT_STAGE_IID_UNVERIFIED;
    return last;
#else
    (void)cacheUsed;(void)store;
    Candidate ignored[1]={};size_t count=0;ULONG plausible=0;
    HRESULT discovery=CollectCandidates(created,reference,ignored,1,&count,&plausible);
    session.Diagnostics.CandidateCount=StadiaEncodeCandidates(plausible,store.crashDetected,0);
    session.Diagnostics.Stage=STADIA_GATT_STAGE_IID_UNVERIFIED;
    return FAILED(discovery)?discovery:HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
#endif
}

static HRESULT Open(UINT64 address,Session& session) {
    ComPtr<IInspectable> created;void* reference=nullptr;
    HRESULT hr=CreateDeviceObject(address,created.GetAddressOf(),&reference,session);
    if(FAILED(hr)) return hr;

    session.Diagnostics.Stage=STADIA_GATT_STAGE_DEVICE_QUERY;
    for(size_t i=0;i<DEVICE_IID_COUNT;i++) {
        ComPtr<IInspectable> facet;
        hr=created->QueryInterface(DeviceIids[i],(void**)facet.GetAddressOf());
        if(SUCCEEDED(hr) && facet) {
            session.Diagnostics.IidIndex=(ULONG)(i+1);
            // Verified path: no SEH, no registry, no policy changes at all.
            return TryOpenWithDevice(facet.Get(),session);
        }
        // E_NOINTERFACE is the expected "this IID belongs to another build"
        // result. Any other failure must not be masked by the next IID.
        if(hr!=E_NOINTERFACE) return hr;
    }
    return AutoAdapt(created.Get(),reference,session);
}
extern "C" HRESULT StadiaPrivateGattOpen(UINT64 address,void** output,STADIA_GATT_DIAGNOSTICS* diagnostics) {
    if(!output || !diagnostics || !address) return E_INVALIDARG;
    *output=nullptr;*diagnostics={};
    diagnostics->Stage=STADIA_GATT_STAGE_START;
    Apartment apartment;
    HRESULT hr=apartment.Check();if(FAILED(hr)) {diagnostics->OpenStatus=hr;return hr;}
    std::unique_ptr<Session> session(new(std::nothrow) Session);if(!session) return E_OUTOFMEMORY;
    session->Diagnostics.Stage=STADIA_GATT_STAGE_START;
    hr=Open(address,*session);session->Diagnostics.OpenStatus=hr;*diagnostics=session->Diagnostics;
    if(SUCCEEDED(hr)) *output=session.release();return hr;
}
static HRESULT Write(Session& session,const UCHAR* report,size_t length) {
    if(!report || length!=5 || report[0]!=session.Diagnostics.ReportId) return E_INVALIDARG;
    ComPtr<IInspectable> characteristic=session.Characteristic; HRESULT hr=S_OK;
    ComPtr<IBufferFactory> factory;
    hr=RoGetActivationFactory(HStringReference(L"Windows.Storage.Streams.Buffer").Get(),IID_PPV_ARGS(&factory));if(FAILED(hr)) return hr;
    ComPtr<IBuffer> buffer;hr=factory->Create(4,&buffer);if(FAILED(hr)) return hr;
    hr=buffer->put_Length(4);if(FAILED(hr)) return hr;
    ComPtr<IBufferByteAccess> access;hr=buffer.As(&access);if(FAILED(hr)) return hr;
    BYTE* bytes=nullptr;hr=access->Buffer(&bytes);if(FAILED(hr) || !bytes) return FAILED(hr)?hr:E_UNEXPECTED;
    // Report ID belongs to the Report Reference descriptor, not the GATT value.
    CopyMemory(bytes,report+1,4);
    ComPtr<IInspectable> operation;
    // GattWriteOption::WriteWithResponse (1); no write-without-response path.
    hr=Slot<HRESULT (STDMETHODCALLTYPE *)(IInspectable*,UINT32,UINT32,IBuffer*,IInspectable**)>(characteristic.Get(),8)
        (characteristic.Get(),1,0,buffer.Get(),&operation);
    if(FAILED(hr)) return hr;
    ComPtr<IInspectable> result;hr=Await(operation.Get(),&result,&session.Diagnostics.AsyncStage);if(FAILED(hr)) return hr;
    return CheckCommunication(result.Get());
}
extern "C" HRESULT StadiaPrivateGattWrite(void* opaque,const UCHAR* report,size_t length,STADIA_GATT_DIAGNOSTICS* diagnostics) {
    if(!opaque || !diagnostics) return E_INVALIDARG;
    Session& session=*static_cast<Session*>(opaque);Apartment apartment;
    HRESULT hr=apartment.Check();if(SUCCEEDED(hr)) hr=Write(session,report,length);
    session.Diagnostics.WriteStatus=hr;
    if(SUCCEEDED(hr)) {session.Diagnostics.WriteCount++;if(report[1]==0 && report[2]==0 && report[3]==0 && report[4]==0) session.Diagnostics.StopCount++;}
    *diagnostics=session.Diagnostics;return hr;
}
extern "C" void StadiaPrivateGattClose(void* session) {
    Apartment apartment;delete static_cast<Session*>(session);
}
