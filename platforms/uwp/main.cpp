#include <Windows.h>
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

int bootstrap(int argc, char** argv)
{
  // todo: Force these in SDL-UWP? (at least wgi)
  SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "true");
  SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "true");

  return SDL_main(argc, argv);
}

int CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR argv, int argc)
{
  return SDL_RunApp(0, 0, bootstrap, NULL);
}
