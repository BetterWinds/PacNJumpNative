# Pac'N-Jump native 0.9.1

A port of Pac'N-Jump (Namco, 2011), a mobile game that was pulled from the Android and iOS app stores. Portable C99 core with an SDL3 shell. All 8 stages, menus, help and achievements are in.

The release has none of the original game's art, sound or level data. You bring your own copy of the original Android APK (`com.namcobandaigames.pacnjump_and.apk`) and the game builds its data file from it, on the device or in your browser. If you misplaced your APK, someone on the INTERNET may have ARCHIVED it.

## Platforms

| Platform | How you get it | Tested |
| --- | --- | --- |
| Android | Build an APK in your browser at the project page, or import on the phone https://heyjonanderson.github.io/PacNJumpNative/ | Tested on an old LG G8 |
| Windows, macOS, Linux | Download below, drag your APK onto the game window | Built by CI. Tested on Windows w/ HDR, MacOS, and Linux VM. Steam Deck untested |
| Nintendo 3DS | Download the 3DS zip below, copy the `.cia` inside to your SD card, and install it with FBI. Copy your APK to the SD card `SD:/3ds/pacnjump/` and it imports on the console on load | Tested on Azahar and New 3DS XL, with stereo 3D. Original 3DS and 2DS untested |
| Sega Dreamcast | Build one CDI from your APK in your browser, on the project page https://heyjonanderson.github.io/PacNJumpNative/dreamcast.html | Real console with MODE, progressive and interlaced video on CRTs. VMU saves work. Burned discs and PAL consoles untested |
| PS Vita | Download the Vita zip below, install the `.vpk` inside with VitaShell, then copy your APK into `ux0:data/jon/pnj/` | Vita3K, Real Vita |

## What is new in 0.9.1

- RecompLauncher support for the desktop builds. The release files now include a `recomplauncher.json` so the launcher can start the game and show its settings (window or fullscreen, screen rotation, HDR). The project is not listed in the launcher yet, and the file has not been tested inside it.
- The desktop downloads now carry the version and platform in their names, for example `PacNJump-v0.9.1-Windows-x64.zip`, which is how the launcher looks for them. The contents are the same.
- The 3DS and Vita downloads are now zips (`PacNJump-v0.9.1-3DS.zip` and `PacNJump-v0.9.1-Vita.zip`). Each holds the `.cia` or `.vpk` plus a short txt on where to put the files.
- The README is rewritten for first-time readers, with a features list.

## What came in 0.9.0

- Nintendo 3DS, PS Vita, and Sega Dreamcast ports.
- Browser builders for Android and Dreamcast. Your APK stays in your browser and is not uploaded.
- 3DS: stereo 3D, menus and pause screen on the bottom screen, import progress on the top screen, and a clean app exit.
- Dreamcast: 60 fps with loading screens and tate. Scores and preferences save to the VMU (about 1 block).
- Vita: tate mode, and both sticks steer.
- The power-up jingle is decoded by the game's own AAC decoder, so it works in browsers with no AAC support.
- Tate mode (rotated screen) on desktop, Vita, and Dreamcast.

## Known limits

- Analog sticks are read as a digital direction. True analog control is planned for every version.
- The Windows exe is unsigned, so SmartScreen may warn the first time. macOS needs the Open Anyway step on first launch.
- The Help pages come from the original game, so they describe its phone controls (touch and tilt), including on the Dreamcast.
- RecompLauncher support is new. It has not been tested in the launcher yet.
- Platforms marked untested above should work, but nobody has tried them. Reports are welcome.

