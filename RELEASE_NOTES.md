# Pac'N-Jump native 0.9.0

A port of Pac'N-Jump (Namco, 2011), a mobile game that was pulled from the stores. Portable C99 core with an SDL3 shell. All 8 stages, menus, help and achievements are in.

The release has none of the original game's art, sound or level data. You bring your own copy of the original Android APK (`com.namcobandaigames.pacnjump_and.apk`) and the game builds its data file from it, on the device or in your browser. If you misplaced your APK, someone on the INTERNET may have ARCHIVED it.

## Platforms

| Platform | How you get it | Tested |
| --- | --- | --- |
| Android | Build an APK in your browser at the project page, or import on the phone | Real device (LG G8) |
| Windows, macOS, Linux | Download below, drag your APK onto the window | Built by CI. Steam Deck untested |
| Nintendo 3DS | `pacnjump.cia` for the Home menu, or `pacnjump.3dsx` for the Homebrew Launcher. Copy your APK to the SD card and it imports on the console | New 3DS XL, with stereo 3D. Original 3DS and 2DS untested |
| Sega Dreamcast | Build one CDI from your APK in your browser, on the project page (dreamcast.html) | Real console, progressive and interlaced video on CRTs. VMU saves work. Burned discs and PAL consoles untested |
| PS Vita | `pacnjump.vpk`, install it with VitaShell, then copy your APK into `ux0:data/jon/pnj/` | Real Vita |

## What is new in 0.9.0

- Nintendo 3DS, PS Vita and Sega Dreamcast ports.
- Browser builders for Android and Dreamcast. Your APK stays in your browser and is not uploaded.
- 3DS: stereo 3D, menus and pause screen on the bottom screen, import progress on the top screen, and the app closes cleanly.
- Dreamcast: 60 fps with loading screens and tate. Scores and preferences save to the VMU (about 1 block).
- Vita: tate mode, and both sticks steer.
- The power-up jingle is decoded by the game's own AAC decoder, so it works in browsers with no AAC support.
- Tate mode (rotated screen) on desktop, Vita and Dreamcast.

## Known limits

- Analog sticks are read as a digital direction. True analog control is planned for every version.
- The Windows exe is unsigned, so SmartScreen may warn the first time. macOS needs the Open Anyway step on first launch.
- The 3DS icon is a redraw. A matching icon is on the roadmap.
- The Help pages come from the original game, so they describe its phone controls (touch and tilt), including on the Dreamcast.
- Platforms marked untested above should work, but nobody has tried them. Reports are welcome.
