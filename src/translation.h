#ifndef TRANSLATION_H
#define TRANSLATION_H

#include <QCoreApplication>
#include <QString>

// QML strings are extracted under the "main" context; controller messages
// historically use "AppController". Keep both contexts explicitly so the
// existing .qm files remain valid after the controller split.
inline QString TranslateMain(const char* text) {
  return QCoreApplication::translate("main", text);
}

inline QString TranslateApp(const char* text) {
  return QCoreApplication::translate("AppController", text);
}

#endif  // TRANSLATION_H
