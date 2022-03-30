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

CardIo::CardIo(bool *insertedCard, std::string *basePath, std::string *cardName)
{
	this->insertedCard = insertedCard;
	this->basePath = basePath;
	this->cardName = cardName;

#ifdef NDEBUG
	spdlog::set_level(spdlog::level::warn);
#else
	spdlog::set_level(spdlog::level::debug);
#endif
	spdlog::set_pattern("[%^%l%$] %v");
}

void CardIo::Command_10_Initalize()
{
	enum Mode {
		Standard = 0x30, // We actually eject anyway..
		EjectAfter = 0x31,
		ResetSpecifications = 0x32,
	};

	Mode mode = static_cast<Mode>(currentPacket[0]);

	switch (currentStep) {
		case 1:
			if (status.r == R::HAS_CARD_1) {
				status.r = R::EJECTING_CARD;
			}
			// TODO: Reset printer settings
			break;
		default:
			break;
	}

	if (currentStep > 1) {
		runningCommand = false;
	}
}

void CardIo::Command_20_ReadStatus()
{
	switch (currentStep) {
		default:
			status.SoftReset();
			runningCommand = false;
			break;
	}
}

void CardIo::Command_33_ReadData2()
{
	enum Mode {
		Standard = 0x30, // read 69-bytes
		ReadVariable = 0x31, // variable length read, 1-47 bytes
		CardCapture = 0x32, // pull in card?
	};

	enum BitMode {
		SevenBitParity = 0x30,
		EightBitNoParity = 0x31,
	};

	enum Track {
		Track_1 = 0x30,
		Track_2 = 0x31,
		Track_3 = 0x32,
		Track_1_And_2 = 0x33,
		Track_1_And_3 = 0x34,
		Track_2_And_3 = 0x35,
		Track_1_2_And_3 = 0x36,
	};

	Mode mode = static_cast<Mode>(currentPacket[0]);
	BitMode bit = static_cast<BitMode>(currentPacket[1]);

	// TODO: Impliment this, is req for some applications, how are multitrack write/reads handled? is there a break?
	Track track = static_cast<Track>(currentPacket[2]);

	switch (currentStep) {
		case 1:
			if (mode == static_cast<uint8_t>(Mode::CardCapture)) { // don't reply any card info if we get this
				if (status.r != R::HAS_CARD_1) {
					status.s = S::WAITING_FOR_CARD; // FIXME: is this correct?
					currentStep--;
				}
			} else {
				if (status.r == R::HAS_CARD_1) {
					status.s = S::RUNNING_COMMAND; // TODO: don't set this here, cleanup from above
					cardData.clear();
					LoadCardFromFS();
					std::copy(cardData.begin(), cardData.end(), std::back_inserter(commandBuffer));
				} else {
					status.p = P::ILLEGAL_ERR; // FIXME: is this correct?
				}
			}
		break;
		default:
			break;
	}

	if (currentStep > 1) {
		runningCommand = false;
	}
}

void CardIo::Command_40_Cancel()
{
	switch (currentStep) {
		default:
			status.SoftReset();
			runningCommand = false;
			break;
	}
}

