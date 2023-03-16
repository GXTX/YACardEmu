/*
    YACardEmu
    ----------------
    Copyright (C) 2020-2023 wutno (https://github.com/GXTX)


    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <fstream>
#include <iostream>

#include "ghc/filesystem.hpp"

#define TRACK_SIZE 0x45
#define USAGE std::cerr << "Used to convert old style card images, aka track_0, track_1, track_2 files\nUsage: convert.exe C:\\PATH\\TO\\card.bin.track_0\n";

const std::string empty(TRACK_SIZE, '\0');

int main(int argc, char* argv[])
{
    if (argc != 2) {
        USAGE
        return 1;
    }

    std::string path = argv[1];

    if (!ghc::filesystem::exists(path.c_str()) || ghc::filesystem::file_size(path.c_str()) != TRACK_SIZE || path.find(".track_") == std::string::npos) {
        USAGE
        return 1;
    }

    std::string buffer;

    for (auto i = 0; i < 3; i++) {
        std::string readBack(TRACK_SIZE, '\0');
        auto newPath = path.substr(0, path.size() - 1) + std::to_string(i);
        std::cout << newPath << "\n";
        if (!ghc::filesystem::exists(newPath.c_str())) {
            buffer.append(empty);
            continue;
        }
        std::ifstream card(newPath.c_str(), std::ifstream::in | std::ifstream::binary);
        card.read(&readBack[0], TRACK_SIZE);
        card.close();
        buffer.append(readBack);
    }

    auto combined = "combined.bin";

    std::ofstream card;
    card.open(combined, std::ofstream::out | std::ofstream::binary);
    card.write(buffer.c_str(), buffer.size());
    card.close();

    std::cout << "Completed!\n";

    return 0;
}