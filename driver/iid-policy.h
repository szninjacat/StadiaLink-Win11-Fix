#pragma once
// Pure policy helpers shared by the driver and its unit tests. No WinRT, no
// registry, no I/O: classifying an IID must be testable in a normal process.
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// IID sources encoded into diagnostics byte 61 (IidIndex).
#define STADIA_IID_SOURCE_KNOWN   0x01 // 1..2: hard-coded, hardware-verified
#define STADIA_IID_SOURCE_CACHE   0x40 // validated on an earlier boot, cached
#define STADIA_IID_SOURCE_AUTO    0x80 // 0x80+i: validated during this session

// Flags OR'ed into diagnostics byte 62 (CandidateCount).
#define STADIA_CANDIDATE_FLAG_CRASH 0x80 // a previous attempt crashed the host
#define STADIA_CANDIDATE_FLAG_CACHE 0x40 // a cached IID was tried
#define STADIA_CANDIDATE_COUNT_MASK 0x3F

// Hard-coded values in driver/private-gatt.cpp, kept here so the policy can
// exclude them and tests can assert the classification.
static const GUID STADIA_IID_STATICS = { 0xcbaa1a5f, 0xdcef, 0x508b, { 0xac, 0xc6, 0x15, 0x8f, 0x1e, 0x16, 0x0c, 0x0f } };
static const GUID STADIA_IID_DEVICE_26200 = { 0x6bbede1f, 0x797f, 0x5c17, { 0x8a, 0xc7, 0x3f, 0x34, 0x20, 0x37, 0xcb, 0x59 } };
static const GUID STADIA_IID_DEVICE_22631 = { 0x0799a6a6, 0xe942, 0x5218, { 0x8a, 0x0e, 0x7c, 0x9b, 0x99, 0x6e, 0x41, 0x58 } };

// WinRT plumbing interfaces that GetIids() commonly reports. None of them is
// the GattClientDevice device facet.
static const GUID STADIA_IID_IUNKNOWN = { 0x00000000, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
static const GUID STADIA_IID_IINSPECTABLE = { 0xaf86e2e0, 0xb12d, 0x4c6a, { 0x9c, 0x5a, 0xd7, 0xaa, 0x65, 0x10, 0x1e, 0x90 } };
static const GUID STADIA_IID_IAGILEOBJECT = { 0x94ea2b94, 0xe9cc, 0x49e0, { 0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90 } };
static const GUID STADIA_IID_ISTRINGABLE = { 0x96369f54, 0x8eb6, 0x48f0, { 0xab, 0xce, 0xc1, 0xb2, 0x11, 0xe6, 0x27, 0xc3 } };
static const GUID STADIA_IID_ICLOSABLE = { 0x30d5a829, 0x7fa4, 0x4026, { 0x83, 0xbb, 0xd7, 0x5b, 0xae, 0x4f, 0xd9, 0x9e } };
static const GUID STADIA_IID_IWEAKREFERENCE = { 0x00000037, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
static const GUID STADIA_IID_IACTIVATIONFACTORY = { 0x00000035, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

static inline int StadiaIidIsKnown(const GUID* g) {
    if (!g) return 0;
    if (IsEqualGUID(*g, STADIA_IID_STATICS)) return 1;
    if (IsEqualGUID(*g, STADIA_IID_DEVICE_26200)) return 1;
    if (IsEqualGUID(*g, STADIA_IID_DEVICE_22631)) return 1;
    if (IsEqualGUID(*g, STADIA_IID_IUNKNOWN)) return 1;
    if (IsEqualGUID(*g, STADIA_IID_IINSPECTABLE)) return 1;
    if (IsEqualGUID(*g, STADIA_IID_IAGILEOBJECT)) return 1;
    if (IsEqualGUID(*g, STADIA_IID_ISTRINGABLE)) return 1;
    if (IsEqualGUID(*g, STADIA_IID_ICLOSABLE)) return 1;
    if (IsEqualGUID(*g, STADIA_IID_IWEAKREFERENCE)) return 1;
    if (IsEqualGUID(*g, STADIA_IID_IACTIVATIONFACTORY)) return 1;
    return 0;
}

// A WinRT parameterized interface IID is a version-5 UUID (Data3 = 5xxx) and
// carries the RFC 4122 variant bits. This is necessary but NOT sufficient:
// the driver must still validate the vtable and the whole GATT chain.
static inline int StadiaIidIsPlausible(const GUID* g) {
    if (!g) return 0;
    if ((g->Data3 & 0xF000) != 0x5000) return 0;
    if ((g->Data4[0] & 0xC0) != 0x80) return 0;
    if (StadiaIidIsKnown(g)) return 0;
    return 1;
}

static inline int StadiaIidListContains(const GUID* list, size_t count, const GUID* g) {
    if (!list || !g) return 0;
    for (size_t i = 0; i < count; i++) if (IsEqualGUID(list[i], *g)) return 1;
    return 0;
}

// Append without duplicates, keeping the newest entries when full.
static inline size_t StadiaIidListAdd(GUID* list, size_t count, size_t cap, const GUID* g) {
    if (!list || !g || cap == 0) return count;
    if (StadiaIidListContains(list, count, g)) return count;
    if (count < cap) { list[count] = *g; return count + 1; }
    for (size_t i = 1; i < cap; i++) list[i - 1] = list[i];
    list[cap - 1] = *g;
    return cap;
}

static inline unsigned char StadiaEncodeCandidates(unsigned long plausible, int crashed, int cacheUsed) {
    // Saturate instead of wrapping: a wrapped count would look like a small
    // number of candidates to whoever reads the status report.
    unsigned char value = plausible > STADIA_CANDIDATE_COUNT_MASK
        ? (unsigned char)STADIA_CANDIDATE_COUNT_MASK : (unsigned char)plausible;
    if (crashed) value |= STADIA_CANDIDATE_FLAG_CRASH;
    if (cacheUsed) value |= STADIA_CANDIDATE_FLAG_CACHE;
    return value;
}

#ifdef __cplusplus
}
#endif
