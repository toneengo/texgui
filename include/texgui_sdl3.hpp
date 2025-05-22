#pragma once

#include "texgui.h"
#include "context.hpp"
#include "types.h"
#include "SDL3/SDL.h"

NAMESPACE_BEGIN(TexGui);
bool initSDL3(SDL_Window* window);
void processEvent_SDL3(const SDL_Event& event);
NAMESPACE_END(TexGui);
