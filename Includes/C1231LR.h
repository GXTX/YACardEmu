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
protected:
	enum class CardPosition {
		NO_CARD                 = 0x30,
		POS_MAG                 = 0x31,
		POS_THERM               = 0x32,
		POS_THERM_DISP          = 0x33,
		POS_EJECTED_NOT_REMOVED = 0x34,
	};

    CardPosition localStatus{CardPosition::NO_CARD};
    bool HasCard() override;
    void DispenseCard() override;
    void EjectCard() override;
    void UpdateRStatus() override;
    uint8_t GetRStatus() override;

    void MoveCard(CardIo::MovePositions position) override;
    CardIo::MovePositions GetCardPos() override;
};

#endif //C1231LR
