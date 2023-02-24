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

#ifndef C1231R_H
#define C1231R_H

#include <vector>
#include <iostream>
#include <fstream>
#include <atomic>
#include <ctime>
#include <map>
#include <bitset>

#include "CardIo.h"
#include "C1231BR.h"

class C1231R : public C1231BR
{
public:
    C1231R(CardIo::Settings* settings);

    CardIo::StatusCode BuildPacket(std::vector<uint8_t>& readBuffer) override;
    CardIo::StatusCode ReceivePacket(std::vector<uint8_t>& writeBuffer) override;
protected:
    struct LocalStatus: virtual C1231BR::LocalStatus {
        bool localError = false;
        uint8_t localErrorCode = 0;
        bool shutter = true;
        bool dispenser = true; // report full dispenser
        CardPosition position = CardPosition::NO_CARD;

        std::vector<uint8_t> status{};

        const std::vector<uint8_t>& GetByte()
        {
            status.clear();
            std::bitset<6> bitT = (static_cast<uint8_t>(position) << 5) | dispenser;
            for (auto i = 5; i > -1; i--) {
                status.emplace_back(bitT[i] + 0x30);
            }
            return status;
        }
    } localStatus;

    // COMMAND, LENGTH
    std::map<uint16_t, uint8_t> m_commandLengths{
        {0, 0},
        {0x494E, 0},
        {0x4341, 0},
        {0x4F54, 1}, // []0
        {0x4859, 0},
        {0x434C, 0},
        {0x5254, 1}, // []5
        {0x524C, 0},
        {0x574C, TRACK_SIZE},
        {0x5353, 0},
    };

    void HandlePacket() override;
    void Command_494E_Initialize();
    void Command_4341_Cancel();
    void Command_4F54_Eject();
    void Command_4849_Dispense();
    void Command_434C_Clean();
    void Command_5254_UNK();
    void Command_525C_Read();
    void Command_574C_Write();
    void Command_5353_ReadStatus();

    uint16_t localCommand{};
    bool localError = false;
};

#endif //C1231R
