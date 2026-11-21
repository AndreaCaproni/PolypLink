#include "../src/redact.h"

#include <QCoreApplication>
#include <QDebug>
#include <QString>

static int failures = 0;

static void check(const QString& input, const QString& expected) {
  const QString actual = RedactSecrets(input);
  if (actual != expected) {
    qCritical().noquote() << "FAIL:" << input << "->" << actual
                          << "(expected" << expected << ")";
    ++failures;
  }
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);

  check(QStringLiteral("http://server/videos/a?ApiKey=secret123&MediaSourceId=x"),
        QStringLiteral("http://server/videos/a?ApiKey=***&MediaSourceId=x"));
  check(QStringLiteral("http://server/a?api_key=abc&x=1"),
        QStringLiteral("http://server/a?api_key=***&x=1"));
  check(QStringLiteral("http://server/a?token=abc&x=1"),
        QStringLiteral("http://server/a?token=***&x=1"));
  check(QStringLiteral("http://server/a?access_token=abc&x=1"),
        QStringLiteral("http://server/a?access_token=***&x=1"));
  check(QStringLiteral("Authorization: Bearer abc123"),
        QStringLiteral("Authorization: ***"));
  check(QStringLiteral("MediaBrowser Client=\"PolypLink\", Token=\"abc123\""),
        QStringLiteral("MediaBrowser Client=\"PolypLink\", Token=\"***\""));
  check(QStringLiteral("X-Emby-Token: abc123"),
        QStringLiteral("X-Emby-Token: ***"));
  check(QStringLiteral("http://server/QuickConnect/Connect?secret=abc123"),
        QStringLiteral("http://server/QuickConnect/Connect?secret=***"));
  check(QStringLiteral("QML Image: Error decoding: http://server/Items/x/Images/"
                       "Primary?api_key=secret123: Connection refused"),
        QStringLiteral("QML Image: Error decoding: http://server/Items/x/Images/"
                       "Primary?api_key=***: Connection refused"));
  check(QStringLiteral("http://user:pass@server/a?x=1"),
        QStringLiteral("http://user:***@server/a?x=1"));
  check(QStringLiteral("http://server/a?x=1&y=2"),
        QStringLiteral("http://server/a?x=1&y=2"));

  if (failures == 0) qInfo("redaction tests: ok");
  return failures == 0 ? 0 : 1;
}
