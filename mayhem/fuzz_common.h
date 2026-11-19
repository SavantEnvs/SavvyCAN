// Shared setup for the SavvyCAN fuzz harnesses.
//
// SavvyCAN's loaders are QWidget-era code: they take a file NAME, call qApp->processEvents() in their
// loops and pop a modal QMessageBox when a file cannot be parsed. So the harness
//   * creates one QApplication on the offscreen platform (no display needed),
//   * writes each input to one scratch file in a private mkdtemp() dir under $TMPDIR (overwritten per
//     input, so every input's result depends only on that input),
//   * closes any modal dialog the code opens: an event filter posts a queued reject() when a QDialog is
//     shown, so QDialog::exec() returns as soon as its event loop starts (no timer involved),
//   * drops qDebug/qWarning output (the loaders log every line) but still aborts on qFatal.
// Kept across inputs on purpose: the QApplication, the scratch path and a throwaway QSettings location.
#pragma once
#include <QApplication>
#include <QByteArray>
#include <QDialog>
#include <QEvent>
#include <QFile>
#include <QMetaObject>
#include <QString>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

namespace savvyfuzz {

inline void quietHandler(QtMsgType type, const QMessageLogContext &, const QString &msg) {
    if (type == QtFatalMsg) {
        fprintf(stderr, "qFatal: %s\n", msg.toLocal8Bit().constData());
        abort();
    }
}

class DialogCloser : public QObject {
public:
    bool eventFilter(QObject *obj, QEvent *ev) override {
        if (ev->type() == QEvent::Show) {
            if (QDialog *d = qobject_cast<QDialog *>(obj))
                QMetaObject::invokeMethod(d, "reject", Qt::QueuedConnection);
        }
        return false;
    }
};

inline std::string &scratchDir() { static std::string d; return d; }

inline void init(int *argc, char ***argv) {
    const char *tmp = getenv("TMPDIR");
    std::string tmpl = std::string(tmp && *tmp ? tmp : "/tmp") + "/savvyfuzz-XXXXXX";
    if (!mkdtemp(&tmpl[0])) { perror("mkdtemp"); abort(); }
    scratchDir() = tmpl;
    // QSettings (used by some loaders) must never touch the user's real config.
    setenv("XDG_CONFIG_HOME", tmpl.c_str(), 1);
    setenv("HOME", tmpl.c_str(), 1);
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qInstallMessageHandler(quietHandler);
    static int qargc = 1;
    static char name[] = "savvycan-fuzz";
    static char *qargv[] = {name, nullptr};
    (void)argc; (void)argv;
    static QApplication app(qargc, qargv);
    QCoreApplication::setOrganizationName("SavvyCANFuzz");
    QCoreApplication::setApplicationName("SavvyCANFuzz");
    app.installEventFilter(new DialogCloser);
}

// Write the bytes to the scratch file (same name every time) and return its path.
inline QString writeInput(const uint8_t *data, size_t size, const char *name) {
    QString path = QString::fromStdString(scratchDir()) + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) abort();
    if (size) f.write(reinterpret_cast<const char *>(data), (qint64)size);
    f.close();
    return path;
}

} // namespace savvyfuzz
