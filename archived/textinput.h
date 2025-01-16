#pragma once

#include "common.h"
#include "widget.h"
#include <chrono>

NAMESPACE_BEGIN(TexGui);

class TextInput : public Widget
{
    friend class Widget;
public:
    TextInput(const char* placeholder, int x = 0, int y = 0, int width = INHERIT, int height = INHERIT);
    virtual void draw(RenderState& state);
    void onCursorPosEvent(int x, int y);
    void onMouseDownEvent(int button, int action);
    void onKeyEvent(int key, int scancode, int action, int mods);
    void onCharEvent(unsigned int codepoint);

    std::string str() { return m_text_buf; };
protected:
    std::string m_text_buf;
    std::vector<float> m_pos_buf;

    std::string m_placeholder;

    float m_width;
    float m_offsetx;

    int m_text_cur;

    //x1, x2
    Math::ivec2 m_sel;

    Math::fvec4 m_padding;
private:
    std::chrono::nanoseconds accumulator;
    inline void eraseSelection();
    bool showTextCursor;
};

NAMESPACE_END(TexGui);
