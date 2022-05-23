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

CardIo::CardIo(bool *insertedCard, std::string *basePath, std::string *cardName, bool *dispenserStatus)
{
	this->insertedCard = insertedCard;
	this->basePath = basePath;
	this->cardName = cardName;
	this->dispenserStatus = dispenserStatus;

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

	//Mode mode = static_cast<Mode>(currentPacket[0]);

	switch (currentStep) {
		case 1:
			if (HasCard()) {
				EjectCard();
			}
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

	if (currentPacket.size() < 3) {
		SetPError(P::SYSTEM_ERR);
		return;
	}

	Mode mode = static_cast<Mode>(currentPacket[0]);
	BitMode bit = static_cast<BitMode>(currentPacket[1]);
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
					switch (track) {
						case Track::Track_1:
						case Track::Track_2:
						case Track::Track_3:
							{
								const uint8_t ctrack = static_cast<uint8_t>(track) - 0x30;
								cardData.at(ctrack).clear();
								bool res = ReadTrack(cardData.at(ctrack), ctrack);
								if (!res) {
									SetPError(P::READ_ERR);
									return;
								}
								std::copy(cardData.at(ctrack).begin(), cardData.at(ctrack).end(), std::back_inserter(commandBuffer));
							}
							break;
						case Track::Track_1_And_2:
						case Track::Track_1_And_3:
						case Track::Track_2_And_3:
							{
								// TODO: This feels bad...
								uint8_t ctrack, ctrack1;

								if (track == Track::Track_1_And_2) {
									ctrack = 0; ctrack1 = 1;
								} else if (track == Track::Track_1_And_3) {
									ctrack = 0; ctrack1 = 2;
								} else {
									ctrack = 1; ctrack1 = 2;
								}

								cardData.at(ctrack).clear();
								cardData.at(ctrack1).clear();

								bool res = ReadTrack(cardData.at(ctrack), ctrack);
								res = ReadTrack(cardData.at(ctrack1), ctrack1);
								if (!res) {
									SetPError(P::READ_ERR);
									return;
								}

								std::copy(cardData.at(ctrack).begin(), cardData.at(ctrack).end(), std::back_inserter(commandBuffer));
								std::copy(cardData.at(ctrack1).begin(), cardData.at(ctrack1).end(), std::back_inserter(commandBuffer));
							}
							break;
						case Track::Track_1_2_And_3:
							{
								cardData.clear();
								cardData.resize(NUM_TRACKS);
								for (int i = 0; i < NUM_TRACKS; i++) {
									bool res = ReadTrack(cardData.at(i), i);
									if (!res) {
										SetPError(P::READ_ERR);
										return;
									}
									std::copy(cardData.at(i).begin(), cardData.at(i).end(), std::back_inserter(commandBuffer));
								}
							}
							break;
						default:
							SetPError(P::ILLEGAL_ERR);
							spdlog::warn("Unknown track read option {0:X}", static_cast<uint8_t>(track));
							return;
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

	if (currentPacket.size() < 3) {
		SetPError(P::SYSTEM_ERR);
		return;
	} 

	Mode mode = static_cast<Mode>(currentPacket[0]);
	BitMode bit = static_cast<BitMode>(currentPacket[1]);
	Track track = static_cast<Track>(currentPacket[2]);

	switch (currentStep) {
		case 1:
			if (!HasCard() || mode == Mode::WriteVariable) {
				SetPError(P::ILLEGAL_ERR);
			} else {
				switch (track) {
					case Track::Track_1:
					case Track::Track_2:
					case Track::Track_3:
						{
							const uint8_t ctrack = static_cast<uint8_t>(track) - 0x30;
							cardData.at(ctrack).clear();
							std::copy(currentPacket.begin() + 3, currentPacket.end(), std::back_inserter(cardData.at(ctrack)));
							WriteTrack(cardData.at(ctrack), ctrack);
						}
						break;
					case Track::Track_1_And_2:
					case Track::Track_1_And_3:
					case Track::Track_2_And_3:
						{
							if (currentPacket.size() - 3 < TRACK_SIZE + 1) {
								SetPError(P::SYSTEM_ERR); // FIXME: Should we do this? Or should we just fill in NULL
								return;
							}

							// TODO: This feels bad...
							uint8_t ctrack, ctrack1;

							if (track == Track::Track_1_And_2) {
								ctrack = 0; ctrack1 = 1;
							} else if (track == Track::Track_1_And_3) {
								ctrack = 0; ctrack1 = 2;
							} else {
								ctrack = 1; ctrack1 = 2;
							}

							cardData.at(ctrack).clear();
							cardData.at(ctrack1).clear();

							std::copy(currentPacket.begin() + 3, currentPacket.begin() + 3 + TRACK_SIZE, std::back_inserter(cardData.at(ctrack)));
							std::copy(currentPacket.begin() + 3 + TRACK_SIZE, currentPacket.end(), std::back_inserter(cardData.at(ctrack1)));

							WriteTrack(cardData.at(ctrack), ctrack);
							WriteTrack(cardData.at(ctrack1), ctrack1);
						}
						break;
					case Track::Track_1_2_And_3:
						{
							if (currentPacket.size() - 3 < CARD_SIZE) {
								SetPError(P::SYSTEM_ERR); // FIXME: Should we do this? Or should we just fill in NULL
								return;
							}

							cardData.clear();
							cardData.resize(NUM_TRACKS);
							for (int i = 0; i < NUM_TRACKS; i++) {
								const uint8_t offset = 3 + (i * TRACK_SIZE);
								std::copy(currentPacket.begin() + offset, currentPacket.begin() + offset + TRACK_SIZE, std::back_inserter(cardData.at(i)));
								WriteTrack(cardData.at(i), i);
							}
						}
						break;
					default:
						SetPError(P::ILLEGAL_ERR);
						spdlog::warn("Unknown track write option {0:X}", static_cast<uint8_t>(track));
						return;
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

void CardIo::Command_78_PrintSettings2()
{
	switch (currentStep) {
		default:
			status.SoftReset();
			runningCommand = false;
			break;
	}
}

void CardIo::Command_7A_RegisterFont()
{
	switch (currentStep) {
		default:
			status.SoftReset();
			runningCommand = false;
			break;
	}
}

void CardIo::Command_7B_PrintImage()
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

	if (currentPacket.size() < 3) {
		SetPError(P::SYSTEM_ERR);
		return;
	}

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

void CardIo::Command_7E_PrintBarcode()
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
		case 1: // FIXME: Special for "Transfer Card Data" in MT2EXP, we need 2 S::RUNNING_COMMAND replies
			break;
		case 2:
			if (HasCard()) {
				EjectCard();
			} else {
				SetSError(S::ILLEGAL_COMMAND); // FIXME: Is this correct?
			}
			break;
		default:
			break;
	}

	if (currentStep > 2) {
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
		case 2: // Force a RUNNING_JOB only response
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

	if (currentPacket.empty()) {
		SetSError(S::ILLEGAL_COMMAND);
		return;
	}

	Mode mode = static_cast<Mode>(currentPacket[0]);

	switch (currentStep) {
		case 1:
			if (mode == Mode::CheckOnly) {
				if (*dispenserStatus) {
					status.s = S::DISPENSER_EMPTY;
				} else {
					status.s = S::CARD_FULL;
				}
			} else {
				if (status.s != S::ILLEGAL_COMMAND) {
					if (HasCard()) {
						SetSError(S::ILLEGAL_COMMAND);
					} else {
						if (*dispenserStatus) {
							status.s = S::DISPENSER_EMPTY;
						} else {
							DispenseCard();
						}
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
				setTime = std::mktime(tempTime);
			}
			status.SoftReset();
			runningCommand = false;
			break;
	}
}

void CardIo::Command_F0_GetVersion()
{
	switch (currentStep) {
		case 0:
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
		case 0:
			{
				std::string timeStr(13, 0);
				std::time_t currentTime = std::time(nullptr);

				std::time_t convTime{};

				if (setTime != 0) {
					convTime = setTime + currentTime - startTime;
				} else {
					convTime = currentTime;
				}

				std::strftime(&timeStr[0], timeStr.size(), "%y%m%d%H%M%S", std::localtime(&convTime));
				std::copy(timeStr.begin(), timeStr.end(), std::back_inserter(commandBuffer));
			}
			status.SoftReset();
			runningCommand = false;
			break;
		default:
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

bool CardIo::ReadTrack(std::vector<uint8_t> &trackData, int trackNumber)
{
	std::string fullPath = *basePath + *cardName;
	fullPath.append(".track_" + std::to_string(trackNumber));

	if (std::filesystem::exists(fullPath.c_str())) {
		if (std::filesystem::file_size(fullPath.c_str()) == TRACK_SIZE) {
			std::ifstream card(fullPath.c_str(), std::ifstream::in | std::ifstream::binary);
			std::string readBack(TRACK_SIZE, 0);

			card.read(&readBack[0], readBack.size());
			trackData.clear();
			std::copy(readBack.begin(), readBack.end(), std::back_inserter(trackData));
			card.close();
		} else {
			return false;
		}
	} else {
		// TODO: Is this a desired outcome?
		std::vector<uint8_t> blank(TRACK_SIZE, 0);
		WriteTrack(blank, trackNumber);
		trackData.resize(TRACK_SIZE);
	}

	return true;
}

void CardIo::WriteTrack(std::vector<uint8_t> &trackData, int trackNumber)
{
	std::string fullPath = *basePath + *cardName;
	fullPath.append(".track_" + std::to_string(trackNumber));

	std::string writeBack{};
	std::copy(trackData.begin(), trackData.end(), std::back_inserter(writeBack));

	std::ofstream card;
	card.open(fullPath, std::ofstream::out | std::ofstream::binary);
	card.write(writeBack.c_str(), writeBack.size());
	card.close();
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
	commandBuffer[1] = GetRStatus();
	commandBuffer[2] = static_cast<uint8_t>(status.p);
	commandBuffer[3] = static_cast<uint8_t>(status.s);

	spdlog::debug("R: {0:X} P: {1:X} S: {2:X}", 
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
		switch (currentCommand) {
			case 0x10: Command_10_Initalize(); break;
			case 0x20: Command_20_ReadStatus(); break;
			case 0x33: Command_33_ReadData2(); break;
			case 0x40: Command_40_Cancel(); break;
			case 0x53: Command_53_WriteData2(); break;
			case 0x78: Command_78_PrintSettings2(); break;
			case 0x7A: Command_7A_RegisterFont(); break;
			case 0x7B: Command_7B_PrintImage(); break;
			case 0x7C: Command_7C_PrintL(); break;
			case 0x7D: Command_7D_Erase(); break;
			case 0x7E: Command_7E_PrintBarcode(); break;
			case 0x80: Command_80_EjectCard(); break;
			case 0xA0: Command_A0_Clean(); break;
			case 0xB0: Command_B0_DispenseCardS31(); break;
			case 0xE1: Command_E1_SetRTC(); break;
			case 0xF0: Command_F0_GetVersion(); break;
			case 0xF1: Command_F1_GetRTC(); break;
			case 0xF5: Command_F5_CheckBattery(); break;
			default:
				spdlog::warn("CardIo::HandlePacket: Unhandled command {0:X}", currentCommand);
				SetSError(S::ILLEGAL_COMMAND);
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
		readBuffer.erase(readBuffer.begin()); // SLOW!
		return SyncError;
	}

	if (readBuffer.size() < 8) {
		return SyntaxError;
	}

	uint8_t count = GetByte(&buffer);

	// count counts itself but readBuffer will have both the STX and sum, we need to skip these.
	if (count > readBuffer.size() - 2) {
		spdlog::debug("Waiting for more data");
		return SizeError;
	}

	if (readBuffer.at(count) != END_OF_TEXT) {
		spdlog::debug("Missing ETX!");
		readBuffer.erase(readBuffer.begin(), readBuffer.begin() + count);
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

	// FIXME: MT2EXP "Transfer Card Data" interrupts Eject to do a CheckStatus, if we don't actaully eject here the system will error
	if (currentCommand == 0x80) {
		EjectCard();
	}

	currentCommand = currentPacket[0];

	// Remove the current command and the masters status bytes, we don't need it
	currentPacket.erase(currentPacket.begin(), currentPacket.begin() + 4);

	// TODO: Do all of this below better...
	status.SoftReset();
	status.s = S::RUNNING_COMMAND;
	runningCommand = true;
	currentStep = 0;

	commandBuffer.clear();
	commandBuffer.emplace_back(currentCommand);
	commandBuffer.emplace_back(GetRStatus());
	commandBuffer.emplace_back(static_cast<uint8_t>(status.p));
	commandBuffer.emplace_back(static_cast<uint8_t>(status.s));

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
