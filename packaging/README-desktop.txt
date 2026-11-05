Pac'N-Jump (native port)

This program has none of the original game's art or sound. It builds them from your own copy of the original
Pac'N-Jump Android APK.

First run
  1. Start the program. It will ask for the game data.
  2. Drag the original APK (com.namcobandaigames.pacnjump_and.apk) onto the window.
     On a Mac you can also drop it on the app icon.
  Or from a terminal:  pacnjump --import path/to/game.apk
  This is done once. A progress bar shows while it runs. On Windows and Linux the data is saved next to the program (pnj.pack); on macOS it is saved in your user data folder.
  The power-up jingle is converted during the import too. Nothing else needs to be installed.

Controls
  Left / Right arrows or A / D    steer in the game (all keys can be changed: Options > Controls > KEYS)
  Menus: arrows (or W A S D)      move between buttons, Enter or Space presses, Esc goes back
                                  (Left/Right turn pages, and move the sliders on a sound row in Options)
  Mouse                           click and drag, like a finger
  Touchscreen                     works, several fingers at once
  Game controller                 d-pad or left stick, A = select, B = back, Start = pause
  P or Esc                        pause
  R                               restart
  F11 or Alt+Enter                fullscreen
  Title / game over screens       any key, pad button, click or tap continues
  On-screen pad and pause button  Options > Controls > TOUCH UI: AUTO (hidden while you use keyboard or pad), ON, OFF
  --fullscreen                    start fullscreen (a Steam Deck does this by itself)
  --windowed                      force a window
  Options > Video > SCALE                 SMOOTH fills the window, INT SCALE keeps whole-number sizes and sharp dots (with black bars)
  --orient standard|left|right    screen orientation (see Tate below)

Tate (portrait) play
  The game is portrait. By default the window is 2:3 and the picture is letterboxed at any size.
  To play with the monitor tipped on its side, open Options and press the SCREEN ROTATION button to pick TATE LEFT or
  TATE RIGHT. The picture turns 90 degrees on your landscape screen: with TATE LEFT the top of the game is on the
  left of your screen, with TATE RIGHT it is on the right. Mouse and touch follow the turn.
  Controls: Options > Controls > KEYS. Tap a box, then press the key or pad button. MATCH SCREEN sets the arrows to follow the
  turned picture (for example TATE RIGHT: Up arrow steers left). DEFAULTS restores the normal keys.
  If you rotate the display in your OS settings instead, leave SCREEN ROTATION on STANDARD and use --fullscreen.
  On Linux with X11 a rotated touchscreen may need `xinput map-to-output`.

macOS: opening the app the first time
  The app is not signed with an Apple developer certificate, so macOS blocks it at first. This is normal.
  macOS 15 (Sequoia) and later:
    1. Double-click PacNJump. macOS says it could not verify the app. Click Done.
    2. Open the Apple menu > System Settings > Privacy & Security.
    3. Scroll down to the Security section. It says "PacNJump" was blocked. Click Open Anyway.
       (The button only stays there for about an hour after you try to open the app. If it is gone, double-click the app again.)
    4. Enter your Mac password, then click Open. macOS remembers this, so the app opens normally from now on.
  macOS 14 (Sonoma) and earlier: right-click (or Control-click) PacNJump, choose Open, then click Open again.
  If none of that works, or the app says it is "damaged", open Terminal, type "xattr -cr " (with the trailing space), drag
  PacNJump.app into the Terminal window, press Return, then open the app again. That only removes the "downloaded from
  the internet" mark from this one app.
  Tip: move PacNJump.app to your Applications folder (or anywhere outside the zip's folder) before the first run.
