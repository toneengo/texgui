#pragma once

#include <vector>
#include "types.h"
#include "context.h"
#include "stdio.h"

NAMESPACE_BEGIN(TexGui);

class Widget
{
friend class Row;
friend class Column;
friend class Label;
friend class ListItem;
public:
    Widget(float xpos = 0, float ypos = 0, float width = 0, float height = 0);
    virtual void       draw(RenderState& state);
    virtual void       update();

    inline void        setPos(Math::fvec2 pos) { m_box.pos = pos; };
    inline virtual void        setSize(Math::fvec2 size) {
        m_box.size = size;
        if (size.x < 1) m_inherit.width = size.x == 0 ? 1 : size.x;
        else m_inherit.width = 0;
        if (size.y < 1) m_inherit.height = size.y == 0 ? 1 : size.y;
        else m_inherit.height = 0;
        m_needs_update = true;
    }
    inline void        setAlign(uint32_t xa, uint32_t ya) { m_align.x = xa, m_align.y = ya; }
    void               setFlags(unsigned int flags) { m_render_flags = flags; m_needs_update = true; };

    inline Math::fvec2 getPos() { return m_box.pos; };
    inline Math::fvec2 getSize() { return m_box.size; };

    void    setPadding(float px) { m_padding = Math::fvec4(px); };
    Widget* addChild(Widget* widget);
    Widget* findWidget(Math::ivec2& pos);
    virtual bool    contains(const Math::fvec2& pos);
    bool    visible() { return m_visible; };
    void    assignTexture(std::string tex);
    void    setActiveChild(Widget* widget) { if (m_parent) m_parent->setActiveChild(widget); m_active_child = widget; };
    virtual void    clear() { m_children.clear(); m_needs_update = true; }

    virtual void onFrameResizeEvent(int button, int action) {};
    virtual void onMouseDownEvent(int button, int action);
    virtual void onCursorPosEvent(int x, int y);
    virtual void onCursorEnterEvent();
    virtual void onCursorExitEvent();
    virtual void onKeyEvent(int key, int scancode, int action, int mods) {};
    virtual void onCharEvent(unsigned int codepoint) {};

    Widget*& operator[](size_t idx) {
        return m_children[idx];
    }
protected:
    Widget* m_parent = nullptr;
    std::vector<Widget*> m_children;

    std::string  m_texture_name;
    TexEntry*    m_texentry;

    Math::fbox   m_box;
    Math::fbox   m_bounds;
    Math::fbox   m_inherit;
    Math::uivec2 m_align;
    Math::fvec4  m_padding = {0, 0, 0, 0};
    Math::fvec4  m_margin  = {0, 0, 0, 0};
    Math::fvec4  m_text_color;
    Math::ivec2  m_cursor_pos;

    float        m_text_scale;
    int          m_pixel_size;
    Widget*      m_active_child = nullptr;
    unsigned int m_render_flags;
    unsigned int m_state;

    bool m_inherit_bounds = true;
    bool m_visible;

    bool m_needs_update = true;
};

NAMESPACE_END();
