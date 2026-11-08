Pac'N-Jump (native port) for the Nintendo 3DS

This has none of the original game's art or sound. It builds them from your own copy of the original
Pac'N-Jump Android APK (com.namcobandaigames.pacnjump_and.apk).

You need a 3DS running custom firmware and a CIA installer such as FBI. A New 3DS XL is tested. The original
3DS and the 2DS have not been tried.

Where things go
  1. pacnjump.cia   Put it anywhere on the SD card, then open it in FBI and choose Install. A Pac'N-Jump icon
                    appears on the Home menu.
  2. Your APK       Copy it into SD:/3ds/pacnjump/ (the folder "pacnjump" inside "3ds" at the top of the card).
                    Make the folder if it is not there. The file can have any name that ends in .apk.
  3. Start the game It finds the APK and imports it. The first time takes a few minutes. Keep the power on and
                    the SD card in. This only happens once.
  4. Sound          The 3DS needs its DSP firmware file at SD:/3ds/dspfirm.cdc. Without it the game runs
                    silent. The DSP1 homebrew app makes that file from your own console.

The game's data and your scores are kept in SD:/3ds/pacnjump/.

Project page and source: https://github.com/heyjonanderson/PacNJumpNative
MIT license for the code. It does not cover the original game. Not affiliated with Namco or Bandai Namco.
