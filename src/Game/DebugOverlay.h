#pragma once
#include <string>
#include <vector>

namespace cubiq {

class UIRenderer;

// Ladiaca obrazovka (F3): riadky textu vľavo a vpravo, každý na polopriehľadnom pozadí.
struct DebugOverlay {
    std::vector<std::string> left;
    std::vector<std::string> right;

    void draw(UIRenderer& ui) const;
};

} // namespace cubiq
