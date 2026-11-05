# HDR and the SDL3 move

Status: the desktop shell and the Android project both run on SDL3 (branch `sdl3`, SDL 3.4.20 in `third_party/SDL3`) and there is a first HDR pass. The HDR part has only been compiled and run on a non-HDR display here. It needs a look on a real HDR screen. The Android build is only verified by CI so far (CI uploads a test APK on this branch), not yet run on the LG G8.

## What is in the branch

- Options > HDR OFF/ON (desktop). It also saves, and `--hdr` / `--no-hdr` override it for one run. The line next to the button says SDR OUTPUT, ACTIVE with the display's peak, or NO HDR DISPLAY FOUND.
- With HDR on, the renderer is made with a linear output colorspace. SDL then scales ordinary drawing to the display's SDR white by itself, so the game should look the same as before. Colors above 1.0 are allowed.
- Glow and sparkle sprites (everything drawn additively) get pushed past white: gain is 1 + 0.65 x (min(peak, 4) - 1).
- Tate with HDR draws into a float texture so the highlights survive the turn.
- If the renderer will not give linear output (OpenGL, software), the game falls back to SDR and says so in the Options line.
- `pacnjump --hdrtest` shows white patches at 1x, 1.5x, 2x, 3x and 4x SDR white. On an HDR display they should keep getting brighter until the display runs out.

SDL3's own docs say the Direct3D 11, Direct3D 12 and Metal renderers support the linear output (the source also has it in Vulkan and GPU). Windows will pick Direct3D 11.


## Why SDL3

HDR output is only exposed in SDL3's renderer. SDL2 has no way to ask for an HDR output or read the display's headroom. So HDR means moving the shell to SDL3 first.

SDL3 renderer properties that matter:

- `SDL_PROP_RENDERER_OUTPUT_COLORSPACE_NUMBER`: set to sRGB linear or HDR10 at creation to ask for HDR. Default is sRGB.
- `SDL_PROP_RENDERER_HDR_ENABLED_BOOLEAN`: true when the output is HDR and the display has headroom. It can change while running.
- `SDL_PROP_RENDERER_SDR_WHITE_POINT_FLOAT`: what "normal white" is on this display.
- `SDL_PROP_RENDERER_HDR_HEADROOM_FLOAT`: peak brightness as a multiple of SDR white. 1.0 when HDR is off.
- `SDL_EVENT_WINDOW_HDR_STATE_CHANGED`: fires when the user flips HDR in the OS.

Not confirmed from the docs: which renderer backends actually do HDR (Direct3D 11/12, Metal, Vulkan are the likely ones, OpenGL probably not). That needs a test on real hardware (the ROG Ally and a Mac with an HDR display).

## What HDR would mean for this game

The art is 8-bit sRGB sprites from a 2011 phone game. There is nothing to "upgrade". The honest options are small:

1. Render as normal. SDR content on an HDR display just works once the colorspace is handled. This is the minimum and what "supports HDR" should mean here.
2. Push a few things past SDR white, like the power-up flash, the score pop and the Pac-Man glow, using the headroom value. This is a small change in the draw code (a color multiplier above 1.0) and only does anything when `HDR_ENABLED` is true.

Anything beyond that is inventing a look the game never had.

## Migration surface

The shell is about 1,770 lines (`main.c`, `menus.h`, `audio.h`, `pack.h`, `import.h`). The core does not touch SDL. A rough grep of the calls in use:

