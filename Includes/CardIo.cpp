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

#include "CardIo.h"

#define DEBUG_CARD_PACKETS

CardIo::CardIo(std::atomic<bool> *insert)
{
	insertedCard = insert;
}

void CardIo::Command_10_Initalize()
{
	switch (currentStep) {
		case 1:
			if (status.r == R::HAS_CARD_1) {
				status.r = R::EJECTING_CARD;
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

void CardIo::Command_33_ReadData2()
{
	enum Mode {
		Standard = 0x30, // read 69-bytes
		ReadVariable = 0x31, // variable length read, 1-47 bytes
		CardCapture = 0x32, // pull in card?
	};

	switch (currentStep) {
	case 1:
		if (mode == static_cast<uint8_t>(Mode::CardCapture)) { // don't reply any card info if we get this
			if (status.r != R::HAS_CARD_1) {
				status.s = S::WAITING_FOR_CARD; // FIXME: is this correct?
				currentStep--;
			}
		} else {
			if (status.r != R::HAS_CARD_1) {
				status.s = S::ILLEGAL_COMMAND; // FIXME: is this correct?
			} else {
				status.s = S::RUNNING_COMMAND; // clean up from above
				if (cardData.empty()) {
					std::puts("load\n");
					LoadCardFromFS();
				}
				std::copy(cardData.begin(), cardData.end(), std::back_inserter(commandBuffer));
			}
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

void CardIo::Command_53_WriteData2(std::vector<uint8_t> &packet)
{
	enum Mode {
		Standard     = 30, // write 69-bytes
		ReadVariable = 31, // variable length write, 1-47 bytes
		CardCapture  = 32, // pull in card?
	};

	enum BitMode {
		SevenBitParity   = 30,
		EightBitNoParity = 31,
	};

	enum Track {
		Track_1 = 30,
		Track_2 = 31,
		Track_3 = 32,
		Track_1_And_2 = 33,
		Track_1_And_3 = 34,
		Track_2_And_3 = 35,
		Track_1_2_And_3 = 36,
	};

	Mode mode = static_cast<Mode>(packet[0]);
	BitMode bitMode = static_cast<BitMode>(packet[1]);
	Track trackMode = static_cast<Track>(packet[2]);

	// TODO: transmit card data
	return;
}

void CardIo::Command_7D_Erase()
{
	if (currentStep == 1 && status.r != R::HAS_CARD_1) {
		status.s = S::ILLEGAL_COMMAND; // FIXME: Verify
	}

	if (currentStep > 1) {
		runningCommand = false;
	}
}

void CardIo::Command_80_EjectCard()
{
	switch (currentStep) {
		case 1:
			if (status.r == R::HAS_CARD_1) {
				status.r = R::EJECTING_CARD;
			} else {
				status.s = S::ILLEGAL_COMMAND; // FIXME: Is this correct?
			}
			break;
		default:
			break;
	}

	if (currentStep > 1) {
		runningCommand = false;
	}
}

void CardIo::Command_A0_Clean()
{
	switch (currentStep) {
		case 1:
			if (status.r == R::NO_CARD) {
				status.s = S::WAITING_FOR_CARD;
				currentStep--;
			}
			break;
		case 2:
			status.s = S::RUNNING_COMMAND; // TODO: don't set this here
			break;
		case 3:
			status.r = R::EJECTING_CARD;
			break;
		default: 
			break;
	}

	if (currentStep > 3) {
		runningCommand = false;
	}

	return;
}

void CardIo::Command_B0_DispenseCardS31()
{
	enum Mode {
		Dispense = 0x31,
		CheckOnly = 0x32, // check status of dispenser
	};

	if (mode == static_cast<uint8_t>(Mode::Dispense)) {
		if (status.s != S::ILLEGAL_COMMAND) {
			switch (currentStep) {
				case 1:
					if (status.r == R::HAS_CARD_1) {
						status.s = S::ILLEGAL_COMMAND;
					} else {
						status.r = R::HAS_CARD_1;
					}
					break;
				default:
					break;
			}
		}

		if (currentStep > 1) {
			runningCommand = false;
		}
	} else {
		// FIXME: Verify
		status.s = S::CARD_FULL;
		runningCommand = false;
	}

	return;
}

void CardIo::PutStatusInBuffer()
{
	ResponseBuffer.insert(ResponseBuffer.begin(), static_cast<uint8_t>(currentCommand));

	ResponseBuffer.insert(ResponseBuffer.begin()+1, static_cast<uint8_t>(status.r));
	ResponseBuffer.insert(ResponseBuffer.begin()+2, static_cast<uint8_t>(status.p));
	ResponseBuffer.insert(ResponseBuffer.begin()+3, static_cast<uint8_t>(status.s));
}

void CardIo::LoadCardFromFS()
{
	// FIXME: Create a card if one doesn't exist. Find a better way to do this, gdb running as sudo can't find the file unless it's full path

	std::ifstream card(cardName.c_str(), std::ifstream::in | std::ifstream::binary | std::ifstream::ate);
	std::string readBack(CARD_SIZE, 0);
	int size = 0;

	if (card.good() && card.tellg() == CARD_SIZE && cardData.empty()) {
		size = card.tellg();
		card.seekg(std::ifstream::beg);
		card.read(&readBack[0], size);
		card.close();
		std::copy(readBack.begin(), readBack.end(), std::back_inserter(cardData));
		std::copy(readBack.begin(), readBack.end(), std::back_inserter(backupCardData));
	}

	return;
}

void CardIo::SaveCardToFS()
{
	std::ofstream card;
	std::string writeBack{};

	std::copy(cardData.begin(), cardData.end(), std::back_inserter(writeBack));

	if (!writeBack.empty()) {
		card.open(cardName, std::ofstream::out | std::ofstream::binary);
		card.write(writeBack.c_str(), writeBack.size());
		card.close();
	}

	return;
} 

void CardIo::HandlePacket(std::vector<uint8_t> &packet)
{
	if (!runningCommand) {
		//if (status.r == R::EJECTING_CARD) {
		//	*insertedCard = false;
		//	status.r = R::NO_CARD;
		//}

		if (status.s != S::NO_JOB && status.s != S::ILLEGAL_COMMAND) {
			status.s = S::NO_JOB;
		}
	}
	
	// We "grab" the card for the user
	if (status.r == R::EJECTING_CARD) {
		*insertedCard = false;
		status.r = R::NO_CARD;
	}

	// We require the user to "insert" a card if we're waiting
	if (*insertedCard && status.s == S::WAITING_FOR_CARD) {
		status.r = R::HAS_CARD_1;
	}

	//if (status.r == R::HAS_CARD_1 && cardData.empty()) {
	//	LoadCardFromFS();
	//}

	if (runningCommand && currentStep == 0) {
		// Get S::RUNNING_COMMAND to be the first response always
		currentStep++;
			std::printf("\n\ncurrentCommand: %X, currentStep: %d, status.r: %X, status.p: %X, status.s: %X, runningCommand: %d, mnode: %X\n",
	currentCommand, currentStep, static_cast<uint8_t>(status.r), 
	static_cast<uint8_t>(status.p), static_cast<uint8_t>(status.s), runningCommand, mode);
		return;
	} else if (runningCommand) {
		switch (currentCommand) {
			case 0x10: mode = packet[0]; Command_10_Initalize(); break;
			case 0x20: runningCommand = false; break; // We don't need to do anything special here
			case 0x33: mode = packet[0]; Command_33_ReadData2(); break;
			case 0x40: runningCommand = false; break; // We don't need to do anything special here
			case 0x53: Command_53_WriteData2(packet); break;
			case 0x78: runningCommand = false; break; // TODO: Implement, reply status is fine
			case 0x7C: runningCommand = false; break; // TODO: Implement, reply status is fine
			case 0x7D: Command_7D_Erase(); return;
			case 0x80: Command_80_EjectCard(); break;
			case 0xA0: Command_A0_Clean(); break;
			case 0xB0: mode = packet[0]; Command_B0_DispenseCardS31(); break;
			default:
				std::printf("CardIo::HandlePacket: Unhandled Command %02X\n", packet[0]);
				runningCommand = false;
				status.s = S::ILLEGAL_COMMAND;
				break;
		}
		currentStep++;
	}

	std::printf("\n\ncurrentCommand: %X, currentStep: %d, status.r: %X, status.p: %X, status.s: %X, runningCommand: %d, mode: %X\n",
	currentCommand, currentStep, static_cast<uint8_t>(status.r), 
	static_cast<uint8_t>(status.p), static_cast<uint8_t>(status.s), runningCommand, mode);
}

uint8_t CardIo::GetByte(uint8_t **buffer)
{
	const uint8_t value = (*buffer)[0];
	*buffer += 1;

	return value;
}

CardIo::StatusCode CardIo::ReceivePacket(std::vector<uint8_t> &readBuffer)
{
	std::cout << "CardIo::ReceivePacket:";

	uint8_t *buffer = &readBuffer[0];

	// First, read the sync byte
	uint8_t sync = GetByte(&buffer);

	if (sync == ENQUIRY) {
		std::cout << " ENQ\n";
		readBuffer.erase(readBuffer.begin());
		HandlePacket(currentPacket);
		return ServerWaitingReply;
	} else if (sync != START_OF_TEXT) {
		std::cerr << " Missing STX!\n";
		status.s = S::ILLEGAL_COMMAND;
		readBuffer.clear();
		return SyncError;
	}

	uint8_t count = GetByte(&buffer);

	if (count > readBuffer.size() - 1) { // Count counts itself but we still have STX
		std::cout << " Waiting for more data\n";
		return SizeError;
	}

	if (readBuffer.at(count) != END_OF_TEXT) {
		std::cout << " Missing ETX!\n";
		status.s = S::ILLEGAL_COMMAND;
		readBuffer.clear();
		return SyntaxError;
	}

	// Checksum is calcuated by xoring the entire packet excluding the start and the end
	uint8_t actual_checksum = count;

	// Clear previous packet
	currentPacket.clear();

	// Decode the payload data
	for (int i = 0; i < (count - 1); i++) { // NOTE: -1 to ignore sum byte
		uint8_t value = GetByte(&buffer);
		currentPacket.push_back(value);
		actual_checksum ^= value;
	}

	// Read the checksum from the last byte
	uint8_t packet_checksum = GetByte(&buffer);

	currentPacket.pop_back(); // Remove the END_OF_TEXT

	// Clear out the part of the buffer we've handled.
	readBuffer.erase(readBuffer.begin(), readBuffer.begin() + count + 2);

	// Verify checksum - skip packet if invalid
	if (packet_checksum != actual_checksum) {
		std::cerr << " Checksum error!\n";
		status.s = S::ILLEGAL_COMMAND;
		return ChecksumError;
	}

	for (const uint8_t n : currentPacket) {
		std::printf(" %02X", n);
	}
	std::cout << "\n";

	currentCommand = currentPacket[0];

	// Remove the current command and the masters status bytes, we don't need it
	currentPacket.erase(currentPacket.begin(), currentPacket.begin()+4);

	//status.SoftReset();

	status.s = S::RUNNING_COMMAND;
	runningCommand = true;
	currentStep = 0;
	mode = 0x30;
	commandBuffer.clear();

	return Okay;
}

CardIo::StatusCode CardIo::BuildPacket(std::vector<uint8_t> &writeBuffer)
{
	std::cout << "CardIo::BuildPacket:";

	std::copy(commandBuffer.begin(), commandBuffer.end(), std::back_inserter(ResponseBuffer));

	PutStatusInBuffer();

	uint8_t count = (ResponseBuffer.size() + 2) & 0xFF; // FIXME: +2 why?

	// Ensure our outgoing buffer is empty.
	if (!writeBuffer.empty()) {
		writeBuffer.clear();
	}

	// Send the header bytes
	writeBuffer.emplace_back(START_OF_TEXT);
	writeBuffer.emplace_back(count);

	// Calculate the checksum
	uint8_t packet_checksum = count;

	// Encode the payload data
	for (const uint8_t n : ResponseBuffer) {
		writeBuffer.emplace_back(n);
		packet_checksum ^= n;
	}

	writeBuffer.emplace_back(END_OF_TEXT);
	packet_checksum ^= END_OF_TEXT;

	// Write the checksum to the last byte
	writeBuffer.emplace_back(packet_checksum);

	ResponseBuffer.clear();

	for (const uint8_t n : writeBuffer) {
		std::printf(" %02X", n);
	}
	std::cout << "\n";

	return Okay;
}
