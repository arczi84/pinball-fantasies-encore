#pragma once
// The system's own shortcuts stood down while a ball is in play, where the game's keys set them
// off too easily (the keyboard itself is taken by the window: Window::setPlaying).

namespace encore {

/// While `off`, and back once it is not:
/// - macOS: the window server's shortcuts, for this program (Spotlight on Cmd+Space, the emoji
///   on the Globe key, Cmd+Tab, Mission Control...). The switch is private, and looked up:
///   without it nothing happens. It holds only for this program's connection to the window
///   server, so it goes with the program however that ends.
/// - Windows: the accessibility features' shortcuts: Shift pressed five times (Sticky Keys),
///   right Shift held eight seconds (Filter Keys) and Num Lock held five (Toggle Keys). A feature
///   the player has on is left as it is. The player's settings come back as they were read.
/// - Elsewhere, nothing.
void systemShortcutsOff(bool off);

}  // namespace encore
