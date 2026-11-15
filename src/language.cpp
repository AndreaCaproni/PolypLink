#include "language.h"

#include <QCoreApplication>
#include <QDebug>
#include <QQmlApplicationEngine>
#include <QSettings>
#include <QVariantMap>

Language::Language(QQmlApplicationEngine* engine, QObject* parent)
    : QObject(parent), engine_(engine) {
  QSettings settings;
  const QString saved = settings.value(QStringLiteral("language")).toString();
  current_ = (saved == QLatin1String("en")) ? QStringLiteral("en")
                                            : QStringLiteral("it");
  apply();
}

QVariantList Language::available() const {
  QVariantList list;

  QVariantMap italian;
  italian.insert(QStringLiteral("code"), QStringLiteral("it"));
  italian.insert(QStringLiteral("name"), QStringLiteral("Italiano"));
  list.append(italian);

  QVariantMap english;
  english.insert(QStringLiteral("code"), QStringLiteral("en"));
  english.insert(QStringLiteral("name"), QStringLiteral("English"));
  list.append(english);

  return list;
}

void Language::setCurrent(const QString& code) {
  if (code == current_) return;
  if (code != QLatin1String("it") && code != QLatin1String("en")) return;

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

  qInfo("[i18n] lingua %s, esempio: %s", qPrintable(current_),
        qPrintable(QCoreApplication::translate("main", "Preferiti")));
}
