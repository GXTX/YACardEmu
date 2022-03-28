YACardEmu
============

YACardEmu (*YetAnother*CardEmu) is a program to emulate the Sanwa CRP-1231LR-10NAB card reader that's found on Wangan Midnight Tune 1 and 2 arcade machines by Namco. A longer goal of this project is to be compatible with newer and older models that line of readers.

**Checkout some gameplay videos [here!](https://www.youtube.com/channel/UCle6xQNwROzwYfYMyrnIcBQ)**

Building
---------
##### Prerequisites

Ubuntu / Debian / Raspbian

```sh
sudo apt install build-essential cmake pkg-config libserialport-dev
```

##### Build

```
git clone --recursive https://github.com/GXTX/YACardEmu
cd YACardEmu
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

Running
---------

You must have a USB to RS242 (or a physical serial port) connected to your machine. Configuration is made via both a `config.ini` file & via a web portal / API.
To access the web portal point your browser to `http://YOURIPHERE:8080/`, this web page is where you'll choose your card and insert your card.

Editing `config.ini` to point to where you want your cards stored is required. The program *must* be ran as root.

```
cp ../config.ini.sample config.ini
sudo ./YACardEmu
```