| Area | SDL2 now | SDL3 change |
| --- | --- | --- |
| Init and window | `SDL_Init`, `SDL_CreateWindow(x, y, w, h, flags)`, `SDL_WINDOW_FULLSCREEN_DESKTOP`, `SDL_WINDOW_ALLOW_HIGHDPI` | `SDL_CreateWindow(title, w, h, flags)`, fullscreen is `SDL_SetWindowFullscreen` with a bool, high DPI is the default, position set separately |
| Renderer | `SDL_CreateRenderer(win, -1, flags)`, `SDL_RENDERER_PRESENTVSYNC` | `SDL_CreateRenderer(win, NULL)`, vsync via `SDL_SetRenderVSync` |
| Draw calls | `SDL_RenderCopyF`, `SDL_RenderCopyExF`, `SDL_RenderFillRectF`, `SDL_RenderDrawLineF` | `SDL_RenderTexture`, `SDL_RenderTextureRotated`, `SDL_RenderFillRect`, `SDL_RenderLine` (float is the only form) |
| Scaling | `SDL_RenderSetLogicalSize`, `SDL_RenderSetIntegerScale`, `SDL_HINT_RENDER_SCALE_QUALITY` | `SDL_SetRenderLogicalPresentation` with letterbox or integer-scale mode, `SDL_SetTextureScaleMode` per texture (nearest or linear) |
| Surfaces and textures | `SDL_CreateRGBSurfaceWithFormat`, `SDL_FreeSurface` | `SDL_CreateSurface`, `SDL_DestroySurface` |
| Audio | `SDL_OpenAudioDevice` callback, `SDL_BuildAudioCVT`/`SDL_ConvertAudio`, `SDL_LoadWAV_RW` | audio streams (`SDL_OpenAudioDeviceStream` with a callback, `SDL_ConvertAudioSamples`, `SDL_LoadWAV_IO`) |
| File IO | `SDL_RWops`, `SDL_RWFromFile` | `SDL_IOStream`, `SDL_IOFromFile` |
| Events | `SDL_QUIT`, `SDL_WINDOWEVENT`, `SDL_KEYDOWN`, `SDL_CONTROLLERBUTTONDOWN`, `SDL_FINGERDOWN` | `SDL_EVENT_QUIT`, one event per window event, `SDL_EVENT_KEY_DOWN`, `SDL_EVENT_GAMEPAD_BUTTON_DOWN`, `SDL_EVENT_FINGER_DOWN` |
| Keys | `keysym.sym`, `SDL_GetKeyboardState` | `event.key.key`, `event.key.scancode`, `SDL_GetKeyboardState` returns bool |
| Gamepad | `SDL_GameController*`, `SDL_CONTROLLER_BUTTON_*` | `SDL_Gamepad*`, `SDL_GAMEPAD_BUTTON_*`, face button names changed meaning (label vs position) |
| Touch | `SDL_GetTouchDeviceType`, `SDL_TOUCH_MOUSEID`, mouse/touch hints | `SDL_GetTouchDeviceType` still there, touch-mouse hints renamed |
| Entry point | `SDL_main` via SDL2main | `SDL_main.h` and `SDL_MAIN_USE_CALLBACKS` optional, Android glue changes |
| Android | SDL2 Java glue in `android/` | SDL3 has its own Java glue (`SDLActivity`) and a different JNI setup, so the Android project needs its glue replaced |

Biggest risks, in order: the Android glue, the audio rewrite (the mixer in `audio.h` is callback based and holds a lock), and the gamepad button label change (A/B swap behavior on Nintendo-style pads).

## Plan

1. Add SDL3 as a second submodule on an `sdl3` branch. Keep SDL2 on `main` until SDL3 passes every test.
2. Add a thin compat header (`sdlx.h`) so `main.c` and `menus.h` call one set of names. That keeps the diff reviewable and lets both versions build for a while.
3. Desktop first (Linux, then Windows and macOS through CI). Android last.
4. Turn on the HDR colorspace behind a menu toggle (OFF by default), read the headroom properties, and add the handful of over-white highlights.
5. Test on real hardware: an HDR monitor on Windows, an HDR-capable Mac display, and the ROG Ally's panel in HDR mode.

## Not doing

- No tone-mapped "HDR look" on art that has none.
- No SDL3 switch on `main` until Android builds and runs on the LG G8.
