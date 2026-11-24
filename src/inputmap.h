#ifndef INPUT_MAP_H
#define INPUT_MAP_H

// Pure key mapping shared by the QML UI and the player. Keeping it out of the
// QML handler makes the rules testable and avoids sprinkling Qt key names
// through every screen.
namespace InputMap {

// Map a physical key to the key the UI understands.
// modifiers is needed to tell the numeric keypad from the number row.
// spaceAsConfirm is true in the UI (Space selects); the player calls with false
// because Space is play/pause there.
int NormalizeKey(int qtKey, int modifiers, bool spaceAsConfirm);

// True for the four arrows, after normalization.
bool IsArrow(int qtKey);

}  // namespace InputMap

#endif  // INPUT_MAP_H
