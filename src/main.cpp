#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QLockFile>
#include <QScreen>
#include <QSurfaceFormat>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QDebug>

#include <cstdio>

#include "appcontroller.h"
#include "buildstamp.h"
#include "inputpump.h"
#include "language.h"
#include "redact.h"
#include "version.h"

// ---------------------------------------------------------------------------
// Log: stderr (che il launcher manda in /tmp) + un file sulla chiavetta USB.
// Cosi' si puo' leggere il log anche senza SSH: basta rimettere la chiavetta
// nel PC. Il file finisce nella radice della USB, accanto a "steamlink".
// ---------------------------------------------------------------------------

namespace {

QFile g_usbLog;

QString FindUsbLogPath() {
  const QStringList roots = {QStringLiteral("/media"), QStringLiteral("/run/media"),
                             QStringLiteral("/mnt"), QStringLiteral("/var/media"),
                             QStringLiteral("/var/run/media")};
  for (const QString& root : roots) {
    QDir r(root);
    const QStringList level1 =
        r.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& a : level1) {
      const QString base1 = root + QLatin1Char('/') + a;
      if (QFileInfo::exists(base1 + QStringLiteral("/steamlink")))
        return base1 + QStringLiteral("/polyplink.log");
      QDir d1(base1);
      const QStringList level2 =
          d1.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
      for (const QString& b : level2) {
        const QString base2 = base1 + QLatin1Char('/') + b;
        if (QFileInfo::exists(base2 + QStringLiteral("/steamlink")))
          return base2 + QStringLiteral("/polyplink.log");
      }
    }
  }
  return QString();
}

void MessageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg) {
  const char* level = "info";
  switch (type) {
    case QtDebugMsg: level = "debug"; break;
    case QtInfoMsg: level = "info"; break;
    case QtWarningMsg: level = "warn"; break;
    case QtCriticalMsg: level = "crit"; break;
    case QtFatalMsg: level = "fatal"; break;
  }
  // Redige qui, non solo nei singoli qWarning: cosi' copriamo anche i
  // messaggi generati da Qt/QML (per esempio gli URL delle immagini che
  // contengono api_key) e ogni log futuro.
  const QString line = QStringLiteral("%1 [%2] %3\n")
                           .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
                           .arg(QLatin1String(level))
                           .arg(RedactSecrets(msg));
  fputs(line.toLocal8Bit().constData(), stderr);
  fflush(stderr);
  if (g_usbLog.isOpen()) {
    g_usbLog.write(line.toUtf8());
    g_usbLog.flush();
  }
  if (type == QtFatalMsg) abort();
}

void SetupLogging() {
  const QString path = FindUsbLogPath();
  if (!path.isEmpty()) {
    g_usbLog.setFileName(path);
    if (g_usbLog.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      g_usbLog.write("polyplink — log su chiavetta\n");
    }
  }
  qInstallMessageHandler(MessageHandler);
}

}  // namespace

// Conta i frame realmente presentati: e' la metrica che conta su questo hardware.
class FpsCounter : public QObject {
  Q_OBJECT
  Q_PROPERTY(double fps READ fps NOTIFY fpsChanged)
  Q_PROPERTY(double worst READ worst NOTIFY fpsChanged)

 public:
  explicit FpsCounter(QObject* parent = nullptr) : QObject(parent) { timer_.start(); }

  double fps() const { return fps_; }
  double worst() const { return worst_; }

 public slots:
  void onFrameSwapped() {
    if (!warm_) {
      // Il primo secondo include l'avvio del motore grafico: non e' un dato utile
      warm_ = true;
      frames_ = 0;
      timer_.restart();
      return;
    }
    ++frames_;
    const qint64 ms = timer_.elapsed();
    if (ms < 1000) return;
    fps_ = frames_ * 1000.0 / static_cast<double>(ms);
    if (worst_ <= 0.0 || fps_ < worst_) worst_ = fps_;
    frames_ = 0;
    timer_.restart();
    qInfo("[fps] %.1f (minimo %.1f)", fps_, worst_);
    emit fpsChanged();
  }

 signals:
  void fpsChanged();

 private:
  QElapsedTimer timer_;
  int frames_ = 0;
  double fps_ = 0.0;
  double worst_ = 0.0;
  bool warm_ = false;
};

