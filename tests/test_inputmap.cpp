#include "inputmap.h"

#include <Qt>
#include <cstdio>

namespace {
int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "inputmap: FAIL %s\n", what);
    ++failures;
  }
}
}  // namespace

int main() {
  // Navigation passes through unchanged.
  check(InputMap::NormalizeKey(Qt::Key_Up, 0, true) == Qt::Key_Up, "Up");
  check(InputMap::NormalizeKey(Qt::Key_Down, 0, true) == Qt::Key_Down, "Down");
  check(InputMap::NormalizeKey(Qt::Key_Left, 0, true) == Qt::Key_Left, "Left");
  check(InputMap::NormalizeKey(Qt::Key_Right, 0, true) == Qt::Key_Right, "Right");

  // Numeric keypad arrows.
  check(InputMap::NormalizeKey(Qt::Key_8, Qt::KeypadModifier, true) == Qt::Key_Up, "NumPad8");
  check(InputMap::NormalizeKey(Qt::Key_8, 0, true) == Qt::Key_8, "row 8 untouched");
  check(InputMap::NormalizeKey(Qt::Key_2, Qt::KeypadModifier, true) == Qt::Key_Down, "NumPad2");
  check(InputMap::NormalizeKey(Qt::Key_4, Qt::KeypadModifier, true) == Qt::Key_Left, "NumPad4");
  check(InputMap::NormalizeKey(Qt::Key_6, Qt::KeypadModifier, true) == Qt::Key_Right, "NumPad6");

  // Confirmation and back.
  check(InputMap::NormalizeKey(Qt::Key_Enter, 0, true) == Qt::Key_Return, "Enter");
  check(InputMap::NormalizeKey(Qt::Key_Select, 0, true) == Qt::Key_Return, "Select");
  check(InputMap::NormalizeKey(Qt::Key_Backspace, 0, true) == Qt::Key_Escape, "Backspace");

  // Space selects in the UI but stays Space in the player.
  check(InputMap::NormalizeKey(Qt::Key_Space, 0, true) == Qt::Key_Return, "Space UI");
  check(InputMap::NormalizeKey(Qt::Key_Space, 0, false) == Qt::Key_Space, "Space player");

  // WASD is deliberately not remapped: typing must keep working.
  check(InputMap::NormalizeKey(Qt::Key_W, 0, true) == Qt::Key_W, "W untouched");
  check(InputMap::NormalizeKey(Qt::Key_A, 0, true) == Qt::Key_A, "A untouched");
  check(InputMap::NormalizeKey(Qt::Key_S, 0, true) == Qt::Key_S, "S untouched");
  check(InputMap::NormalizeKey(Qt::Key_D, 0, true) == Qt::Key_D, "D untouched");

  check(InputMap::IsArrow(Qt::Key_Left), "IsArrow left");
  check(InputMap::IsArrow(Qt::Key_Up), "IsArrow up");
  check(!InputMap::IsArrow(Qt::Key_Return), "IsArrow return");
  check(!InputMap::IsArrow(Qt::Key_Escape), "IsArrow escape");

  if (failures == 0) std::puts("inputmap tests: ok");
  return failures == 0 ? 0 : 1;
}
