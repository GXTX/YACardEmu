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
    C1231LR(CardIo::Settings *settings);
    ~C1231LR() { EjectCard(); }
protected:
    // True "R" values
    static constexpr std::array<uint8_t, static_cast<size_t>(R::MAX_POSITIONS)> positionValues{ {
        0x30,
        0x34,
        0x31,
        0x32,
        0x33
    } };

    uint8_t GetPositionValue() override
    {
        return positionValues[static_cast<uint8_t>(status.r)];
    }

    void ProcessNewPosition() override;

    bool HasCard() override
    {
        return (status.r != R::NO_CARD && status.r != R::EJECT);
    }
};

#endif //C1231LR
