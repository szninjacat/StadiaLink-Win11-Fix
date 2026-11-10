// Unit tests for the pure IID-selection policy shared with the driver.
// Builds and runs as an ordinary user-mode process; no WinRT, no hardware.
#include "../driver/iid-policy.h"
#include <stdio.h>

static int failures = 0;
static int checks = 0;

static void expect(int condition, const char* label) {
    checks++;
    if (!condition) { failures++; printf("  FAIL  %s\n", label); }
    else printf("  ok    %s\n", label);
}

static GUID synthetic(unsigned short data3, unsigned char variant) {
    GUID g = { 0x11223344, 0x5566, data3, { variant, 1, 2, 3, 4, 5, 6, 7 } };
    return g;
}

int main() {
    printf("IID policy tests\n");

    // Hardware-verified IIDs and the shared statics IID must never be treated
    // as fresh candidates: they are handled before auto-adaptation runs.
    expect(StadiaIidIsKnown(&STADIA_IID_DEVICE_22631), "22631 device IID is known");
    expect(StadiaIidIsKnown(&STADIA_IID_DEVICE_26200), "26200 device IID is known");
    expect(StadiaIidIsKnown(&STADIA_IID_STATICS), "statics IID is known");
    expect(!StadiaIidIsPlausible(&STADIA_IID_DEVICE_22631), "verified IID is excluded from candidates");
    expect(!StadiaIidIsPlausible(&STADIA_IID_STATICS), "statics IID is excluded from candidates");

    // WinRT plumbing interfaces.
    expect(StadiaIidIsKnown(&STADIA_IID_IUNKNOWN), "IUnknown is known");
    expect(StadiaIidIsKnown(&STADIA_IID_IINSPECTABLE), "IInspectable is known");
    expect(StadiaIidIsKnown(&STADIA_IID_IAGILEOBJECT), "IAgileObject is known");
    expect(StadiaIidIsKnown(&STADIA_IID_ICLOSABLE), "IClosable is known");

    // A version-5 parameterized interface with a valid RFC 4122 variant.
    GUID candidate = synthetic(0x5218, 0x8a);
    expect(StadiaIidIsPlausible(&candidate), "0x5xxx + valid variant is plausible");
    expect(!StadiaIidIsKnown(&candidate), "synthetic candidate is not known");

    GUID nonParameterized = synthetic(0x4218, 0x8a);
    expect(!StadiaIidIsPlausible(&nonParameterized), "non-0x5xxx IID is rejected");
    GUID badVariant = synthetic(0x5abc, 0x3a);
    expect(!StadiaIidIsPlausible(&badVariant), "invalid GUID variant is rejected");
    GUID otherVersion = synthetic(0x6123, 0x8a);
    expect(!StadiaIidIsPlausible(&otherVersion), "non-v5 UUID is rejected");
    expect(!StadiaIidIsPlausible(nullptr), "null IID is rejected");

    // Simulated GetIids() result: count only plausible, non-blacklisted ones.
    GUID reported[] = {
        STADIA_IID_IUNKNOWN, STADIA_IID_IINSPECTABLE, STADIA_IID_STATICS,
        STADIA_IID_DEVICE_22631,
        synthetic(0x5a01, 0x8a), synthetic(0x5b02, 0x9f),
        synthetic(0x4c03, 0x80), synthetic(0x5c04, 0x40),
    };
    ULONG plausible = 0;
    for (size_t i = 0; i < sizeof(reported) / sizeof(reported[0]); i++)
        if (StadiaIidIsPlausible(&reported[i])) plausible++;
    expect(plausible == 2, "mixed GetIids result yields exactly 2 plausible candidates");

    // Blacklist: no duplicates, newest kept when full.
    GUID list[4] = {};
    size_t count = 0;
    GUID a = synthetic(0x5a01, 0x8a), b = synthetic(0x5b02, 0x8a);
    GUID c = synthetic(0x5c03, 0x8a), d = synthetic(0x5d04, 0x8a);
    GUID e = synthetic(0x5e05, 0x8a);
    count = StadiaIidListAdd(list, count, 4, &a);
    count = StadiaIidListAdd(list, count, 4, &a);
    expect(count == 1, "duplicate IID is not added twice");
    count = StadiaIidListAdd(list, count, 4, &b);
    count = StadiaIidListAdd(list, count, 4, &c);
    count = StadiaIidListAdd(list, count, 4, &d);
    count = StadiaIidListAdd(list, count, 4, &e);
    expect(count == 4, "blacklist stays within capacity");
    expect(!StadiaIidListContains(list, count, &a), "oldest entry is evicted");
    expect(StadiaIidListContains(list, count, &e), "newest entry is retained");
    expect(!StadiaIidListContains(list, count, nullptr), "null lookup is safe");
    expect(StadiaIidListAdd(list, count, 0, &a) == count, "zero-capacity list is safe");

    // Diagnostics byte encoding.
    expect(StadiaEncodeCandidates(3, 0, 0) == 0x03, "candidate count encodes in low 6 bits");
    expect(StadiaEncodeCandidates(2, 1, 1) == 0xC2, "crash + cache flags are preserved");
    expect((StadiaEncodeCandidates(99, 0, 0) & STADIA_CANDIDATE_COUNT_MASK) == 0x3F,
           "candidate count saturates without touching flags");
    expect(STADIA_IID_SOURCE_AUTO + 0x1F <= 0x9F, "auto IID index stays in reserved range");

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
