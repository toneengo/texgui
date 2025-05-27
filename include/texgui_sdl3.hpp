#pragma once

#include "texgui.h"
#include "texgui_types.hpp"
#include "SDL3/SDL.h"

NAMESPACE_BEGIN(TexGui);
bool initSDL3(SDL_Window* window);
void processEvent_SDL3(const SDL_Event& event);
NAMESPACE_END(TexGui);
