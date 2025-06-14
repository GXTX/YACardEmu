/*
    YACardEmu
    ----------------
    Copyright (C) 2020-2025 wutno (https://github.com/GXTX)
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
	~C1231BR() { EjectCard(); }
protected:
	// True "R" values
	static constexpr std::array<uint8_t, static_cast<size_t>(R::MAX_POSITIONS)> positionValues{ {
		0b00000,
		0b00001,
		0b11000,
		0b00111,
		0b11100
	} };

	// Specific to this model
	bool shutter = true;

	uint8_t GetPositionValue() override
	{
		return 1 << (shutter ? 7 : 6) | (!m_cardSettings->reportDispenserEmpty ? 1 : 0) << 5 | positionValues[static_cast<uint8_t>(status.r)];
	}

	void ProcessNewPosition() override;

	// We have special requirements for these commands
	void Command_10_Initalize() override;
	void Command_D0_ShutterControl() override;
};

#endif // C1231BR
