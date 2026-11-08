Pac'N-Jump (native port) for the PS Vita

This has none of the original game's art or sound. It builds them from your own copy of the original
Pac'N-Jump Android APK (com.namcobandaigames.pacnjump_and.apk).

You need a Vita running custom firmware with VitaShell installed. It is tested in Vita3K and on a real Vita.

Where things go
  1. pacnjump.vpk   Copy it to the Vita (over USB, or by FTP from VitaShell), then open it in VitaShell and
                    choose install. A Pac'N-Jump bubble appears on the LiveArea.
  2. Start the game once. It creates its data folder and shows where it is looking for your APK.
  3. Your APK       Copy it into ux0:data/jon/pnj/ . The file can have any name that ends in .apk. The game
                    finds it by itself and imports it. The first time takes up to a minute, and it only
                    happens once.
  4. To play with the Vita turned on its side, open Options > Video and set SCREEN ROTATION. Both sticks steer.

The game's data and your scores are kept in ux0:data/jon/pnj/.

Project page and source: https://github.com/heyjonanderson/PacNJumpNative
MIT license for the code. It does not cover the original game. Not affiliated with Namco or Bandai Namco.
