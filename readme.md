YACardEmu
============

YACardEmu (*YetAnother*CardEmu) is a program to emulate the Sanwa CRP-1231LR-10NAB card reader that's found on Wangan Midnight Tune 1, 2, 3, 3DX, and 3DX+ arcade machines by Namco. A longer goal of this project is to be compatible with similar models that line of readers (such as 1231BR).

**Checkout some gameplay videos [here!](https://www.youtube.com/channel/UCle6xQNwROzwYfYMyrnIcBQ)**

Building
---------
##### Prerequisites

Ubuntu / Debian / Raspbian

```sh
sudo apt install build-essential cmake pkg-config libserialport-dev
```

Windows

[VS2022](https://visualstudio.microsoft.com/vs/)  
[premake5](https://premake.github.io/download)  
[libserialport](https://github.com/sigrokproject/libserialport/archive/refs/heads/master.zip)  


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

Run premake5.bat  
Open YACardEmu.sln and build solution (debug or release).

Running
---------

You must have a USB to RS232 (or a physical serial port) connected to your machine. Configuration is made via both a `config.ini` file & via a web portal / API.
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