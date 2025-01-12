#pragma once

#include "defaults.h"
#include "common.h"
#include "types.h"
#include <array>

NAMESPACE_BEGIN(TexGui);

class UIContext;

enum KeyState : uint8_t
{
    KEY_Off = 0,
    KEY_Press = 1,
    KEY_On = 2,
    KEY_Release = 3,
};

struct InputState
{
    Math::ivec2 cursor_pos;
    uint8_t lmb;
};

class ImmCtx
{
public:
    InputState* input;
    RenderState* rs;

    Math::fbox bounds;

    inline ImmCtx withBounds(Math::fbox bounds) const
    {
        ImmCtx copy = *this;
        copy.bounds = bounds;
        return copy;
    }

    ImmCtx Window(const char* name, uint32_t flags, float w, float h);
    bool Button(const char* text);
    template <typename... Cols>
    std::vector<ImmCtx> Row(Cols... columns)
    {
        float absoluteWidth = 0;
        float n = 0;
        float inherit = 0;
        auto _width = [&inherit, &absoluteWidth, &n](float col) {
            if (col < 1)
            {
                absoluteWidth += col;
                inherit++;
            }
            n++;
        };

        std::vector<ImmCtx> row;
        auto _row = [&row, &absoluteWidth, &n, &inherit, this](float col) {
            float x = 0, y = 0;
            float spacing = Defaults::Row::Spacing;
            float width = col >= 1 ? col : bounds.width - absoluteWidth - (spacing * (n - 1)) * col / inherit;

            ImmCtx out = *this;
            out.bounds = Math::fbox(bounds.x + x, bounds.y + y, width, bounds.height);

            row.push_back(out);

            x += width + spacing;
        };
        (_width(columns), ...);
        (_row(columns), ...);
        return row;
    }

    template <typename... Rows>
    std::vector<ImmCtx> Column(Rows... rows)
    {
        float absoluteHeight = 0;
        float n = 0;
        float inherit = 0;
        auto _height = [&inherit, &absoluteHeight, &n](float col) {
            if (col < 1)
            {
                absoluteHeight += col;
                inherit++;
            }
            n++;
        };

        std::vector<ImmCtx> column;
        auto _column = [&column, &absoluteHeight, &n, &inherit, this](float row) {
            float x = 0, y = 0;
            float spacing = Defaults::Row::Spacing;
            float height = row >= 1 ? row : bounds.height - absoluteHeight - (spacing * (n - 1)) * row / inherit;

            ImmCtx out = *this;
            out.bounds = Math::fbox(bounds.x + x, bounds.y + y, bounds.width, height);

            column.push_back(out);

            y += height + spacing;
        };
        (_height(rows), ...);
        (_column(rows), ...);
        return column;
    }
private:
    

};

inline InputState g_input_state;
inline RenderState g_immediate_state = {};
inline ImmCtx g_immediate_ctx;

inline void clearImmediate()
{
    g_immediate_state.clear();
}

NAMESPACE_END(TexGui);


