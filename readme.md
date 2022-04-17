YACardEmu
============

YACardEmu (*YetAnother*CardEmu) is a program to emulate the Sanwa CRP-1231LR-10NAB card reader that's found on Wangan Midnight Tune 1 and 2 arcade machines by Namco. A longer goal of this project is to be compatible with newer and older models that line of readers.

**Checkout some gameplay videos [here!](https://www.youtube.com/channel/UCle6xQNwROzwYfYMyrnIcBQ)**

Building
---------
##### Prerequisites

Ubuntu / Debian / Raspbian

```sh
sudo apt install build-essential cmake pkg-config libserialport-dev libicu-dev libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev
```

Windows

[VS2022](https://visualstudio.microsoft.com/vs/)  
[premake5](https://premake.github.io/download)  
[libserialport](https://github.com/sigrokproject/libserialport/archive/refs/heads/master.zip)  
[icu](https://github.com/unicode-org/icu/releases/download/release-71-1/icu4c-71_1-Win32-MSVC2019.zip)  
[SDL2](https://www.libsdl.org/release/SDL2-devel-2.0.20-VC.zip)  
[SDL2_ttf](https://github.com/libsdl-org/SDL_ttf/releases/download/release-2.0.18/SDL2_ttf-devel-2.0.18-VC.zip)  
[SDL2_image](https://www.libsdl.org/projects/SDL_image/release/SDL2_image-devel-2.0.5-VC.zip)


##### Build

Ubuntu / Debian / Raspbian

```
git clone --recursive https://github.com/GXTX/YACardEmu
cd YACardEmu
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

Windows

```
git clone --recursive https://github.com/GXTX/YACardEmu
cd YACardEmu
```
Extract premake5 (next to premake5.bat)
Extract libserialport to 3rdparty/libserialport  
Extract icu to 3rdparty/icu4c  
Extract SDL2 to 3rdparty/SDL2 (!! without the SDL2-2.0.20 folder !!)  
Extract SDL2_ttf to 3rdparty/SDL2_ttf (!! without the SDL2_ttf-2.0.18 folder !!)  
Extract SDL2_image to 3rdparty/SDL2_image (!! without the SDL2_image-2.0.5 folder !!)

Run premake5.bat
Open YACardEmu.sln and build solution (debug or release).

Running
---------

You must have a USB to RS242 (or a physical serial port) connected to your machine. Configuration is made via both a `config.ini` file & via a web portal / API.
To access the web portal point your browser to `http://YOURIPHERE:8080/`, this web page is where you'll choose your card and insert your card.

Editing `config.ini` to point to where you want your cards stored is required. 

Ubuntu

The program *must* be ran as root.

```
cp ../config.ini.sample config.ini
sudo ./YACardEmu
```

Windows

```
cd bin\release
YACardEmu.exe
```