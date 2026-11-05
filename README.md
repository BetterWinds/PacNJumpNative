# Pac'N-Jump native

A port of Pac'N-Jump (Namco, 2011), a mobile game that was pulled from the stores. Portable C99 core, SDL2 shell. Runs on Android, Windows, macOS and Linux. All 8 stages, menus, help and achievements are in.

**This repo has none of the original game's art, sound or level data.** The game reads all of it from a `pnj.pack` file at run time. The game builds that file from your own copy of the original Android APK (`com.namcobandaigames.pacnjump_and.apk`), so you need to bring the APK.

## Getting it running

**Windows, macOS, Linux:** builds come from the `desktop` workflow (Actions tab, Artifacts). Unzip, run, and drag the APK onto the window. It imports once and shows a progress bar while it works. Tagged versions will show up under Releases. More in `packaging/README-desktop.txt`.

**Android, no tools:** open https://heyjonanderson.github.io/PacNJumpNative/, drop your original APK on the page and download the APK it builds. It all happens in your browser and nothing is uploaded. Android 7.0 and up, 64-bit.

**Android, import on the phone:** `android\package.bat byo` builds an APK with no game data in it. Install it and pick the original APK on first launch.

**From source:** see Building below.

### macOS first launch

The app is not signed, so Gatekeeper blocks it the first time.

- macOS 15 and later: double-click the app and click Done. Open System Settings > Privacy & Security, scroll to Security, click **Open Anyway**, enter your password, click Open. The button only stays for about an hour after you try to open the app.
- macOS 14 and earlier: right-click the app, choose Open, click Open.
- If macOS says the app is damaged: run `xattr -cr PacNJump.app` in Terminal.

## Controls (desktop)

- Keyboard: arrows or WASD, Enter or Space to select, P to pause, Esc to go back (quit on the title screen), R to restart, 1-8 to pick a stage, F11 or Alt+Enter for fullscreen. Everything except R, 1-8 and F11 can be remapped in Options > Controls > KEYS (two keys and one pad button per action).
- Mouse: click buttons, drag to steer.
- Touch: one finger steers. Two fingers hold left and right like the phone's digital pad. Mac trackpads count as a mouse, not touch.
- Gamepad: d-pad or left stick, A to confirm, B to go back, Start to pause.
- Title and game over screens: any key, button, click or tap continues. The prompt says "TOUCH SCREEN" or "PRESS ANYTHING" depending on the last input you used.
- On-screen pad and pause button: Options > Controls > TOUCH UI cycles AUTO, ON and OFF (all platforms). AUTO shows them after a tap or click and hides them once you use a key or pad button.
- Tate: Options > Video > SCREEN ROTATION cycles STANDARD, TATE LEFT and TATE RIGHT (or start with `--orient standard|left|right`). It turns the picture 90 degrees on a normal landscape screen so you can tip the monitor over. TATE LEFT puts the top of the game on the left of your screen, TATE RIGHT on the right. Mouse and touch follow the turn. In Options > Controls > KEYS, MATCH SCREEN remaps the arrows to follow the turned picture and DEFAULTS puts them back. Use DEFAULTS if you rotate the display in your OS instead.
- The window resizes and keeps the 2:3 portrait shape. `--fullscreen` starts fullscreen and `--windowed` forces a window. On a Steam Deck (Steam sets `SteamDeck`) it starts fullscreen by itself.
- Options is split into Sound, Video and Controls. Video has a fullscreen switch (also F11), SCREEN, SCALE and HDR.
- Options > Video > SCALE: SMOOTH fills the window. INT SCALE uses whole-number sizes only, so the dots stay sharp, and leaves black bars. It also works in tate.

## The web builder

`web/builder.js` takes the original APK and the app template (`web/template.apk`, the app with no game data in it) and builds one installable APK in the browser:

1. Reads the original APK and builds the pack.
2. Stores the pack as `assets/pnj.pack` in the template and swaps the placeholder launcher icons for the icon from your own APK.
3. Signs the result (APK Signature Scheme v2, RSA 2048, SHA-256) with a key made in the browser and kept in IndexedDB. Building again in the same browser gives an APK that updates the installed one.

v2 signing means Android 7.0 and up. `web/test_builder.mjs` runs the same code in Node. `pip install apksigtool` then `apksigtool verify out.apk` checks the signature independently.

`.github/workflows/pages.yml` builds the template on GitHub, checks it has no game data in it, and publishes `web/` to GitHub Pages on each push to main (Settings > Pages > Source: GitHub Actions). You do not build or commit the template by hand. Any static host works. The page fetches `template.apk` from its own folder.

