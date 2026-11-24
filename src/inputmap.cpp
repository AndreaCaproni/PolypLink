#include "inputmap.h"

#include <Qt>

namespace InputMap {

int NormalizeKey(int qtKey, int modifiers, bool spaceAsConfirm) {
  if ((modifiers & Qt::KeypadModifier) != 0) {
    switch (qtKey) {
      case Qt::Key_8:
        return Qt::Key_Up;
      case Qt::Key_2:
        return Qt::Key_Down;
      case Qt::Key_4:
        return Qt::Key_Left;
      case Qt::Key_6:
        return Qt::Key_Right;
      default:
        break;
    }
  }
  switch (qtKey) {
    case Qt::Key_Enter:
    case Qt::Key_Select:
      return Qt::Key_Return;
    case Qt::Key_Backspace:
      return Qt::Key_Escape;
    case Qt::Key_Space:
      return spaceAsConfirm ? Qt::Key_Return : Qt::Key_Space;
    default:
      return qtKey;
  }
}

bool IsArrow(int qtKey) {
  return qtKey == Qt::Key_Up || qtKey == Qt::Key_Down ||
         qtKey == Qt::Key_Left || qtKey == Qt::Key_Right;
}

}  // namespace InputMap
