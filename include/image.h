#pragma once

#include "common.h"
#include "widget.h"

NAMESPACE_BEGIN(TexGui);

class Image : public Widget
{
    friend class Widget;
public:
    Image(const char* image);
    virtual void draw(GLContext* ctx) override;
protected:
};

NAMESPACE_END(TexGui);