## Building

Desktop:

```
git clone --recursive <repo>
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build                   # build/pacnjump
cmake --build build --target bundle   # macOS: PacNJump.app
```

CMake builds the bundled SDL 2.30.9 statically unless you pass `-DPNJ_SYSTEM_SDL=ON`. For quick dev builds, `make -f pnj.mk` works too (needs the SDL2 dev package), and `make -f pnj.mk test` runs a headless smoke test.

Import from a terminal with `pacnjump --import game.apk`. `python tools/make_pack.py game.apk` also writes a `pnj.pack`, but the power-up jingle needs ffmpeg on that route. The importer built into the app decodes the jingle itself.

To build the template locally, `android\package.bat template` writes `web/template.apk` unsigned (CI does this for the live page). Android needs Android Studio. `android\package.bat byo` finds a JDK (Gradle 8.13 wants 21 or 17) and writes a debug-signed APK to `dist\`. Keep your `%USERPROFILE%\.android\debug.keystore`, or later builds will not install over earlier ones. The build is arm64-v8a only and portrait. Add `armeabi-v7a` in `app/build.gradle` if you need 32-bit.

## Layout

```
core/            portable C99 game core: physics, camera, generator, RNG, draw list, input model
platform/sdl/    SDL2 shell: main.c (render, input), menus.h, audio.h, pack.h (finds and reads pnj.pack),
                 import.h (APK to pack), aac_dec.c (jingle decoder), atlas.h
android/         Android Studio project (SDL Java glue, CMake into ../core, file picker activity)
web/             the browser APK builder: builder.js (APK to pack to signed APK, client side), index.html
tools/           make_pack.py, dexatlas.py, gen_maps.py, parity scripts
third_party/     SDL (submodule), stb (image and vorbis, public domain)
```

## pnj.pack

Little endian: `"PNJPACK1"`, u32 count, then count entries of `{ u16 name_len, name, u32 offset, u32 size }`, then the data.

- `tex/<name>.png`: the APK's hdpi sheets, unchanged
- `audio/<name>.ogg` and `audio/wakawaka.wav`: sounds and music, plus the decoded power-up jingle
- `maps/<name>.map`: the 42 level maps
- `lang/language.*`: text, including the Help pages
- `meta/atlas.txt`: the 224 sprite rectangles, one `tex x y w h tw` per line
- `meta.json`

The sprite table is not a data file in the APK. It is built in code inside one class in `classes.dex`, so `tools/dexatlas.py`, `import.h` and `web/builder.js` each read it by walking that class's static initializer. Keep the three in sync. `web/test_pack.mjs` builds just the pack so you can diff them.

The app looks for the pack in this order: `--pack`, `$PNJ_PACK`, the program's own folder (Windows and Linux, if it is writable, otherwise the user data folder), then `pnj.pack` in the current directory. On macOS and Android it is always the user data folder. Scores are saved next to the pack. `PNJ_NO_BUNDLE=1` ignores loose files.

## Parity

The core was checked against the original's game logic step for step: same input, same state, over many seeds on all 8 stages. The only differences are last-bit `sin` and `cos` rounding. The scripts are in `tools/` (`trace_c.c`, `compare.py`, `sweep.sh`). They need a reference build that is not in this repo.

`fx` is `double` by default. `-DPNJ_FLOAT` makes it `float` for devices with an FPU and not much else.

To compile the 42 maps into the binary for a personal build, run `python tools/gen_maps.py game.apk`. It writes `core/pnj_maps_data.h` (gitignored) and the next build picks it up. `-DPNJ_BYO` turns that back off.

## What is in the repo

No art, sound, level data, sprite table or Help text. What is in the repo is the game's rules written as C (physics constants, platform and enemy generator tables, scoring) after watching how the original behaves, plus short UI labels and stage names.

## Code signing policy

The Windows exe is built from this repo by GitHub Actions (`.github/workflows/desktop.yml`) and is meant to be signed through the SignPath Foundation's free program for open source projects. Releases up to now are not signed.

- Authors, reviewers and approvers: Jon Anderson ([@heyjonanderson](https://github.com/heyjonanderson)), who owns the repo and signs off on every release before it is signed.
- Only artifacts built from this repo's source are signed.

Privacy: this program will not transfer any information to other networked systems unless specifically requested by the user.

## License

MIT, for the code in this repo (see LICENSE). It does not cover the original game, which belongs to its owners. Not affiliated with Namco or Bandai Namco.
