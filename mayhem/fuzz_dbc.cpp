// Fuzz SavvyCAN's signal-definition importers through DBCHandler::loadFileAs(path, filterIndex), the
// public, dialog-free entry point (0 = DBC, 1 = Tesla JSON, 2 = "secret" CSV). FUZZ_FILTERS picks the
// formats per target; with more than one, the first input byte selects it. After a successful load,
// every signal of every message is decoded from a frame whose payload is the first 64 input bytes, so
// the signal decoder (bit positions/lengths/multiplexing read from the file) runs on the parsed result.
// The loaded file is removed again before returning, so no state carries over to the next input.
#include "fuzz_common.h"
#include "dbchandler.h"

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
    QString path = savvyfuzz::writeInput(data, size, filter == 1 ? "input.json" : (filter == 2 ? "input.csv" : "input.dbc"));
    DBCHandler *h = DBCHandler::getReference();
    DBCFile *file = h->loadFileAs(path, filter);
    if (file) {
        QByteArray payload(reinterpret_cast<const char *>(data), (int)qMin<size_t>(size, 64));
        const QList<DBC_MESSAGE *> msgs = file->messageHandler->getMsgsAsList();
        for (DBC_MESSAGE *m : msgs) {
            CommFrame frame(m->ID & 0x1FFFFFFF, payload);
            const QList<DBC_SIGNAL *> sigs = m->sigHandler->getSignalsAsList();
            for (DBC_SIGNAL *s : sigs) {
                QString out;
                s->processAsText(frame, out);
            }
        }
    }
    h->removeAllFiles();
    return 0;
}
