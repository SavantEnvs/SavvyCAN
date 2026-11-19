// Fuzz SavvyCAN's ISO-TP reassembly and UDS decoding on a stream of CAN frames, the way a capture or
// a live bus feeds them: ISOTP_HANDLER::rapidFrames (public slot: single/first/consecutive/flow-control
// frames, normal or extended addressing) emits each reassembled message, which goes through
// UDS_HANDLER::tryISOtoUDS and UDS_HANDLER::getDetailedMessageAnalysis (the decoder behind the UDS
// views). Both handlers are created fresh for every input, so no reassembly state carries over.
//
// Input: byte 0 = options (bit0 extended addressing, bit1 flow control replies), then repeated
// frames of [len % 9][id hi][id lo][len data bytes]; bit7 of id hi marks a 29-bit id.
#include "fuzz_common.h"
#include "isotp_handler.h"
#include "uds_handler.h"

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
    savvyfuzz::init(argc, argv);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1) return -1;
    const uint8_t opts = data[0];
    size_t pos = 1;
    QVector<CommFrame> frames;
    while (pos + 3 <= size) {
        int len = data[pos] % 9;
        uint32_t id = ((uint32_t)(data[pos + 1] & 0x7F) << 8) | data[pos + 2];
        bool ext = data[pos + 1] & 0x80;
        if (ext) id |= 0x18DA0000u;
        pos += 3;
        int avail = (int)qMin<size_t>((size_t)len, size - pos);
        CommFrame f(id, QByteArray(reinterpret_cast<const char *>(data + pos), avail));
        f.setExtendedFrameFormat(ext);
        f.setReceived(true);
        f.setBus(0);
        frames.append(f);
        pos += avail;
    }

    QVector<CommFrame> modelFrames;
    auto noConn = [](ISOTP_HANDLER *) { return QMetaObject::Connection(); };
    auto *iso = new ISOTP_HANDLER(modelFrames, [](const CommFrame &) {}, []() { return 1; }, noConn);
    UDS_HANDLER uds(iso, []() { return 1; }, [](UDS_HANDLER *) { return QMetaObject::Connection(); });
    iso->setExtendedAddressing(opts & 1);
    iso->setFlowCtrl(opts & 2);
    iso->setProcessAll(true);
    iso->setReception(true);
    volatile int sink = 0;
    QObject::connect(iso, &ISOTP_HANDLER::newISOMessage, [&uds, &sink](const ISOTP_MESSAGE &msg) {
        bool ok = false;
        UDS_MESSAGE u = uds.tryISOtoUDS(msg, &ok);
        if (ok) sink += uds.getDetailedMessageAnalysis(u).size();
    });
    iso->rapidFrames(frames);
    (void)sink;
    return 0; // ~UDS_HANDLER deletes iso
}
