// Verify that the address advertised over UDP cannot point to a host other
// than the one that sent the datagram.
#include "discoveryaddress.h"

#include <cstdio>

namespace {

int g_failures = 0;

void expect(const char* advertised, const QHostAddress& sender,
            const char* expected) {
  const QString got = NormalizeDiscoveredAddress(QString::fromUtf8(advertised), sender);
  const QString want = QString::fromUtf8(expected);
  if (got != want) {
    std::fprintf(stderr, "FAIL: \"%s\" da %s -> \"%s\" (atteso \"%s\")\n",
                 advertised, qPrintable(sender.toString()), qPrintable(got),
                 expected);
    ++g_failures;
  }
}

}  // namespace

int main() {
  const QHostAddress sender(QStringLiteral("192.168.1.50"));

  expect("http://192.168.1.50:8096", sender, "http://192.168.1.50:8096");
  expect("http://192.168.1.50:8096/", sender, "http://192.168.1.50:8096");

  // Arbitrary host: replaced with the sender.
  expect("http://evil.example:8096", sender, "http://192.168.1.50:8096");
  expect("http://192.168.1.99:8096", sender, "http://192.168.1.50:8096");
  expect("https://jellyfin.local:8920/jellyfin/", sender,
         "https://192.168.1.50:8920/jellyfin");

  // Unacceptable responses.
  expect("http://user:pass@192.168.1.50:8096", sender, "");
  expect("ftp://192.168.1.50:8096", sender, "");
  expect("javascript:alert(1)", sender, "");
  expect("http://192.168.1.50:8096/?x=1", sender, "");
  expect("http://192.168.1.50:8096/#frag", sender, "");
  expect("", sender, "");
  expect("http://192.168.1.50:8096", QHostAddress(), "");
  expect("http://192.168.1.50:8096", QHostAddress(QHostAddress::AnyIPv4), "");
  expect("http://192.168.1.50:8096", QHostAddress(QStringLiteral("::ffff:192.168.1.50")),
         "http://192.168.1.50:8096");

  if (g_failures != 0) {
    std::fprintf(stderr, "discovery address tests: %d falliti\n", g_failures);
    return 1;
  }
  std::printf("discovery address tests: ok\n");
  return 0;
}
