#include "Game/DebugOverlay.h"
#include "Engine/UIRenderer.h"

#include <algorithm>
#include <cmath>

namespace cubiq {

void DebugOverlay::draw(UIRenderer& ui) const
{
    const float scale = std::max(1.0f, std::round(ui.height() / 360.0f));
    const float line = UIRenderer::lineHeight(scale);
    const float pad = 1.0f * scale, margin = 3.0f * scale;
    const glm::vec4 BG(0.0f, 0.0f, 0.0f, 0.45f), TEXT(0.92f, 0.92f, 0.92f, 1.0f);

    auto column = [&](const std::vector<std::string>& lines, bool alignRight) {
        float y = margin;
        for (const std::string& s : lines) {
            if (!s.empty()) {
                float w = UIRenderer::textWidth(s.c_str(), scale);
                float x = alignRight ? ui.width() - margin - w : margin;
                ui.rect(x - pad, y - pad, w + 2 * pad, line, BG);
                ui.text(s.c_str(), x, y, scale, TEXT);
            }
            y += line;
        }
    };
    column(left, false);
    column(right, true);
}

} // namespace cubiq
