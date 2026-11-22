#include "../src/serverurl.h"

#include <QCoreApplication>
#include <QDebug>
#include <QString>

static int failures = 0;

static void check(const QString& input, const QString& expected) {
  const QString actual = NormalizeServerUrl(input);
  if (actual != expected) {
    qCritical().noquote() << "FAIL:" << input << "->" << actual
                          << "(expected" << expected << ")";
    ++failures;
  }
}

static void checkInsecure(const QString& input, bool expected) {
  const bool actual = IsInsecureHttpUrl(input);
  if (actual != expected) {
    qCritical().noquote() << "FAIL insecure:" << input << "->" << actual
                          << "(expected" << expected << ")";
    ++failures;
  }
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);

  // LAN: HTTP and port 8096 handled by the caller.
  check(QStringLiteral("192.168.1.199"), QStringLiteral("http://192.168.1.199"));
  check(QStringLiteral("192.168.1.199:8096"),
        QStringLiteral("http://192.168.1.199:8096"));
  check(QStringLiteral("10.0.0.5"), QStringLiteral("http://10.0.0.5"));
  check(QStringLiteral("172.16.0.1"), QStringLiteral("http://172.16.0.1"));
  check(QStringLiteral("localhost:8096"), QStringLiteral("http://localhost:8096"));
  check(QStringLiteral("jellyfin.local"), QStringLiteral("http://jellyfin.local"));
  check(QStringLiteral("jellyfin"), QStringLiteral("http://jellyfin"));

  // Public hosts: HTTPS by default.
  check(QStringLiteral("jellyfin.example.com"),
        QStringLiteral("https://jellyfin.example.com"));
  check(QStringLiteral("jellyfin.example.com:8443"),
        QStringLiteral("https://jellyfin.example.com:8443"));
  check(QStringLiteral("172.32.0.1"), QStringLiteral("https://172.32.0.1"));
  check(QStringLiteral("8.8.8.8"), QStringLiteral("https://8.8.8.8"));
  check(QStringLiteral("[2001:db8::1]"),
        QStringLiteral("https://[2001:db8::1]"));
  check(QStringLiteral("[fc00::1]"), QStringLiteral("http://[fc00::1]"));
  check(QStringLiteral("[::1]"), QStringLiteral("http://[::1]"));

  // Explicit scheme: must not be changed.
  check(QStringLiteral("https://jellyfin.example.com/"),
        QStringLiteral("https://jellyfin.example.com"));
  check(QStringLiteral("http://192.168.1.10:8096/"),
        QStringLiteral("http://192.168.1.10:8096"));

  check(QStringLiteral("  https://jellyfin.example.com  "),
        QStringLiteral("https://jellyfin.example.com"));
  check(QString(), QString());

  // Plain HTTP warning: only for hosts reachable over the network, not loopback.
  checkInsecure(QStringLiteral("http://192.168.1.10:8096"), true);
  checkInsecure(QStringLiteral("http://jellyfin.example.com"), true);
  checkInsecure(QStringLiteral("http://jellyfin.local:8096"), true);
  checkInsecure(QStringLiteral("http://localhost:8096"), false);
  checkInsecure(QStringLiteral("http://127.0.0.1:8096"), false);
  checkInsecure(QStringLiteral("http://[::1]:8096"), false);
  checkInsecure(QStringLiteral("https://192.168.1.10:8920"), false);
  checkInsecure(QString(), false);

  if (failures == 0) qInfo("server URL tests: ok");
  return failures == 0 ? 0 : 1;
}
