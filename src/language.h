#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <QObject>
#include <QTranslator>
#include <QVariantList>

class QQmlApplicationEngine;

// Manages the UI language: loads the .qm file from the resource and
// asks the QML engine to re-evaluate qsTr() strings without restarting.
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
