#include "language.h"

#include <QCoreApplication>
#include <QDebug>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QSettings>
#include <QVariantMap>

namespace {

struct LanguageEntry {
  const char* code;
  const char* name;
};

// Nome della lingua nella lingua stessa: cosi' il selettore resta leggibile
// anche quando l'interfaccia e' in una lingua che non conosci.
const LanguageEntry kLanguages[] = {
    {"it", "Italiano"},
    {"en", "English"},
    {"es", "Español"},
    {"fr", "Français"},
    {"de", "Deutsch"},
    {"pt-BR", "Português (Brasil)"},
    {"ru", "Русский"},
    {"nl", "Nederlands"},
    {"pl", "Polski"},
    {"pt-PT", "Português (Portugal)"},
    {"sv", "Svenska"},
    {"da", "Dansk"},
    {"nb", "Norsk bokmål"},
    {"cs", "Čeština"},
    {"uk", "Українська"},
    {"ca", "Català"},
    {"tr", "Türkçe"},
    {"ro", "Română"},
    {"el", "Ελληνικά"},
    {"hu", "Magyar"},
    {"fi", "Suomi"},
};

const LanguageEntry* FindLanguage(const QString& code) {
  for (const LanguageEntry& entry : kLanguages) {
    if (code == QLatin1String(entry.code)) return &entry;
  }
  return nullptr;
}

// Alla prima apertura segue la lingua del sistema, se disponibile.
QString DetectSystemLanguage() {
  const QStringList uiLanguages = QLocale::system().uiLanguages();
  for (QString tag : uiLanguages) {
    tag.replace(QLatin1Char('_'), QLatin1Char('-'));
    if (const LanguageEntry* exact = FindLanguage(tag)) {
      return QString::fromLatin1(exact->code);
    }
    const QString base = tag.section(QLatin1Char('-'), 0, 0);
    if (base == QLatin1String("no")) return QStringLiteral("nb");
    if (base == QLatin1String("pt")) return QStringLiteral("pt-BR");
    for (const LanguageEntry& entry : kLanguages) {
      const QString entryBase =
          QString::fromLatin1(entry.code).section(QLatin1Char('-'), 0, 0);
      if (base.compare(entryBase, Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(entry.code);
      }
    }
  }
  return QStringLiteral("en");
}

}  // namespace

Language::Language(QQmlApplicationEngine* engine, QObject* parent)
    : QObject(parent), engine_(engine) {
  QSettings settings;
  const QString saved = settings.value(QStringLiteral("language")).toString();
  if (FindLanguage(saved) != nullptr) {
    current_ = saved;
  } else {
    current_ = DetectSystemLanguage();
  }
  apply();
}

QVariantList Language::available() const {
  QVariantList list;
  for (const LanguageEntry& entry : kLanguages) {
    QVariantMap item;
    item.insert(QStringLiteral("code"), QString::fromLatin1(entry.code));
    item.insert(QStringLiteral("name"), QString::fromUtf8(entry.name));
    list.append(item);
  }
  return list;
}

void Language::setCurrent(const QString& code) {
  if (code == current_) return;
  if (FindLanguage(code) == nullptr) return;

  current_ = code;
  QSettings().setValue(QStringLiteral("language"), current_);
  apply();
  emit currentChanged();
}

void Language::apply() {
  QCoreApplication::removeTranslator(&translator_);

  if (current_ != QLatin1String("it")) {
    const QString path =
        QStringLiteral(":/i18n/jellylink_%1.qm").arg(current_);
    if (translator_.load(path)) {
      QCoreApplication::installTranslator(&translator_);
    } else {
      qWarning("[i18n] traduzione non caricata: %s", qPrintable(path));
    }
  }

  if (engine_ != nullptr && !engine_->rootObjects().isEmpty()) {
    engine_->retranslate();
  }

  qInfo("[i18n] lingua %s (%s)", qPrintable(current_),
        current_ == QLatin1String("it")
            ? "sorgente"
            : (translator_.isEmpty() ? "traduzione mancante"
                                     : "traduzione caricata"));
}
