// Fuzz SavvyCAN's log-notes readers: LogNotes::fromMarkdown (the `<log>.notes.md` sidecar a user gets
// alongside a shared log) and LogNotes::readFromNativeCSV (the notes block embedded in a native CSV
// log), both public static entry points in lognotes.cpp. The first input byte chooses which.
#include "fuzz_common.h"
#include "lognotes.h"

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
    savvyfuzz::init(argc, argv);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1) return -1;
    const bool csv = data[0] & 1;
    data++; size--;
    LogNotes notes;
    if (csv) {
        QString path = savvyfuzz::writeInput(data, size, "input.csv");
        LogNotes::readFromNativeCSV(path, notes);
    } else {
        LogNotes::fromMarkdown(QString::fromUtf8(reinterpret_cast<const char *>(data), (qsizetype)size), notes);
    }
    return 0;
}
