#ifndef INPUT_PUMP_H
#define INPUT_PUMP_H

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QTimer>

class QQuickWindow;
struct _SDL_GameController;
typedef struct _SDL_GameController SDL_GameController;

// Legge il controller via SDL e lo traduce in eventi tastiera Qt, cosi' la UI
// QML resta agnostica sull'input (frecce, Invio, Esc).
//
// Nota: e' il pattern raccomandato da Valve per lo Steam Controller ("SDL game
// controller API"), ed e' quello che usa Moonlight su questo stesso hardware.
class InputPump : public QObject {
  Q_OBJECT

 signals:
  void keyPressed(int qtKey);

 public:
  explicit InputPump(QQuickWindow* window, QObject* parent = nullptr);
  ~InputPump() override;

  bool ok() const { return sdlReady_; }

 private:
  void poll();
  void sendKey(int qtKey);
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
