#ifndef INPUT_PUMP_H
#define INPUT_PUMP_H

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QTimer>

class QQuickWindow;
struct _SDL_GameController;
typedef struct _SDL_GameController SDL_GameController;

// Reads the controller via SDL and translates it into Qt keyboard events, so the
// QML UI stays agnostic about input (arrows, Enter, Esc).
//
// Note: this is the pattern recommended by Valve for the Steam Controller ("SDL game
// controller API"), and it is the one Moonlight uses on this same hardware.
class InputPump : public QObject {
  Q_OBJECT

 signals:
  // autoRepeat=true only for analog stick auto-repeat.
  void keyPressed(int qtKey, bool autoRepeat);

 public:
  explicit InputPump(QQuickWindow* window, QObject* parent = nullptr);
  ~InputPump() override;

  bool ok() const { return sdlReady_; }

 private:
  void poll();
  void sendKey(int qtKey, bool autoRepeat = false);
  void handleAxis(int value, int negativeKey, int positiveKey, qint64 now,
                  int* lastDir, qint64* nextRepeat);
  static int mapButton(int sdlButton);

  QQuickWindow* window_ = nullptr;
  QTimer timer_;
  QList<SDL_GameController*> pads_;
  bool sdlReady_ = false;

  qint64 nextRepeatX_ = 0;
  qint64 nextRepeatY_ = 0;
  int lastDirX_ = 0;
  int lastDirY_ = 0;
};

#endif  // INPUT_PUMP_H
