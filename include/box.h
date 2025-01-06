#pragma once

#include "common.h"
#include "widget.h"

NAMESPACE_BEGIN(TexGui);

class Box : public Widget
{
    friend class Widget;
public:
    Box(int x = 0, int y = 0, int width = INHERIT, int height = INHERIT);
    virtual void draw(RenderState& state);
protected:
};

NAMESPACE_END(TexGui);
