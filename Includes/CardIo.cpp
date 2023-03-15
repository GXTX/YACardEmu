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

#include "CardIo.h"

CardIo::CardIo(CardIo::Settings *settings)
{
	startTime = std::time(nullptr);
	m_cardSettings = settings;
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
	status.SoftReset();
	runningCommand = false;
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
	//BitMode bit = static_cast<BitMode>(currentPacket[1]);
	Track track = static_cast<Track>(currentPacket[2]);

	switch (currentStep) {
		case 1:
			if (mode == static_cast<uint8_t>(Mode::CardCapture)) { // don't reply any card info if we get this
				if (!HasCard()) {
					status.s = S::WAITING_FOR_CARD;
					logger->info("Game requests user to insert card");
					m_cardSettings->waitingForCard = true;
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

								if (cardData.at(ctrack).empty()) {
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

								if (cardData.at(ctrack).empty() || cardData.at(ctrack1).empty()) {
									SetPError(P::READ_ERR);
									return;
								}

								std::copy(cardData.at(ctrack).begin(), cardData.at(ctrack).end(), std::back_inserter(commandBuffer));
								std::copy(cardData.at(ctrack1).begin(), cardData.at(ctrack1).end(), std::back_inserter(commandBuffer));
							}
							break;
						case Track::Track_1_2_And_3:
							{
								for (int i = 0; i < NUM_TRACKS; i++) {
									if (cardData.at(i).empty()) {
										SetPError(P::READ_ERR);
										return;
									}
									std::copy(cardData.at(i).begin(), cardData.at(i).end(), std::back_inserter(commandBuffer));
								}
							}
							break;
						default:
							SetPError(P::ILLEGAL_ERR);
							logger->warn("Unknown track read option {0:X}", static_cast<uint8_t>(track));
							return;
					}
				} else {
					status.s = S::WAITING_FOR_CARD;
					logger->info("Game requests user to insert card");
					m_cardSettings->waitingForCard = true;
					currentStep--;
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

void CardIo::Command_35_GetData()
{
	// We don't start at 0 because some systems don't like us immediately replying.
	// TODO: Should we be moving the card automatically under the read/write head?
	switch (currentStep) {
		case 1:
			if (!HasCard()) {
				// FIXME: Is this correct?
				SetPError(P::ILLEGAL_ERR);
				return;
			}

			cardData.clear();
			cardData.resize(NUM_TRACKS);
			for (int i = 0; i < NUM_TRACKS; i++) {
				if (cardData.at(i).empty()) {
					SetPError(P::READ_ERR);
					return;
				}
				std::copy(cardData.at(i).begin(), cardData.at(i).end(), std::back_inserter(commandBuffer));
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
	status.SoftReset();
	runningCommand = false;
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
	//BitMode bit = static_cast<BitMode>(currentPacket[1]);
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
						}
						break;
					case Track::Track_1_And_2:
					case Track::Track_1_And_3:
					case Track::Track_2_And_3:
						{
							if (currentPacket.size() - 3 < static_cast<size_t>(TRACK_SIZE + 1)) {
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
							for (uint8_t i = 0; i < NUM_TRACKS; i++) {
								const uint8_t offset = 3 + (i * TRACK_SIZE);
								std::copy(currentPacket.begin() + offset, currentPacket.begin() + offset + TRACK_SIZE, std::back_inserter(cardData.at(i)));
							}
						}
						break;
					default:
						SetPError(P::ILLEGAL_ERR);
						logger->warn("Unknown track write option {0:X}", static_cast<uint8_t>(track));
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
	status.SoftReset();
	runningCommand = false;
}

void CardIo::Command_7A_RegisterFont()
{
	status.SoftReset();
	runningCommand = false;
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

	//Mode mode = static_cast<Mode>(currentPacket[0]);
	//BufferControl control = static_cast<BufferControl>(currentPacket[1]);
	//uint8_t lineOffset = currentPacket[2];

	switch (currentStep) {
		case 1:
			if (!HasCard()) {
				SetPError(P::PRINT_ERR);
			}
			break;
		case 2:
			MoveCard(MovePositions::THERMAL_HEAD);
			break;
		case 3:
			{
				// FIXME: It seems the phyiscal readers ignore this, or have the ability to split between print "steps", either way, disable this for now
				//if (control == BufferControl::Clear) {
					printBuffer.clear();
				//}

				std::copy(currentPacket.begin() + 3, currentPacket.end(), std::back_inserter(printBuffer));

				// FIXME: Do this better.
				std::ofstream card;
				std::string writeBack{};
				std::string fullPath(m_cardSettings->cardPath.c_str());
				fullPath.append(printName);

				std::copy(printBuffer.begin(), printBuffer.end(), std::back_inserter(writeBack));

				card.open(fullPath, std::ofstream::out | std::ofstream::binary);
				card.write(writeBack.c_str(), writeBack.size());
				card.close();
			}
			break;
		case 4:
			MoveCard(MovePositions::READ_WRITE_HEAD);
			break;
		default:
			break;
	}

	if (currentStep > 4) {
		runningCommand = false;
	}
}

void CardIo::Command_7D_Erase()
{
	switch (currentStep) {
		case 1:
			if (!HasCard()) {
				SetPError(P::PRINT_ERR);
			}
			break;
		case 2:
			MoveCard(MovePositions::THERMAL_HEAD);
			break;
		case 3:
			MoveCard(MovePositions::READ_WRITE_HEAD);
			break;
		default:
			break;
	}

	if (currentStep > 3) {
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
			EjectCard();
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
		case 2:
			MoveCard(MovePositions::THERMAL_HEAD);
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

	// Mario Kart GP1 issues this command without options
	Mode mode = Mode::Dispense;
	if (!currentPacket.empty()) {
		mode = static_cast<Mode>(currentPacket[0]);
	}

	switch (currentStep) {
		case 1:
			if (mode == Mode::CheckOnly) {
				if (m_cardSettings->reportDispenserEmpty) {
					status.s = S::DISPENSER_EMPTY;
				} else {
					status.s = S::CARD_FULL;
				}
			} else {
				if (status.s != S::ILLEGAL_COMMAND) {
					if (HasCard()) {
						SetSError(S::ILLEGAL_COMMAND);
					} else {
						if (m_cardSettings->reportDispenserEmpty) {
							status.s = S::DISPENSER_EMPTY;
						} else {
							DispenseCard();
						}
					}
				}
			}
			break;
		case 2:
			if (GetCardPos() == MovePositions::DISPENSER_THERMAL) {
				MoveCard(MovePositions::READ_WRITE_HEAD);
			} else if (!m_cardSettings->reportDispenserEmpty) {
				SetPError(P::MOTOR_ERR);
			}
			break;
		default:
			break;
	}

	if (currentStep > 2) {
		runningCommand = false;
	}
}

void CardIo::Command_C0_ControlLED()
{
	// We don't need to handle this properly but let's leave some notes
	enum Mode {
		Off = 0x30,
		On = 0x31,
		SlowBlink = 0x32,
		FastBlink = 0x33,
	};

	status.SoftReset();
	runningCommand = false;
}

void CardIo::Command_C1_SetRetry()
{
	// We don't need to handle this properly but let's leave some notes
	// currentPacket[0] == 0x31 NONE ~ 0x39 MAX8
	status.SoftReset();
	runningCommand = false;
}

void CardIo::Command_D0_ShutterControl()
{
	// Only BR model supports this command
	SetSError(S::ILLEGAL_COMMAND);
}

void CardIo::Command_E1_SetRTC()
{
	std::stringstream timeStrS{};
	std::string timeStr{};
	std::copy(commandBuffer.begin(), commandBuffer.end(), std::back_inserter(timeStr));

	timeStrS << timeStr;

	std::tm *tempTime{};
	timeStrS >> std::get_time(tempTime, "%y%m%d%H%M%S");
	setTime = std::mktime(tempTime);

	status.SoftReset();
	runningCommand = false;
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
	status.SoftReset();
	runningCommand = false;
}

void CardIo::ClearCardData()
{
	cardData.clear();
	cardData.resize(NUM_TRACKS);
}

void CardIo::ReadCard()
{
	std::string fullPath = m_cardSettings->cardPath + m_cardSettings->cardName;
	
	// TODO: Should we actually be seeding zero's when the file doesn't exist?
	std::string readBack(CARD_SIZE, 0);

	ClearCardData();

	if (ghc::filesystem::exists(fullPath.c_str())) {
		if (ghc::filesystem::file_size(fullPath.c_str()) == CARD_SIZE) {
			std::ifstream card(fullPath.c_str(), std::ifstream::in | std::ifstream::binary);

			card.read(&readBack[0], readBack.size());
			card.close();
		} else {
			// File exists but isn't the right size, abort
			return;
		}
	}

	auto offset = 0;
	for (auto i = 0; i < NUM_TRACKS; i++) {
		std::copy(readBack.begin() + offset, readBack.begin() + offset + TRACK_SIZE, std::back_inserter(cardData.at(i)));
		offset += TRACK_SIZE;
	}
}

void CardIo::WriteCard()
{
	// Should only happen when issued from dispenser, we want to avoid overwriting a previous card
	if (!m_cardSettings->insertedCard) {
		auto i = 0;
		while (true) {
			auto newCardName = m_cardSettings->cardName + std::to_string(i);

			// Perfenece to not having a number...
			if (i == 0) {
				newCardName = m_cardSettings->cardName;
			}

			auto fullName = m_cardSettings->cardPath + newCardName;

			if (!ghc::filesystem::exists(fullName.c_str())) {
				m_cardSettings->cardName = newCardName;
				break;
			}
			i++;
		}
	}

	// TODO: Handle if cardPath doesn't have a trailing slash
	auto fullPath = m_cardSettings->cardPath + m_cardSettings->cardName;

	std::string writeBack;
	for (const auto &data: cardData) {
		std::copy(data.begin(), data.end(), std::back_inserter(writeBack));
	}

	std::ofstream card;
	card.open(fullPath, std::ofstream::out | std::ofstream::binary);
	card.write(writeBack.c_str(), writeBack.size());
	card.close();

	ClearCardData();
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

	logger->trace("CardIo::UpdateStatusInBuffer: R: {0:X} P: {1:X} S: {2:X}", 
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

	if (runningCommand) {
		if (m_cardSettings->waitingForCard) {
			logger->info("Game no longer requests user to insert card");
			m_cardSettings->waitingForCard = false;
		}
		switch (currentCommand) {
			case 0x10: Command_10_Initalize(); break;
			case 0x20: Command_20_ReadStatus(); break;
			case 0x33: Command_33_ReadData2(); break;
			case 0x35: Command_35_GetData(); break;
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
			case 0xC0: Command_C0_ControlLED(); break;
			case 0xC1: Command_C1_SetRetry(); break;
			case 0xD0: Command_D0_ShutterControl(); break;
			case 0xE1: Command_E1_SetRTC(); break;
			case 0xF0: Command_F0_GetVersion(); break;
			case 0xF1: Command_F1_GetRTC(); break;
			case 0xF5: Command_F5_CheckBattery(); break;
			default:
				logger->warn("CardIo::HandlePacket: Unhandled command {0:X}", currentCommand);
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
	uint8_t *buffer = &readBuffer[0];

	// First, read the sync byte
	uint8_t sync = GetByte(&buffer);

	if (sync == ENQUIRY) {
		logger->trace("CardIo::ReceivePacket: ENQ");
		readBuffer.erase(readBuffer.begin());
		HandlePacket();
		return ServerWaitingReply;
	} else if (sync != START_OF_TEXT) {
		logger->warn("CardIo::ReceivePacket: Missing STX!");
		readBuffer.erase(readBuffer.begin()); // SLOW!
		return SyncError;
	}

	if (readBuffer.size() < 8) {
		return SyntaxError;
	}

	uint8_t count = GetByte(&buffer);

	// count counts itself but readBuffer will have both the STX and sum, we need to skip these.
	if (count > readBuffer.size() - 2) {
		logger->trace("CardIo::ReceivePacket: Waiting for more data");
		return SizeError;
	}

	if (readBuffer.at(count) != END_OF_TEXT) {
		logger->debug("CardIo::ReceivePacket: Missing ETX!");
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
		logger->warn("CardIo::ReceivePacket: Read checksum bad!");
		return ChecksumError;
	}

	logger->debug("CardIo::ReceivePacket: {:Xn}", spdlog::to_hex(currentPacket));

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
	UpdateStatusInBuffer();

	uint8_t count = static_cast<uint8_t>(commandBuffer.size() + 2);

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

	logger->debug("CardIo::BuildPacket: {:Xn}", spdlog::to_hex(writeBuffer));

	return Okay;
}