void CardIo::Command_53_WriteData2()
{
	enum Mode {
		Standard = 0x30, // 69-bytes
		ReadVariable = 0x31, // variable length, 1-47 bytes
	};

	enum BitMode {
		SevenBitParity = 0x30,
		EightBitNoParity = 0x31,
	};

	enum Track {
		Track_1 = 0x30,
		Track_2 = 0x31,
		Track_3 = 0x32,
		Track_1_And_2 = 0x33,
		Track_1_And_3 = 0x34,
		Track_2_And_3 = 0x35,
		Track_1_2_And_3 = 0x36,
	};

	Mode mode = static_cast<Mode>(currentPacket[0]);
	BitMode bit = static_cast<BitMode>(currentPacket[1]);

	// TODO: Impliment this, is req for some applications, how are multitrack write/reads handled? is there a break?
	Track track = static_cast<Track>(currentPacket[2]);

	switch (currentStep) {
		case 1:
			if (status.r != R::HAS_CARD_1) {
				status.p = P::ILLEGAL_ERR; // FIXME: Verify
			} else {
				cardData.clear();
				// currentPacket still has the mode/bit/track bytes, we need to skip them
				std::copy(currentPacket.begin() + 3, currentPacket.end(), std::back_inserter(cardData));
				SaveCardToFS();
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

void CardIo::Command_78_PrintSettings2()
{
	switch (currentStep) {
		default:
			break;
	}

	if (currentStep > 1) {
		runningCommand = false;
	}
}

void CardIo::Command_7C_PrintL()
{
	enum Mode {
		Wait = 0x30, // is it expected to run this in the background after WriteData2?
		Now = 0x31,
	};

	enum BufferControl {
		Clear = 0x30,
		DontClear = 0x31,
	};

	Mode mode = static_cast<Mode>(currentPacket[0]);
	BufferControl control = static_cast<BufferControl>(currentPacket[1]);
	uint8_t lineOffset = currentPacket[2];

	switch (currentStep) {
		case 1:
			if (status.r != R::HAS_CARD_1) {
				status.p = P::ILLEGAL_ERR; // FIXME: Verify
			} else {
				if (control == BufferControl::Clear) {
					printBuffer.clear();
				}
				std::copy(currentPacket.begin() + 3, currentPacket.end(), std::back_inserter(printBuffer));

				ICUConv conv = ICUConv();
				conv.convertAndPrint(printBuffer);

				// FIXME: Do this better.
				std::ofstream card;
				std::string writeBack{};
				std::string fullPath(basePath->c_str());
				fullPath.append(printName);

				std::copy(printBuffer.begin(), printBuffer.end(), std::back_inserter(writeBack));

				card.open(fullPath, std::ofstream::out | std::ofstream::binary);
				card.write(writeBack.c_str(), writeBack.size());
				card.close();
			}
			break;
		default:
			break;
	}

	if (currentStep > 1) {
		runningCommand = false;
	}
}

void CardIo::Command_7D_Erase()
{
	switch (currentStep) {
		case 1:
			if (status.r != R::HAS_CARD_1) {
				status.p = P::ILLEGAL_ERR; // FIXME: Verify
			}
			break;
		default:
			break;
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
}

void CardIo::Command_B0_DispenseCardS31()
{
	enum Mode {
		Dispense = 0x31,
		CheckOnly = 0x32,
	};

	Mode mode = static_cast<Mode>(currentPacket[0]);

	switch (currentStep) {
		case 1:
			if (mode == Mode::CheckOnly) {
				status.s = S::CARD_FULL;
			} else {
				if (status.s != S::ILLEGAL_COMMAND) {
					if (status.r == R::HAS_CARD_1) {
						status.s = S::ILLEGAL_COMMAND;
					} else {
						status.r = R::HAS_CARD_1;
					}
				}
			}
			break;
		default:
			break;
	}

	if (currentStep > 1) {
		runningCommand = false;
	}
}

void CardIo::Command_F0_GetVersion()
{
	switch (currentStep) {
		case 1:
			std::copy(versionString.begin(), versionString.end(), std::back_inserter(commandBuffer));
			status.SoftReset();
			runningCommand = false;
			break;
		default:
			break;
	}
}

void CardIo::Command_F1_GetRTC()
{
	switch (currentStep) {
		case 1:
			{
				std::time_t t = std::time(nullptr);

				char timeStr[12];
				std::strftime(timeStr, 12, "%y%m%d%H%M%S", std::localtime(&t));

				for (int i = 0; i > 12; i++) {
					commandBuffer.emplace_back(timeStr[i]);
				}

				status.SoftReset();
				runningCommand = false;
			}
			break;
		default:
			break;
	}
}

void CardIo::Command_F5_CheckBattery()
{
	switch (currentStep) {
		default:
			status.SoftReset();
			runningCommand = false;
			break;
	}
}

void CardIo::PutStatusInBuffer()
{
	ResponseBuffer.insert(ResponseBuffer.begin(), static_cast<uint8_t>(currentCommand));

	ResponseBuffer.insert(ResponseBuffer.begin()+1, static_cast<uint8_t>(status.r));
	ResponseBuffer.insert(ResponseBuffer.begin()+2, static_cast<uint8_t>(status.p));
	ResponseBuffer.insert(ResponseBuffer.begin()+3, static_cast<uint8_t>(status.s));

	spdlog::debug("R: {0:X} P: {0:X} S: {0:X}", 
		static_cast<uint8_t>(status.r),
		static_cast<uint8_t>(status.p),
		static_cast<uint8_t>(status.s)
	);
}

void CardIo::LoadCardFromFS()
{
	std::string fullPath(basePath->c_str());
	fullPath.append(cardName->c_str());

	if (std::filesystem::exists(fullPath.c_str()) &&
			std::filesystem::file_size(fullPath.c_str()) == CARD_SIZE) {
		std::ifstream card(fullPath.c_str(), std::ifstream::in | std::ifstream::binary);
		std::string readBack(CARD_SIZE, 0);
		card.read(&readBack[0], CARD_SIZE);
		card.close();
		std::copy(readBack.begin(), readBack.end(), std::back_inserter(cardData));
		std::copy(readBack.begin(), readBack.end(), std::back_inserter(backupCardData));
	} else {
		status.p = P::SYSTEM_ERR; // FIXME: Correct?
	}

	return;
}

void CardIo::SaveCardToFS()
{
	std::ofstream card;
	std::string writeBack{};

	std::string fullPath(basePath->c_str());
	fullPath.append(cardName->c_str());

	std::copy(cardData.begin(), cardData.end(), std::back_inserter(writeBack));

	card.open(fullPath, std::ofstream::out | std::ofstream::binary);
	card.write(writeBack.c_str(), writeBack.size());
	card.close();

	writeBack.clear();

	std::copy(backupCardData.begin(), backupCardData.end(), std::back_inserter(writeBack));

	fullPath.append(".bak");

	card.open(fullPath, std::ofstream::out | std::ofstream::binary);
	card.write(writeBack.c_str(), writeBack.size());
	card.close();

	return;
}

void CardIo::HandlePacket()
{
	if (!runningCommand) {
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

	if (runningCommand && currentStep == 0 && currentCommand != 0x20) { // 20 is a special case, see function
		// Get S::RUNNING_COMMAND to be the first response always
		currentStep++;
		return;
	} else if (runningCommand) {
		switch (currentCommand) {
			case 0x10: Command_10_Initalize(); break;
			case 0x20: Command_20_ReadStatus(); break;
			case 0x33: Command_33_ReadData2(); break;
			case 0x40: Command_40_Cancel(); break;
			case 0x53: Command_53_WriteData2(); break;
			case 0x78: Command_78_PrintSettings2(); break;
			case 0x7C: Command_7C_PrintL(); break;
			case 0x7D: Command_7D_Erase(); break;
			case 0x80: Command_80_EjectCard(); break;
			case 0xA0: Command_A0_Clean(); break;
			case 0xB0: Command_B0_DispenseCardS31(); break;
			// FIXME: These need to not set S::RUNNING_COMMAND;
			case 0xF0: Command_F0_GetVersion(); break;
			case 0xF1: Command_F1_GetRTC(); break;
			case 0xF5: Command_F5_CheckBattery(); break;
			default:
				spdlog::warn("CardIo::HandlePacket: Unhandled command {0:X}", currentCommand);
				status.s = S::ILLEGAL_COMMAND;
				runningCommand = false;
				break;
		}
		currentStep++;
	}
}

uint8_t CardIo::GetByte(uint8_t **buffer)
{
	const uint8_t value = (*buffer)[0];
	*buffer += 1;

	return value;
}

CardIo::StatusCode CardIo::ReceivePacket(std::vector<uint8_t> &readBuffer)
{
	spdlog::debug("CardIo::ReceivePacket: ");

	uint8_t *buffer = &readBuffer[0];

	// First, read the sync byte
	uint8_t sync = GetByte(&buffer);

	if (sync == ENQUIRY) {
		spdlog::debug("ENQ");
		readBuffer.erase(readBuffer.begin());
		HandlePacket();
		return ServerWaitingReply;
	} else if (sync != START_OF_TEXT) {
		spdlog::warn("Missing STX!");
		readBuffer.clear();
		return SyncError;
	}

	uint8_t count = GetByte(&buffer);

	if (count > readBuffer.size() - 1) { // Count counts itself but we still have STX
		spdlog::debug("Waiting for more data");
		return SizeError;
	}

	if (readBuffer.at(count) != END_OF_TEXT) {
		spdlog::debug("Missing ETX!");
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
		spdlog::warn("Read checksum bad!");
		return ChecksumError;
	}

	spdlog::debug("{:Xn}", spdlog::to_hex(currentPacket));

	currentCommand = currentPacket[0];

	// Remove the current command and the masters status bytes, we don't need it
	currentPacket.erase(currentPacket.begin(), currentPacket.begin()+4);

	status.SoftReset();
	status.s = S::RUNNING_COMMAND;
	runningCommand = true;
	currentStep = 0;
	commandBuffer.clear();
	
	// FIXME: special case, we need to handle this better, but server does NOT accept an ACK from us on eject command
	if (currentCommand == 0x80) {
		return ServerWaitingReply;
	}

	return Okay;
}

CardIo::StatusCode CardIo::BuildPacket(std::vector<uint8_t> &writeBuffer)
{
	spdlog::debug("CardIo::BuildPacket: ");

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

	spdlog::debug("{:Xn}", spdlog::to_hex(currentPacket));

	return Okay;
}
