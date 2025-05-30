#pragma once

#include <cstdint>

namespace TexGui
{
enum ContainerFlags : uint32_t
{
    CENTER_X = 0x01,
    CENTER_Y = 0x04,
    SLICE_9  = 0x08,
    SLICE_3  = 0x10,
    WRAPPED  = 0x80,

    ALIGN_NONE = 0x100,
    ALIGN_LEFT = 0x200,
    ALIGN_CENTER_Y = 0x400,
    ALIGN_CENTER_X = 0x800,
    ALIGN_CENTER = ALIGN_CENTER_X | ALIGN_CENTER_Y,
    ALIGN_RIGHT = 0x1000,
    ALIGN_TOP = 0x2000,
    ALIGN_BOTTOM = 0x4000,

    UNDERLINE = 0x8000,
    LOCKED = 0x10000, // Window is immovable
    HIDE_TITLE = 0x40000,

    CAPTURE_INPUT = 0x80000,
    RESIZABLE = 0x100000,

    BACK = 0x400000,
    FRONT = 0x800000,

    FORCED_ORDER = BACK | FRONT
};

enum RenderFlags : uint32_t
{
    COLQUAD = 0x0,
};

enum ContainerState : uint8_t 
{
    STATE_NONE   = 0x0,
    STATE_ACTIVE = 0x01,
    STATE_HOVER  = 0x04,
    STATE_PRESS  = 0x08,
    STATE_ALL    = 0xff
};

enum {
    INHERIT = 0
};
}
