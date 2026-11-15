#include "inputpump.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QKeyEvent>
#include <QQuickWindow>

#include <SDL.h>

InputPump::InputPump(QQuickWindow* window, QObject* parent)
    : QObject(parent), window_(window) {
  if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) == 0) {
    sdlReady_ = true;
    const int count = SDL_NumJoysticks();
    for (int i = 0; i < count; ++i) {
      if (SDL_IsGameController(i)) {
        if (SDL_GameController* pad = SDL_GameControllerOpen(i)) pads_.append(pad);
      }
    }
    qInfo("[input] SDL pronto, %d controller", static_cast<int>(pads_.size()));
  } else {
    qWarning("[input] SDL_Init fallita: %s", SDL_GetError());
  }

  timer_.setInterval(16);
  connect(&timer_, &QTimer::timeout, this, &InputPump::poll);
  timer_.start();
}

InputPump::~InputPump() {
  timer_.stop();
  for (SDL_GameController* pad : pads_) SDL_GameControllerClose(pad);
  pads_.clear();
  if (sdlReady_) {
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
  }
}

int InputPump::mapButton(int sdlButton) {
  switch (sdlButton) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
      return Qt::Key_Up;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
      return Qt::Key_Down;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
      return Qt::Key_Left;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
      return Qt::Key_Right;
    case SDL_CONTROLLER_BUTTON_A:
      return Qt::Key_Return;
    case SDL_CONTROLLER_BUTTON_B:
      return Qt::Key_Escape;
    case SDL_CONTROLLER_BUTTON_Y:
      return Qt::Key_Y;
    case SDL_CONTROLLER_BUTTON_START:
      return Qt::Key_Menu;
    case SDL_CONTROLLER_BUTTON_BACK:
      return Qt::Key_Backspace;
    default:
      return 0;
  }
}

void InputPump::sendKey(int qtKey) {
  if (qtKey == 0) return;
  emit keyPressed(qtKey);
}

void InputPump::handleAxis(int value, int negativeKey, int positiveKey, qint64 now,
                           int* lastDir, qint64* nextRepeat) {
  if (value == 0) {
    *lastDir = 0;
    *nextRepeat = 0;
    return;
  }
  if (value != *lastDir) {
    *lastDir = value;
    *nextRepeat = now + 350;  // pausa prima dell'auto-repeat
    sendKey(value < 0 ? negativeKey : positiveKey);
    return;
  }
  if (now >= *nextRepeat) {
    *nextRepeat = now + 120;
    sendKey(value < 0 ? negativeKey : positiveKey);
  }
}

void InputPump::poll() {
  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    switch (ev.type) {
      case SDL_CONTROLLERDEVICEADDED: {
        if (SDL_GameController* pad = SDL_GameControllerOpen(ev.cdevice.which)) {
          pads_.append(pad);
          qInfo("[input] controller collegato");
        }
        break;
      }
      case SDL_CONTROLLERDEVICEREMOVED: {
        for (int i = 0; i < pads_.size(); ++i) {
          if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads_.at(i))) ==
              ev.cdevice.which) {
            SDL_GameControllerClose(pads_.takeAt(i));
            qInfo("[input] controller scollegato");
            break;
          }
        }
        break;
      }
      case SDL_CONTROLLERBUTTONDOWN: {
        const int key = mapButton(ev.cbutton.button);
        if (key != 0) sendKey(key);
        break;
      }
      default:
        break;
    }
  }

  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  for (SDL_GameController* pad : pads_) {
    const int ax = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
    const int ay = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
    const int dx = ax > 18000 ? 1 : (ax < -18000 ? -1 : 0);
    const int dy = ay > 18000 ? 1 : (ay < -18000 ? -1 : 0);
    handleAxis(dx, Qt::Key_Left, Qt::Key_Right, now, &lastDirX_, &nextRepeatX_);
    handleAxis(dy, Qt::Key_Up, Qt::Key_Down, now, &lastDirY_, &nextRepeatY_);
  }
}
