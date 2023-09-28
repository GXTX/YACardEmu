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

class C1231BR : public CardIo
{
public:
	C1231BR(CardIo::Settings* settings);
protected:
	enum class Position : uint8_t {
		NO_CARD           = 0b00000,
		READ_WRITE_HEAD   = 0b11000,
		THERMAL_HEAD      = 0b00111,
		DISPENSER_THERMAL = 0b11100,
		EJECT             = 0b00001,
	} override;

	struct LocalStatus {
		bool shutter       = true;
		bool dispenser     = true; // report full dispenser
		Position* position = nullptr;

		uint8_t GetByte()
		{
			return 1 << (shutter ? 7 : 6) | (dispenser ? 1 : 0) << 5 | static_cast<uint8_t>(*position);
		}
	};

	LocalStatus localStatus = {};

	void DispenseCard() override;
	void EjectCard() override;
	void UpdateState() override;
	uint8_t GetPositionByte() override;

	void Command_10_Initalize() override;
	void Command_D0_ShutterControl() override;
};

#endif // C1231BR
