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

#include "C1231LR.h"

C1231LR::C1231LR(CardIo::Settings* settings) : CardIo(settings)
{
}

void C1231LR::ProcessNewPosition()
{
	// We "grab" the card for the user
	if (status.r == R::EJECT) {
		m_cardSettings->insertedCard = false;
		m_cardSettings->hasCard = false;
		MoveCard(R::NO_CARD);
	}

	// We require the user to "insert" a card if we're waiting
	if (m_cardSettings->insertedCard && status.r == R::NO_CARD) {
		ReadCard();
		MoveCard(R::READ_WRITE_HEAD);

		if (runningCommand && status.s == S::WAITING_FOR_CARD) {
			status.s = S::RUNNING_COMMAND;
		}
	}
}
