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

#include "C1231R.h"

C1231R::C1231R(CardIo::Settings* settings) : C1231BR(settings)
{
}

void C1231R::Command_494E_Initialize()
{
}

void C1231R::Command_4341_Cancel()
{
}

void C1231R::Command_4F54_Eject()
{
	if (HasCard()) {
		EjectCard();
	}
}

void C1231R::Command_4849_Dispense()
{
	if (!HasCard()) {
		DispenseCard();
	}
}

void C1231R::Command_434C_Clean()
{
	switch (currentStep) {
		case 0:
			if (!HasCard()) {
				localStatus.localError = true;
				localStatus.localErrorCode = 0x05;
				currentStep--;
			}
			break;
		case 1:
			MoveCard(MovePositions::THERMAL_HEAD);
			break;
		case 2:
			EjectCard();
			break;
		default:
			break;
	}

	if (currentStep > 2) {
		runningCommand = false;
	}
}

void C1231R::Command_5254_UNK()
{
}

void C1231R::Command_525C_Read()
{
}

void C1231R::Command_574C_Write()
{
}

void C1231R::Command_5353_ReadStatus()
{
	std::vector<uint8_t> lstatus{ localStatus.GetByte() };
	std::copy(lstatus.begin(), lstatus.end(), std::back_inserter(commandBuffer));
}

void C1231R::HandlePacket()
{
	//if (localCommand == 0x5353) {
	//	Command_5353_ReadStatus();
	//}
	if (runningCommand) {
		switch (localCommand) {
			case 0x494E: Command_494E_Initialize(); break;
			case 0x4341: Command_4341_Cancel(); break;
			case 0x4F54: Command_4F54_Eject(); break;
			case 0x4849: Command_4849_Dispense(); break;
			case 0x434C: Command_434C_Clean(); break;
			//case 0x5254: Command_5254_UNK(); break;
			case 0x525C: Command_525C_Read(); break;
			case 0x544C: Command_574C_Write(); break;
			case 0x5353: Command_5353_ReadStatus(); break;
			default:
				spdlog::warn("CardIo::HandlePacket: Unhandled command {0:X}", localCommand);
				//SetSError(S::ILLEGAL_COMMAND);
				//N1 - ERROR_COMMAND / Connection Error
				break;
		}
		currentStep++;
	}
}

CardIo::StatusCode C1231R::ReceivePacket(std::vector<uint8_t>& readBuffer)
{
	spdlog::debug("CardIo::ReceivePacket: ");
	constexpr static uint8_t offset = 3;
	uint8_t* buffer = &readBuffer[0];

	// First, read the sync byte
	uint8_t sync = GetByte(&buffer);

	if (sync != START_OF_TEXT) {
		spdlog::warn("Missing STX!");
		readBuffer.erase(readBuffer.begin()); // SLOW!
		return SyncError;
	}

	// Smallest packet size
	if (readBuffer.size() < 5) {
		return SyntaxError;
	}

	// We don't have a count byte, so we need to know early what command we're working with
	localCommand = (GetByte(&buffer) << 8) | GetByte(&buffer); //buffer[readBuffer[2]]
	uint8_t count = m_commandLengths[localCommand];
	//readBuffer[2]
	if (readBuffer.size() - offset - 2 < count) {
		spdlog::debug("Waiting for more data");
		return SizeError;
	}

	if (readBuffer.at(count + offset) != END_OF_TEXT) {
		spdlog::debug("Missing ETX!");
		readBuffer.erase(readBuffer.begin(), readBuffer.begin() + count + offset);
		return SyntaxError;
	}

	// Checksum is calcuated by xoring the entire packet excluding the start
	uint8_t actual_checksum = 0;

	// Clear previous packet
	currentPacket.clear();

	actual_checksum ^= localCommand >> 8;
	currentPacket.push_back(localCommand >> 8);
	actual_checksum ^= localCommand & 0xFF;
	currentPacket.push_back(localCommand & 0xFF);

	// Decode the payload data
	for (int i = 0; i < count; i++) { // NOTE: -2 to ignore stx & sum
		uint8_t value = GetByte(&buffer);
		currentPacket.push_back(value);
		actual_checksum ^= value;
	}

	// Should be our ETX
	actual_checksum ^= GetByte(&buffer);

	// Read the checksum from the last byte
	uint8_t packet_checksum = GetByte(&buffer);

	// Clear out the part of the buffer we've handled.
	readBuffer.erase(readBuffer.begin(), readBuffer.begin() + count + offset + 2);

	// Verify checksum - skip packet if invalid
	if (packet_checksum != actual_checksum) {
		spdlog::warn("Read checksum bad! {0:X} {1:X}", packet_checksum, actual_checksum);
		//N1 - ERROR_COMMAND / Connection Error
		readBuffer.clear();
		return ChecksumError;
	}

	spdlog::debug("{:Xn}", spdlog::to_hex(currentPacket));

	currentCommand = currentPacket[0];

	// Remove the current command, we don't need it
	currentPacket.erase(currentPacket.begin(), currentPacket.begin() + 1);

	// TODO: Do all of this below better...
	status.SoftReset();
	status.s = S::RUNNING_COMMAND;
	runningCommand = true;
	currentStep = 0;

	commandBuffer.clear();

	HandlePacket();
	if (localCommand == 0x434C) {
		return SyntaxError;
	}

	return ServerWaitingReply;
}

CardIo::StatusCode C1231R::BuildPacket(std::vector<uint8_t>& writeBuffer)
{
	spdlog::debug("CardIo::BuildPacket: ");

	std::vector<uint8_t> tempBuffer{};
	if (localStatus.localError) {
		// FIXME: Add real error code
		tempBuffer = { 'N', static_cast<uint8_t>(localStatus.localErrorCode + 0x30) };
	} else if (commandBuffer.empty() || localCommand == 0x574C) {
		tempBuffer = { 'O', 'K' };
	}

	std::copy(commandBuffer.begin(), commandBuffer.end(), std::back_inserter(tempBuffer));
	tempBuffer.emplace_back(END_OF_TEXT);

	uint8_t packet_checksum = 0;
	for (const uint8_t n : tempBuffer) {
		packet_checksum ^= n;
	}

	tempBuffer.insert(tempBuffer.begin(), START_OF_TEXT);
	if (!localStatus.localError) {
		tempBuffer.insert(tempBuffer.begin(), ACK);
	} else {
		tempBuffer.insert(tempBuffer.begin(), 0x06);
		//localStatus.localError = false;
	}
	tempBuffer.emplace_back(packet_checksum);

	std::copy(tempBuffer.begin(), tempBuffer.end(), std::back_inserter(writeBuffer));

	spdlog::debug("{:Xn}", spdlog::to_hex(writeBuffer));

	return Okay;
}
