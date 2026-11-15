#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <QObject>
#include <QTranslator>
#include <QVariantList>

class QQmlApplicationEngine;

// Gestisce la lingua dell'interfaccia: carica il file .qm dalla risorsa e
// chiede al motore QML di rivalutare le stringhe qsTr() senza riavviare.
class Language : public QObject {
  Q_OBJECT

  Q_PROPERTY(QString current READ current NOTIFY currentChanged)
  Q_PROPERTY(QVariantList available READ available CONSTANT)

 public:
  explicit Language(QQmlApplicationEngine* engine, QObject* parent = nullptr);

  QString current() const { return current_; }
  QVariantList available() const;

  Q_INVOKABLE void setCurrent(const QString& code);

 signals:
  void currentChanged();

 private:
  void apply();

  QQmlApplicationEngine* engine_ = nullptr;
  QTranslator translator_;
  QString current_ = QStringLiteral("it");
};

#endif  // LANGUAGE_H
