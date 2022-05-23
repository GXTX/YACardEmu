/*
    YACardEmu
    ----------------
    Copyright (C) 2020-2022 wutno (https://github.com/GXTX)


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

C1231LR::C1231LR(bool *insertedCard, std::string *basePath, std::string *cardName, bool *dispenserStatus) : CardIo(insertedCard, basePath, cardName, dispenserStatus)
{
}

void C1231LR::UpdateRStatus()
{
	// We "grab" the card for the user
	if (localStatus == LR::EJECTING_CARD) {
		*insertedCard = false;
		localStatus = LR::NO_CARD;
	}

	// We require the user to "insert" a card if we're waiting
	if (*insertedCard && localStatus == LR::NO_CARD) {
		localStatus = LR::HAS_CARD_1;

		if (runningCommand && status.s == S::WAITING_FOR_CARD) {
			status.s = S::RUNNING_COMMAND;
		}
	}
}

bool C1231LR::HasCard()
{
	return localStatus == LR::HAS_CARD_1;
}

void C1231LR::DispenseCard()
{
	if (localStatus != LR::NO_CARD) {
		SetSError(S::ILLEGAL_COMMAND);
	} else {
		localStatus = LR::HAS_CARD_1;
	}
}

void C1231LR::EjectCard()
{
	if (localStatus == LR::HAS_CARD_1) {
		localStatus = LR::EJECTING_CARD;
	}
}

uint8_t C1231LR::GetRStatus()
{
	return static_cast<uint8_t>(localStatus);
}
