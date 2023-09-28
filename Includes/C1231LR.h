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

#ifndef C1231LR_H
#define C1231LR_H

#include <vector>
#include <iostream>
#include <fstream>
#include <atomic>
#include <ctime>

#include "CardIo.h"

class C1231LR : public CardIo
{
public:
    C1231LR(CardIo::Settings* settings);
protected:
    enum class Position : uint8_t {
        NO_CARD           = '0',
        READ_WRITE_HEAD   = '1',
        THERMAL_HEAD      = '2',
        DISPENSER_THERMAL = '3',
        EJECT             = '4',
    } override;

    struct LocalStatus {
        Position* position = nullptr;

        uint8_t GetByte() {
            return static_cast<uint8_t>(*position);
        }
    };

    LocalStatus localStatus = {};

    void DispenseCard() override;
    void EjectCard() override;
    void UpdateState() override;
    uint8_t GetPositionByte() override;
};

#endif //C1231LR
