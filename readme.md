YACardEmu
============

YACardEmu (*YetAnother*CardEmu) is a software emulator to emulate a range of magnetic card readers found in various arcade machines. Currently supported models are Sanwa CRP-1231BR-10, the CRP-1231LR-10NAB, and CR-S31R-10HS3 models which are commonly found in games such as Wangan Midnight Tune 3, Virtual-On Force, THE iDOLM@STER, F-Zero, Mario Kart, and Initial D series arcade cabinets.

**Checkout some gameplay videos [here!](https://www.youtube.com/channel/UCle6xQNwROzwYfYMyrnIcBQ)**

Building
---------
#### Prerequisites

Ubuntu / Debian / Raspbian

```sh
sudo apt install build-essential cmake pkg-config libserialport-dev
```

Windows

**[Precompiled Windows binaries can be found here.](https://github.com/GXTX/YACardEmu/tags)**

1. [Visual Studio 2022](https://visualstudio.microsoft.com/vs/)
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
4. Run these commands.
    1. `mkdir build & cd build`
    2. `cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release`
        * VS2022 17.0 or later is required.
5. `cmake --build . --config Release`
6. All the required files will be in `build\Release`

Running
---------

You must have a USB to RS232 (or a physical serial port) connected to your machine. Configuration is made via both a `config.ini` file & via a web portal / API.
To access the web portal point your browser to `http://YOURIPHERE:8080/`, this web page is where you'll choose your card and insert your card.

Editing `config.ini` to point to where you want your cards stored is required.

Ubuntu

```
cp ../config.ini.sample config.ini
./YACardEmu
```

Windows

```
cd Release
YACardEmu.exe
```

Running On Hardware
---------

If you're wanting to run this on hardware there are some settings you'll need to be aware of.

SEGA Chihiro
```
Wangan Maximum Tune 1 : CRP-1231LR-10NAB | 9600b none
Wangan Maximum Tune 2 : CRP-1231LR-10NAB | 9600b none
```

Namco System N2
```
Wangan Maximum Tune 3    : CR-S31R-10HS3 | 38400b even
Wangan Maximum Tune 3DX  : CR-S31R-10HS3 | 38400b even
Wangan Maximum Tune 3DX+ : CR-S31R-10HS3 | 38400b even
```

SEGA Hikaru
```
Virtual-On Force (電脳戦機バーチャロン フォース) : CRP-1231BR-10 | 9600b even
†Requires Sega P/N 838-13661 RS232 converter PCB
```

Namco System 246
```
THE iDOLM@STER (アイドルマスタ) : CRP-1231LR-10NAB | 9600b none
```

SEGA NAOMI 2
```
Initial D Arcade Stage Ver.1 : CRP-1231BR-10 | 9600b even
Initial D Arcade Stage Ver.2 : CRP-1231BR-10 | 9600b even
Initial D Arcade Stage Ver.3 : CRP-1231BR-10 | 9600b even
```

SEGA Triforce
```
F-Zero AX             : CRP-1231BR-10    | 9600b even
Mario Kart Arcade GP  : CRP-1231LR-10NAB | 9600b none
Mario Kart Arcade GP2 : CRP-1231LR-10NAB | 9600b none
```
