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

#include "C1231BR.h"

C1231BR::C1231BR() : CardIo()
{
}

void C1231BR::UpdateRStatus()
{
	// We "grab" the card for the user
	if (localStatus.position == CardPosition::POS_IN_FRONT) {
		cardSettings.insertedCard = false;
		MoveCard(MovePositions::NO_CARD);
	}

	// We require the user to "insert" a card if we're waiting
	if (cardSettings.insertedCard && localStatus.position == CardPosition::NO_CARD) {
		MoveCard(MovePositions::READ_WRITE_HEAD);

		if (runningCommand && status.s == S::WAITING_FOR_CARD) {
			status.s = S::RUNNING_COMMAND;
		}
	}
}

bool C1231BR::HasCard()
{
	return localStatus.position != CardPosition::NO_CARD;
}

void C1231BR::DispenseCard()
{
	localStatus.shutter = true;
	localStatus.position = CardPosition::POS_THERM_DISP;
}

void C1231BR::EjectCard()
{
	if (localStatus.position != CardPosition::NO_CARD) {
		if (!localStatus.shutter) {
			localStatus.shutter = true;
		}
		localStatus.position = CardPosition::POS_IN_FRONT;
	}
}

uint8_t C1231BR::GetRStatus()
{
	return localStatus.GetByte();
}

void C1231BR::Command_A0_Clean()
{
	switch (currentStep) {
		case 1:
			localStatus.shutter = true;
			status.s = S::WAITING_FOR_CARD;
			break;
		case 2:
			MoveCard(MovePositions::EJECT); //insert card
			if (!HasCard()) {
				currentStep--;
			}
			break;
		case 3:
			MoveCard(MovePositions::THERMAL_HEAD);
			break;
		case 4:
			EjectCard();
			break;
		default:
			break;
	}

	if (currentStep > 4) {
		runningCommand = false;
	}
}


void C1231BR::Command_D0_ShutterControl()
{
	// Only actions are to close and open the shutter
	enum Action {
		Close = 0,
		Open = 1,
	};

	Action action = static_cast<Action>(currentPacket[0]);
	switch (currentStep) {
		case 0:
			switch (action) {
				case Action::Close:
					localStatus.shutter = false;
					break;
				case Action::Open:
					localStatus.shutter = true;
					break;
				default:
					break;
			}
			break;
		case 1:
			switch (action) {
				case Action::Close:
					localStatus.shutter = false;
					break;
				case Action::Open:
					localStatus.shutter = true;
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}

	if (currentStep > 1) {
		runningCommand = false;
	}

	return;
}


void C1231BR::MoveCard(CardIo::MovePositions position)
{
	switch (position) {
		case MovePositions::NO_CARD:
			localStatus.position = CardPosition::NO_CARD;
			break;
		case MovePositions::READ_WRITE_HEAD:
			localStatus.position = CardPosition::POS_MAG;
			break;
		case MovePositions::THERMAL_HEAD:
			localStatus.position = CardPosition::POS_THERM;
			break;
		case MovePositions::DISPENSER_THERMAL:
			localStatus.position = CardPosition::POS_THERM_DISP;
			break;
		case MovePositions::EJECT:
			localStatus.position = CardPosition::POS_IN_FRONT;
			break;
		default:
			break;
	}
}

CardIo::MovePositions C1231BR::GetCardPos()
{
	switch (localStatus.position) {
		case CardPosition::NO_CARD:
			return MovePositions::NO_CARD;
		case CardPosition::POS_MAG:
			return MovePositions::READ_WRITE_HEAD;
		case CardPosition::POS_THERM:
			return MovePositions::THERMAL_HEAD;
		case CardPosition::POS_THERM_DISP:
			return MovePositions::DISPENSER_THERMAL;
		case CardPosition::POS_IN_FRONT:
			return MovePositions::EJECT;
		default:
			return MovePositions::NO_CARD;
	}
}

