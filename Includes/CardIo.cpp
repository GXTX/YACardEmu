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

	std::time_t startTime = std::time(nullptr);

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
			if (HasCard()) {
				EjectCard();
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
				if (!HasCard()) {
					status.s = S::WAITING_FOR_CARD;
					currentStep--;
				}
			} else {
				if (HasCard()) {
					status.s = S::RUNNING_COMMAND; // TODO: don't set this here, cleanup from above

					// Clear the tracks that the machine is trying to read, we may have left over data
					if (track < Track::Track_1_And_2) {
						cardData.at(static_cast<size_t>(track) - 0x30).clear();
					} else if (track == Track::Track_1_2_And_3) {
						cardData.clear();
						cardData.resize(NUM_TRACKS);
					} else {
						SetPError(P::ILLEGAL_ERR); // We don't support any other methods for now
						return;
					}

					std::string fullPath(basePath->c_str());
					fullPath.append(cardName->c_str());
					std::string readBack(CARD_SIZE, 0);

					if (std::filesystem::exists(fullPath.c_str()) &&
							std::filesystem::file_size(fullPath.c_str()) == CARD_SIZE) {
						std::ifstream card(fullPath.c_str(), std::ifstream::in | std::ifstream::binary);
						std::string readBack(CARD_SIZE, 0);

						if (track == Track::Track_1_2_And_3) {
							card.read(&readBack[0], CARD_SIZE);
							for (int i = 0; i < 3; i++) {
								std::copy(readBack.begin() + (i * TRACK_SIZE), readBack.begin() + (i * TRACK_SIZE + TRACK_SIZE), std::back_inserter(cardData.at(i)));
							}
							std::copy(readBack.begin(), readBack.end(), std::back_inserter(commandBuffer));
						} else {
							uint8_t absTrack = (static_cast<uint8_t>(track) - 0x30);
							uint8_t filePos = absTrack * TRACK_SIZE;
							card.seekg(filePos);
							card.read(&readBack[0], TRACK_SIZE);
							std::copy(readBack.begin(), readBack.begin() + TRACK_SIZE, std::back_inserter(cardData.at(absTrack)));
							std::copy(readBack.begin(), readBack.begin() + TRACK_SIZE, std::back_inserter(commandBuffer));
						}
						card.close();
					} else {
						SetPError(P::READ_ERR);
					}
				} else {
					SetPError(P::ILLEGAL_ERR);
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
		WriteVariable = 0x31, // variable length, 1-47 bytes
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
			if (!HasCard()) {
				SetPError(P::ILLEGAL_ERR);
			} else {
				if (track < Track::Track_1_And_2) {
					cardData.at(static_cast<size_t>(track) - 0x30).clear();
					std::copy(currentPacket.begin() + 3, currentPacket.end(), std::back_inserter(cardData.at(static_cast<size_t>(track) - 0x30)));
				} else if (track == Track::Track_1_2_And_3) {
					cardData.clear();
					cardData.resize(NUM_TRACKS);
					for (int i = 0; i < NUM_TRACKS; i++) {
						std::copy(currentPacket.begin() + 3, currentPacket.begin() + (i * TRACK_SIZE + TRACK_SIZE), std::back_inserter(cardData.at(i)));
					}
				} else {
					SetPError(P::ILLEGAL_ERR); // We don't support any other methods for now
					return;
				}

				std::string writeBack{};

				for (int i = 0; i < NUM_TRACKS; i++) {
					// Fill in null track data if the machine hasn't given us any to make size correct
					if (cardData.at(i).empty()) {
						cardData.at(i).resize(TRACK_SIZE);
					}
					std::copy(cardData.at(i).begin(), cardData.at(i).end(), std::back_inserter(writeBack));
				}

				std::string fullPath(basePath->c_str());
				fullPath.append(cardName->c_str());

				std::ofstream card;
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
			if (!HasCard()) {
				SetPError(P::ILLEGAL_ERR);
			} else {
				if (control == BufferControl::Clear) {
					printBuffer.clear();
				}
				std::copy(currentPacket.begin() + 3, currentPacket.end(), std::back_inserter(printBuffer));
#if 0
				ICUConv conv = ICUConv();
				conv.convertAndPrint(lineOffset, printBuffer);
#endif
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
			if (!HasCard()) {
				SetPError(P::ILLEGAL_ERR);
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
			if (HasCard()) {
				EjectCard();
			} else {
				SetSError(S::ILLEGAL_COMMAND); // FIXME: Is this correct?
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
			if (!HasCard()) {
				status.s = S::WAITING_FOR_CARD;
				currentStep--;
			}
			break;
		case 2:
			status.s = S::RUNNING_COMMAND; // TODO: don't set this here
			break;
		case 3:
			EjectCard();
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
					if (HasCard()) {
						SetSError(S::ILLEGAL_COMMAND);
					} else {
						DispenseCard();
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

void CardIo::Command_E1_SetRTC()
{
	switch (currentStep) {
		default:
			{
				std::stringstream timeStrS{};
				std::string timeStr{};
				std::copy(commandBuffer.begin(), commandBuffer.end(), std::back_inserter(timeStr));

				timeStrS << timeStr;

				std::tm *tempTime{};
				timeStrS >> std::get_time(tempTime, "%y%m%d%H%M%S");
				time = std::mktime(tempTime);
			}
			status.SoftReset();
			runningCommand = false;
			break;
	}
}

void CardIo::Command_F0_GetVersion()
{
	switch (currentStep) {
		default:
			std::copy(versionString.begin(), versionString.end(), std::back_inserter(commandBuffer));
			status.SoftReset();
			runningCommand = false;
			break;
	}
}

void CardIo::Command_F1_GetRTC()
{
	switch (currentStep) {
		default:
			{
				std::string timeStr(12, 0);
				std::time_t currentTime = std::time(nullptr);

				std::time_t convTime{};

				if (time != 0) {
					convTime = time + currentTime - startTime;
				} else {
					convTime = currentTime;
				}

				std::strftime(&timeStr[0], timeStr.size(), "%y%m%d%H%M%S", std::localtime(&convTime));
				std::copy(timeStr.begin(), timeStr.end(), std::back_inserter(commandBuffer));
			}
			status.SoftReset();
			runningCommand = false;
			break;
	}
}

void CardIo::Command_F5_CheckBattery()
{
	switch (currentStep) {
		default:
			// We don't do anything here because we never report a bad battery.
			status.SoftReset();
			runningCommand = false;
			break;
	}
}

void CardIo::SetPError(P error_code)
{
	status.p = error_code;
	runningCommand = false;
}

void CardIo::SetSError(S error_code)
{
	status.s = error_code;
	runningCommand = false;
}

void CardIo::UpdateStatusInBuffer()
{
	//commandBuffer[1] = static_cast<uint8_t>(status.r);
	commandBuffer[1] = GetRStatus();
	commandBuffer[2] = static_cast<uint8_t>(status.p);
	commandBuffer[3] = static_cast<uint8_t>(status.s);

	spdlog::debug("R: {0:X} P: {1:X} S: {2:X}", 
		//static_cast<uint8_t>(status.r),
		GetRStatus(),
		static_cast<uint8_t>(status.p),
		static_cast<uint8_t>(status.s)
	);
}

void CardIo::HandlePacket()
{
	if (!runningCommand) {
		if (status.s != S::ILLEGAL_COMMAND) {
			status.s = S::NO_JOB;
		}

		status.p = P::NO_ERR;
	}

	UpdateRStatus();

	if (runningCommand) { // 20 is a special case, see function
		if (currentStep == 0 && currentCommand != 0x20) {
			// Get S::RUNNING_COMMAND to be the first response always
			currentStep++;
			return;
		} else {
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

	// count counts itself but readBuffer will have both the STX and sum, we need to skip these.
	if (count > readBuffer.size() - 2) {
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
	currentPacket.erase(currentPacket.begin(), currentPacket.begin() + 4);

	status.SoftReset();
	status.s = S::RUNNING_COMMAND;
	runningCommand = true;
	currentStep = 0;

	commandBuffer.clear();
	commandBuffer.emplace_back(currentCommand);
	commandBuffer.emplace_back(GetRStatus());
	commandBuffer.emplace_back(static_cast<uint8_t>(status.p));
	commandBuffer.emplace_back(static_cast<uint8_t>(status.s));
	
	// FIXME: special case, we need to handle this better, but server does NOT accept an ACK from us on eject command
	if (currentCommand == 0x80) {
		return ServerWaitingReply;
	}

	return Okay;
}

CardIo::StatusCode CardIo::BuildPacket(std::vector<uint8_t> &writeBuffer)
{
	spdlog::debug("CardIo::BuildPacket: ");

	UpdateStatusInBuffer();

	uint8_t count = (commandBuffer.size() + 2);

	// Ensure our outgoing buffer is empty.
	writeBuffer.clear();

	// Send the header bytes
	writeBuffer.emplace_back(START_OF_TEXT);
	writeBuffer.emplace_back(count);

	// Calculate the checksum
	uint8_t packet_checksum = count;

	// Encode the payload data
	for (const uint8_t n : commandBuffer) {
		writeBuffer.emplace_back(n);
		packet_checksum ^= n;
	}

	writeBuffer.emplace_back(END_OF_TEXT);

	// Write the checksum to the last byte
	packet_checksum ^= END_OF_TEXT;
	writeBuffer.emplace_back(packet_checksum);

	spdlog::debug("{:Xn}", spdlog::to_hex(writeBuffer));

	return Okay;
}
