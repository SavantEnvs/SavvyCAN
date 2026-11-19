// Fuzz SavvyCAN's CAN log loaders through the public FrameFileIO::loadFrameFileAs(path, filterIndex,
// &frames) entry point — exactly what File > Load Log does once a format is chosen (index 0 is the
// auto-detect path that File > Open uses by default). FUZZ_FILTERS (set per target by CMake) is the
// list of filter indexes this binary covers; with more than one, the first input byte selects the
// loader and the rest is the file.
#include "fuzz_common.h"
#include "framefileio.h"

static const int kFilters[] = {FUZZ_FILTERS};
static const size_t kNumFilters = sizeof(kFilters) / sizeof(kFilters[0]);

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
    savvyfuzz::init(argc, argv);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    int filter = kFilters[0];
    if (kNumFilters > 1) {
        if (size < 1) return -1;
        filter = kFilters[data[0] % kNumFilters];
        data++; size--;
    }
    QString path = savvyfuzz::writeInput(data, size, "input.log");
    QVector<CommFrame> frames;
    FrameFileIO::loadFrameFileAs(path, filter, &frames);
    // Touch every decoded frame so a bogus payload length is caught here, not silently ignored.
    volatile unsigned sum = 0;
    for (const CommFrame &f : frames) {
        QByteArray p = f.payload();
        for (char c : p) sum += (unsigned char)c;
        sum += f.frameId();
    }
    (void)sum;
    return 0;
}
