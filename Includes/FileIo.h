/*
    YACardEmu
    ----------------
    Copyright (C) 2025 wutno (https://github.com/GXTX)


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
#include <iostream>

#ifndef FILEIO_H
#define FILEIO_H

#include "CardIo.h"

#include "ghc/filesystem.hpp"

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/fmt/bin_to_hex.h"

extern std::shared_ptr<spdlog::async_logger> g_logger;

class FileIo
{
public:
    std::string* m_fullPath = nullptr;
    FileIo(std::string* fullPath) {
        m_fullPath = fullPath;
    }
    ~FileIo() { /*Write();*/ }

    static constexpr uint8_t CARD_SIZE = 0xCF;
    static constexpr uint8_t TRACK_SIZE = 0x45;

    std::vector<std::vector<uint8_t>> m_data{ {}, {}, {} };
    std::vector<std::vector<uint8_t>> m_backupData{ {}, {}, {} };

    bool Read();
    bool Write();
};

#endif // FILEIO_H