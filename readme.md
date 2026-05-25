YACardEmu
============

YACardEmu (*YetAnother*CardEmu) is a software emulator to emulate a range of magnetic card readers found in various arcade machines. Currently supported models are Sanwa CRP-1231BR-10, the CRP-1231LR-10NAB, and CR-S31R-10HS3 models which are commonly found in games such as Wangan Midnight Tune 3, Virtual-On Force, THE iDOLM@STER, F-Zero, Mario Kart, and Initial D series arcade cabinets.

**Checkout some gameplay videos [here!](https://www.youtube.com/channel/UCle6xQNwROzwYfYMyrnIcBQ)**

Card images!
---------
Card images are now working! Place base images in a folder called `images` in the same folder as `YACardEmu.exe`, and the application will randomly choose one when the game calls to save. You'll see afterwards that a image will be placed in the same folder as your cards with your card name. The UI will also display this when picking a card. Enjoy.

Building
---------
#### Prerequisites

Ubuntu / Debian / Raspbian

```sh
sudo apt install build-essential cmake pkg-config libserialport-dev libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev
```

Windows

**[Precompiled Windows binaries can be found here.](https://github.com/GXTX/YACardEmu/tags)**

1. [Visual Studio 2026](https://visualstudio.microsoft.com/vs/)
    * C++ desktop development
    * Windows Universal CRT SDK
    * C++ CMake tools for Windows
    * Git for Windows
		* *Optional if you already have Git installed*

#### Build

Ubuntu / Debian / Raspbian

```
git clone --recursive https://github.com/GXTX/YACardEmu
cd YACardEmu
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

Windows

1. If you don't have CMake installed, open `___ Native Tools Command Prompt for VS 20##`.
2. `git clone --recursive https://github.com/GXTX/YACardEmu`
3. `cd` to the `YACardEmu` directory.
4. Run command with CMake option `cmake -B build -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5`
*Newer CMake version (>3.5) will not work but error. This command force to define legacy CMake.
6. `cd build`
7. `cmake --build . --config Release`
8. All the required files will be in `build\Release`

Getting Started
---------

Configuration is made via both a `config.ini` file & via a web portal / API.
After YACardEmu.exe running, go to `http://YOURIPHERE:8080/` on your browser, this web page is where you'll choose your card and insert your card.
You cannot choose or insert the virtual card but select via your virtual local network (browser).
The card automatically ejected each game ended. You will inset the card every time except in-game continue chosen.

For CXBX-R (Sega Chihiro Emulator) and Flycast (Sega Naomi / Naomi2 Emulator), it is recommended to insert or eject card by in-game cofigurated key (gamepad button).
User no longer need to toggle card on the browser.

Wangan Midnight Maximum Tune 1/2 will generate "廃車カード / Discarded Vehicle Card" automatically after card reached the limits.

Editing `config.ini` to point to where you want your cards stored is required.
If you choose the different directory outside YACardEmu.exe, another config.ini automatically generated in the card folders.
These 2 config.ini should be located the right place.

Ubuntu

```
cp ../config.ini.sample config.ini
./YACardEmu
```

Windows

```
[config]
apiport = 8080
basepath = C:\Users\xxxx\xxxx\yacard\./
serialpath = \\.\pipe\YACardEmu
targetdevice = xxxx
autoselectedcard = card1.bin
```

Running On The Real Card Reader
---------

You must have a USB to RS232 (or a physical serial port) connected to your machine.
If you're wanting to run this on the real hardware there are some settings you'll need to be aware of.

SEGA Chihiro
```
Wangan Maximum Tune 1 : CRP-1231LR-10NAB | 9600 none
Wangan Maximum Tune 2 : CRP-1231LR-10NAB | 9600 none
```

Namco System N2
```
Wangan Maximum Tune 3    : CR-S31R-10HS3 | 38400 even
Wangan Maximum Tune 3DX  : CR-S31R-10HS3 | 38400 even
Wangan Maximum Tune 3DX+ : CR-S31R-10HS3 | 38400 even
```

SEGA Hikaru
```
Virtual-On Force (電脳戦機バーチャロン フォース) : CRP-1231BR-10 | 9600 even
†Requires SEGA P/N 838-13661 RS232 converter PCB
```

Namco System 246
```
THE iDOLM@STER (アイドルマスタ) : CRP-1231LR-10NAB | 9600 none
Dragon Chronicle Online (ドラゴンクロニクルオンライン) : CRP-1231LR-10NAB | 9600 none
Dragon Chronicle: Legend of the Master Ark (ドラゴンクロニクル: マスターアークの伝説) : CRP-1231LR-10NAB | 4800 none
```

SEGA NAOMI
```
Derby Owners Club World Edition : CRP-1231BR-10 | 9600 even
†Requires SEGA P/N 838-13661 RS232 converter PCB
```

SEGA NAOMI 2
```
Initial D Arcade Stage Ver.1 : CRP-1231BR-10 | 9600 even
Initial D Arcade Stage Ver.2 : CRP-1231BR-10 | 9600 even
Initial D Arcade Stage Ver.3 : CRP-1231BR-10 | 9600 even
```

SEGA Triforce
```
F-Zero AX             : CRP-1231BR-10    | 9600 even
Mario Kart Arcade GP  : CRP-1231LR-10NAB | 9600 none
Mario Kart Arcade GP2 : CRP-1231LR-10NAB | 9600 none
```
