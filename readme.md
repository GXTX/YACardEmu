YACardEmu
============

YACardEmu (*YetAnother*CardEmu) is a program to emulate the Sanwa CRP-1231LR-10NAB card reader that's found on Wangan Midnight Tune 1, 2, 3, 3DX, and 3DX+ arcade machines by Namco. A longer goal of this project is to be compatible with similar models that line of readers (such as 1231BR).

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

Info
---------

If you're wishing to run this on hardware there are some settings you'll need to be aware of.

```
MT1: CRP-1231LR-10NAB     | 9600b none
MT2: CRP-1231LR-10NAB     | 9600b none
MT3/DX/DX+: CR-S31R-10HS3 | 38400b even
F-Zero: CRP-1231BR        | 9600b even
```