int main(int argc, char** argv) {
  // Una sola istanza: due processi Polyplink leggerebbero lo stesso controller
  // e si contenderebbero EGLFS, causando input duplicati/randomici.
  QLockFile instanceLock(QStringLiteral("/tmp/polyplink.lock"));
  instanceLock.setStaleLockTime(0);
  if (!instanceLock.tryLock(0)) {
    std::fprintf(stderr, "[avvio] Polyplink e' gia' in esecuzione; esco.\n");
    return 0;
  }

  SetupLogging();

  // Prima di tutto il resto: se nel log non c'e' questa riga, il binario non e'
  // nemmeno partito (problema di launcher o di librerie, non di QML).
  qInfo("[avvio] %s %s — build %s — Qt %s", APP_NAME, APP_VERSION, BUILD_STAMP,
        qVersion());
  qInfo("[avvio] log su USB: %s",
        g_usbLog.isOpen() ? qPrintable(g_usbLog.fileName()) : "(non disponibile)");

  // Il piano video e' un layer hardware sotto la finestra Qt: serve un
  // framebuffer con canale alpha perche' il video si veda "attraverso" la UI.
  QSurfaceFormat format;
  format.setAlphaBufferSize(8);
  format.setRenderableType(QSurfaceFormat::OpenGLES);
  QSurfaceFormat::setDefaultFormat(format);

  QGuiApplication app(argc, argv);
  // Se la finestra venisse nascosta (o chiusa) l'app non deve terminare:
  // il player puo' restare vivo anche senza UI.
  QGuiApplication::setQuitOnLastWindowClosed(false);
  app.setApplicationName(QStringLiteral("PolypLink"));
  app.setOrganizationName(QStringLiteral("polyplink"));

  QGuiApplication::setOverrideCursor(Qt::BlankCursor);
  qInfo("[avvio] piattaforma Qt: %s", qPrintable(QGuiApplication::platformName()));
  for (const QScreen* s : QGuiApplication::screens()) {
    qInfo("[avvio] schermo: %dx%d @ %.1f Hz (dpr %.2f)", s->geometry().width(),
          s->geometry().height(), s->refreshRate(), s->devicePixelRatio());
  }

  // Il dispositivo ha Noto Sans; se manca, Qt ripiega da solo.
  const QStringList families = QFontDatabase().families();
  if (families.contains(QStringLiteral("Noto Sans"))) {
    QFont f(QStringLiteral("Noto Sans"));
    f.setPixelSize(28);
    app.setFont(f);
  }
  qInfo("[avvio] font disponibili: %d", families.size());

  FpsCounter counter;

  QQmlApplicationEngine engine;
  Language language(&engine);
  AppController controller;

  QObject::connect(
      &engine, &QQmlApplicationEngine::warnings, &app,
      [](const QList<QQmlError>& warnings) {
        for (const QQmlError& e : warnings) qWarning("[qml] %s", qPrintable(e.toString()));
      },
      Qt::DirectConnection);

  engine.rootContext()->setContextProperty(QStringLiteral("fpsCounter"), &counter);
  engine.rootContext()->setContextProperty(QStringLiteral("language"), &language);
  engine.rootContext()->setContextProperty(QStringLiteral("app"), &controller);
  engine.rootContext()->setContextProperty(QStringLiteral("appVersion"),
                                           QStringLiteral(APP_VERSION));
  engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

  if (engine.rootObjects().isEmpty()) {
    qCritical("[errore] caricamento QML fallito (vedi i messaggi [qml] sopra)");
    return 1;
  }

  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
  if (window == nullptr) {
    qCritical("[errore] la radice QML non e' una Window");
    return 1;
  }

  // AutoConnection: il conteggio avviene sul thread di rendering, ma il segnale
  // deve arrivare alla UI sul thread principale. Con DirectConnection QML
  // verrebbe aggiornato dal thread sbagliato.
  QObject::connect(window, &QQuickWindow::frameSwapped, &counter, &FpsCounter::onFrameSwapped);

  auto* input = new InputPump(window, &app);
  if (!input->ok()) qWarning("[input] nessun controller disponibile");

  QObject::connect(input, &InputPump::keyPressed, window,
                   [window, &controller](int key, bool autoRepeat) {
    if (controller.playerActive()) {
      switch (key) {
        // OSD navigabile come nel client Jellyfin: la barra e i pulsanti
        // ricevono il focus, il seek parte alla conferma o a fine corsa.
        case Qt::Key_Return: controller.playerConfirm(); break;
        case Qt::Key_Left: controller.playerNavigate(-1, 0, autoRepeat); break;
        case Qt::Key_Right: controller.playerNavigate(1, 0, autoRepeat); break;
        case Qt::Key_Up: controller.playerNavigate(0, -1, autoRepeat); break;
        case Qt::Key_Down: controller.playerNavigate(0, 1, autoRepeat); break;
        case Qt::Key_Escape:
        case Qt::Key_Backspace: controller.playerBack(); break;
        case Qt::Key_Menu: controller.toggleStats(); break;
        default: break;
      }
      return;
    }
    QCoreApplication::postEvent(window, new QKeyEvent(QEvent::KeyPress, key, Qt::NoModifier));
    QCoreApplication::postEvent(window, new QKeyEvent(QEvent::KeyRelease, key, Qt::NoModifier));
  });

  qInfo("[avvio] finestra %dx%d, inizio", window->width(), window->height());
  return app.exec();
}

#include "main.moc"
