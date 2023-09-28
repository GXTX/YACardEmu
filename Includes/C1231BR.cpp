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

#include "C1231BR.h"

C1231BR::C1231BR(CardIo::Settings* settings) : CardIo(settings)
{
	localStatus.position = reinterpret_cast<C1231BR::Position *>(&status.position);
	*localStatus.position = Position::NO_CARD;
}

void C1231BR::UpdateState()
{
	// FIXME: We shouldn't be checking commands here, but F-Zero AX requires the user NOT to insert the card completely, rather, it appears D0 or 33 is pulling in the card
	if (currentCommand != 0xD0 && currentCommand != 0x33) {
		// We "grab" the card for the user
		if (*localStatus.position == Position::EJECT) {
			m_cardSettings->insertedCard = false;
			*localStatus.position = Position::NO_CARD;
		}
	}

	// We require the user to "insert" a card if we're waiting
	if (m_cardSettings->insertedCard && *localStatus.position == Position::NO_CARD) {
		ReadCard();
		// We're not actually ejecting here, we're presenting the card to the machine with POS_IN_FRONT
		*localStatus.position = Position::EJECT;

		if (runningCommand && status.s == S::WAITING_FOR_CARD) {
			// Make sure the user can actually insert a card
			localStatus.shutter = true;
			status.s = S::RUNNING_COMMAND;
		}
	}
}

void C1231BR::DispenseCard()
{
	*localStatus.position = Position::DISPENSER_THERMAL;
}

void C1231BR::EjectCard()
{
	if (*localStatus.position != Position::NO_CARD) {
		localStatus.shutter  = true;
		*localStatus.position = Position::EJECT;
		WriteCard();
	}
}

uint8_t C1231BR::GetPositionByte()
{
	return localStatus.GetByte();
}

void C1231BR::Command_10_Initalize()
{
	enum Mode {
		Standard            = 0x30, // We actually eject anyway..
		EjectAfter          = 0x31,
		ResetSpecifications = 0x32,
	};

	if (static_cast<Mode>(currentPacket[0]) == Mode::EjectAfter) {
		localStatus.shutter = true;
	}
	EjectCard();

	runningCommand = false;
}

void C1231BR::Command_D0_ShutterControl()
{
	// Only actions are to close and open the shutter
	enum Action {
		Close = 0x30,
		Open  = 0x31,
	};

	localStatus.shutter = (static_cast<Action>(currentPacket[0]) == Action::Open) ? true : false;

	runningCommand = false;
}
