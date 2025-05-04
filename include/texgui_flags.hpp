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
    BLUR     = 0x40,
    WRAPPED  = 0x80,

    ALIGN_NONE = 0x100,
    ALIGN_LEFT = 0x200,
    ALIGN_CENTER_Y = 0x400,
    ALIGN_CENTER_X = 0x800,
    ALIGN_RIGHT = 0x1000,
    ALIGN_TOP = 0x2000,
    ALIGN_BOTTOM = 0x4000,

    UNDERLINE = 0x8000,
    LOCKED = 0x10000, // Window is immovable
    HIDE_TITLE = 0x40000,

    CAPTURE_INPUT = 0x80000,
    RESIZABLE = 0x100000,
};

enum ContainerState : uint8_t 
{
    STATE_NONE   = 0b00000001,
    STATE_ACTIVE = 0b00000010,
    STATE_HOVER  = 0b00000100,
    STATE_PRESS  = 0b00001000,
};

enum {
    INHERIT = 0
};
}
