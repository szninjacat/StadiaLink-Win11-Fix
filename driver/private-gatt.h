#pragma once
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif

// Private GATT open stages. Reported through the status feature report so a
// mismatch between Windows builds can be diagnosed without attaching a
// debugger to the UMDF host.
enum {
    STADIA_GATT_STAGE_START           = 1,  // WinRT apartment
    STADIA_GATT_STAGE_ACTIVATION      = 2,  // RoGetActivationFactory
    STADIA_GATT_STAGE_CREATE_ASYNC    = 3,  // GattStatics::CreateAsync
    STADIA_GATT_STAGE_AWAIT_CREATE    = 4,  // async create / IAsyncInfo
    STADIA_GATT_STAGE_DEVICE_QUERY    = 5,  // QueryInterface on created device
    STADIA_GATT_STAGE_HID_SERVICE     = 6,  // HID service enumeration
    STADIA_GATT_STAGE_SERVICE_OBJECT  = 7,  // open service object
    STADIA_GATT_STAGE_DISCOVERY       = 8,  // characteristic discovery object
    STADIA_GATT_STAGE_CHARACTERISTICS = 9,  // enumerate characteristics
    STADIA_GATT_STAGE_REPORT_REF      = 10, // read Report Reference descriptor
    STADIA_GATT_STAGE_READY           = 0x7F
};

// IidIndex: 1..N = a known per-build IID, >=0x80 = discovered at runtime,
// 0 = not resolved yet.
typedef struct _STADIA_GATT_DIAGNOSTICS {
    HRESULT OpenStatus;
    HRESULT WriteStatus;
    ULONG WriteCount;
    ULONG StopCount;
    USHORT ValueHandle;
    UCHAR ReportId;
    UCHAR ReportType;
    ULONG Stage;
    ULONG IidIndex;
} STADIA_GATT_DIAGNOSTICS;

HRESULT StadiaPrivateGattOpen(UINT64 Address, void **Session, STADIA_GATT_DIAGNOSTICS *Diagnostics);
HRESULT StadiaPrivateGattWrite(void *Session, const UCHAR *Report, size_t Length, STADIA_GATT_DIAGNOSTICS *Diagnostics);
void StadiaPrivateGattClose(void *Session);
#ifdef __cplusplus
}
#endif
