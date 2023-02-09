/*
    YACardEmu
    ----------------
    Copyright (C) 2020-2023 wutno (https://github.com/GXTX)
    Copyright (C) 2022-2023 tugpoat (https://github.com/tugpoat)


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

#ifndef C1231BR_H
#define C1231BR_H

#include <atomic>
#include <ctime>
#include <fstream>
#include <iostream>
#include <vector>

#include "CardIo.h"

class C1231BR : public CardIo {
public:
	C1231BR(CardIo::Settings* settings);

protected:
	enum class CardPosition {
		NO_CARD        = 0b00000,
		POS_MAG        = 0b11000,
		POS_THERM      = 0b00111,
		POS_THERM_DISP = 0b11100,
		POS_IN_FRONT   = 0b00001,
	};

	struct LocalStatus {
		bool shutter = true;
		bool dispenser = true; // report full dispenser
		CardPosition position = CardPosition::NO_CARD;

		uint8_t GetByte()
		{
			return 1 << (shutter ? 7 : 6) | (dispenser ? 1 : 0) << 5 | static_cast<uint8_t>(position);
		}
	};

	LocalStatus localStatus{};
	bool HasCard() override;
	void DispenseCard() override;
	void EjectCard() override;
	void UpdateRStatus() override;
	uint8_t GetRStatus() override;

	void Command_10_Initalize() override;
	void Command_D0_ShutterControl() override;

	void MoveCard(CardIo::MovePositions position) override;
	CardIo::MovePositions GetCardPos() override;
};

#endif // C1231BR
