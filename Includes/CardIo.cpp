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
	ReceiveBuffer.reserve(512);
	ResponseBuffer.reserve(255);
	ProcessedPacket.reserve(255);
}

void CardIo::Command_10_Initalize(std::vector<uint8_t> &packet)
{
	//enum Mode {
	//	Standard = 0x30,
	//	EjectAfter = 0x31,
	//	ResetSpecifications = 0x32,
	//};

	if (multiActionCommand == true) {
		switch (currentCommand) {
			case 0:
				if (currentRPS.r == R::HAS_CARD_1) {
					currentRPS.r = R::EJECTING_CARD;
				}
				currentRPS.s = S::RUNNING_COMMAND;
				break;
			case 1:
				if (currentRPS.r == R::EJECTING_CARD) {
					currentRPS.r = R::NO_CARD;
				}
				currentRPS.s = S::NO_JOB;
				break;
			default:
				break;
		}
		currentStep++;
		if (currentStep > 1) {
			multiActionCommand = false;
			currentStep = 0;
		}
	} else {
		multiActionCommand = true;
		currentStep = 0;
	}

	return;
}

void CardIo::Command_20_ReadStatus()
{
	// Return current RPS
	return;
}

void CardIo::Command_33_ReadData2(std::vector<uint8_t> &packet)
{
	//enum Mode {
	//	Standard     = 30, // read 69-bytes
	//	ReadVariable = 31, // variable length read, 1-47 bytes
	//	CardCapture  = 32, // pull in card?
	//};

	//enum BitMode {
	//	SevenBitParity   = 30,
	//	EightBitNoParity = 31,
	//};

	//enum Track {
	//	Track_1 = 30,
	//	Track_2 = 31,
	//	Track_3 = 32,
	//	Track_1_And_2 = 33,
	//	Track_1_And_3 = 34,
	//	Track_2_And_3 = 35,
	//	Track_1_2_And_3 = 36,
	//};
	
	if (multiActionCommand) {
		if (currentRPS.r != R::HAS_CARD_1) {
			switch (currentCommand) {
				case 0:
					currentRPS.s = S::RUNNING_COMMAND;
					break;
				case 1:
					currentRPS.s = S::WAITING_FOR_CARD;
					break;
				default:
					break;
			}
			currentCommand++;
			if (currentCommand > 1) {
				multiActionCommand = false;
				currentCommand = 0;
			}
		}
	} else {
		multiActionCommand = true;
		currentCommand = 0;
	}




/*
	Mode mode = static_cast<Mode>(packet[0]);
	BitMode bitMode = static_cast<BitMode>(packet[1]);
	Track trackMode = static_cast<Track>(packet[2]);

	if (currentRPS.r == R::NO_CARD) {
		currentRPS.s == S::WAITING_FOR_CARD;
	} else {
		std::copy(cardData.begin(), cardData.end(), std::back_inserter(ResponseBuffer));
	}
*/
	return;
}

void CardIo::Command_40_Cancel()
{
	currentRPS.s = S::NO_JOB;
	multiActionCommand = false;
	currentStep = 0;
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

void CardIo::Command_78_PrintSettings2(std::vector<uint8_t> &packet)
{
	// TODO: Have these be Printer class members
	enum CardType {
		LeucoRewrite_Blue = 0x37,
	};

	enum PrintMethod {
		Overwrite = 0x30,
		Overlay = 0x31,
	};

	enum FontWidth {
		Normal = 0x30,
		Half = 0x31,
	};

	enum PrintCoordinates {
		Standard = 0x30,
		// 31, 32
		LeftRotate = 0x33,
		RightRotate = 0x34,
	};

	return;
}

void CardIo::Command_7C_PrintL(std::vector<uint8_t> &packet)
{
	enum Mode {
		Both = 0x30, // process and print
		ProcessOnly = 0x31,
	};

	enum Buffer {
		Clear = 0x30,
		DoNotClear = 0x31,
	};

	enum PrintCoord {
		Standard = 0x01,
	};

	Mode mode  = static_cast<Mode>(packet[0]);
	Buffer bufferControl = static_cast<Buffer>(packet[1]);
	PrintCoord coords = static_cast<PrintCoord>(packet[2]);

	// TODO: Generate image, document control codes

	return;
}

void CardIo::Command_80_EjectCard()
{
	if (multiActionCommand) {
		switch (currentStep) {
			case 0:
				currentRPS.r = R::EJECTING_CARD;
				currentRPS.s = S::RUNNING_COMMAND;
				break;
			case 1:
				currentRPS.r = R::NO_CARD;
				break;
		}
		currentStep++;
		if (currentStep > 2) {
			multiActionCommand = false;
			currentStep = 0;
		}
	}

	if (currentRPS.r == R::HAS_CARD_1) {
		multiActionCommand = true;
	}
	//currentRPS.Reset();
	//*insertedCard = false;
}

void CardIo::Command_A0_Clean()
{
	switch (currentStep) {
		case 0: 
			//*insertedCard = false;
			currentRPS.Reset();
			currentRPS.s = S::WAITING_FOR_CARD;
			break;
		case 1:
			//*insertedCard = true;
			currentRPS.r = R::HAS_CARD_1;
			currentRPS.s = S::RUNNING_COMMAND;
			break;
		case 2:
			//*insertedCard = false;
			currentRPS.r = R::EJECTING_CARD;
			break;
		case 3:
			currentRPS.r = R::NO_CARD;
			break;
		default: 
			break;
	}

	currentStep++;

	if (currentStep > 3) {
		currentStep = 0;
		multiActionCommand = false;
	}

	return;
}

void CardIo::Command_B0_DispenseCardS31(std::vector<uint8_t> &packet)
{
	enum Mode {
		Dispenser = 0x31,
		CheckOnly = 0x32, // check status of dispenser
	};

	if (multiActionCommand) {
		switch (currentStep) {
			case 0:
				currentRPS.r = R::EJECTING_CARD;
				currentRPS.s = S::RUNNING_COMMAND;
				break;
			case 1:
				currentRPS.r = R::NO_CARD;
				break;
			case 2:
				currentRPS.r = R::HAS_CARD_1;
				break;
			case 3:
				currentRPS.r = R::EJECTING_CARD;
				break;
			case 4:
				currentRPS.r = R::NO_CARD;
				break;
			default:
				break;
		}
		currentStep++;
		if (currentStep > 4) {
			multiActionCommand = false;
			currentStep = 0;
		}
	} else {
		Mode mode = static_cast<Mode>(packet[0]);

		if (mode == Mode::Dispenser) {
			if (currentRPS.r == R::HAS_CARD_1) {
				multiActionCommand = true;
				currentStep = 0;
				//currentRPS.s = S::UNKNOWN_COMMAND;
			} else {
				currentRPS.r = R::HAS_CARD_1;
				//*insertedCard = true;
			}
		} else if (mode == Mode::CheckOnly) {
			currentRPS.s = S::CARD_FULL;
		}
	}

	return;
}

void CardIo::UpdateStatusBytes()
{
	if (currentCommand == 0x10 && multiActionCommand == true) {
		Command_10_Initalize(emptyBuffer);
	}

	if (currentCommand == 0xA0 && multiActionCommand == true) {
		Command_A0_Clean();
	}

	if (currentCommand == 0xB0 && multiActionCommand == true) {
		Command_B0_DispenseCardS31(emptyBuffer);
	}

	if (currentCommand == 0x80 && multiActionCommand == true) {
		Command_80_EjectCard();
	}
/*
	if (*insertedCard == true) {
		currentRPS.r = R::HAS_CARD_1;
	} else {
		currentRPS.r = R::NO_CARD;
	}
*/
	bool t = *insertedCard;

	std::printf("\ncurrentCommand: %X, insertedCard: %d, currentRPS.r: %X, currentRPS.p: %X, currentRPS.s: %X, multiActionCommand: %d\n",
		currentCommand, t, static_cast<uint8_t>(currentRPS.r), 
		static_cast<uint8_t>(currentRPS.p), static_cast<uint8_t>(currentRPS.s), multiActionCommand);
}

void CardIo::PutStatusInBuffer()
{
	ResponseBuffer.insert(ResponseBuffer.begin(), static_cast<uint8_t>(currentCommand));
	ResponseBuffer.insert(ResponseBuffer.begin()+1, static_cast<uint8_t>(currentRPS.r));
	ResponseBuffer.insert(ResponseBuffer.begin()+2, static_cast<uint8_t>(currentRPS.p));
	ResponseBuffer.insert(ResponseBuffer.begin()+3, static_cast<uint8_t>(currentRPS.s));

/*
	if (*insertedCard) {
		currentRPS.r = R::HAS_CARD_1;
		if (cardData.empty()) {
			LoadCardFromFS();
		}
	} else {
		if (!cardData.empty()) {
			cardData.clear();
		}
	}
*/
	if (!multiActionCommand) {
		currentRPS.p = P::NO_ERR;
		currentRPS.s = S::NO_JOB;
	}
}

void CardIo::LoadCardFromFS()
{
	// FIXME: Create a card if one doesn't exist.

	std::ifstream card(cardName.c_str(), std::ifstream::in | std::ifstream::binary | std::ifstream::ate);
	std::string readBack(CARD_SIZE, 0);
	int size = 0;

	if (card.good() && card.tellg() == CARD_SIZE) {
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
	if (currentCommand != packet[0]) {
		currentRPS.Reset();
	}

	currentCommand = packet[0];

	// This removes the current command and the master's status bytes.
	packet.erase(packet.begin(), packet.begin()+4);

	switch (currentCommand) {
		case 0x10: Command_10_Initalize(packet); return;
		case 0x20: Command_20_ReadStatus(); return;
		case 0x33: Command_33_ReadData2(packet); return;
		case 0x40: Command_40_Cancel(); return;
		case 0x53: Command_53_WriteData2(packet); return;
		case 0x78: Command_78_PrintSettings2(packet); return;
		case 0x7C: Command_7C_PrintL(packet); return;
		//case 0x7D: Command_7D_Erase(); return; // FIXME: Custom case
		case 0x80: Command_80_EjectCard(); return;
		case 0xA0: multiActionCommand = true; Command_A0_Clean(); return;
		case 0xB0: Command_B0_DispenseCardS31(packet); return;
		default:
			std::printf("CardIo::HandlePacket: Unhandled Command %02X\n", packet[0]);
			currentRPS.s = S::UNKNOWN_COMMAND;
			return;
	}
}

uint8_t CardIo::GetByte(uint8_t **buffer)
{
	const uint8_t value = (*buffer)[0];
	*buffer += 1;

	return value;
}

CardIo::StatusCode CardIo::ReceivePacket(std::vector<uint8_t> &buffer)
{
	uint8_t count{};
	uint8_t actual_checksum{};

#ifdef DEBUG_CARD_PACKETS
	std::cout << "CardIo::ReceivePacket:";
#endif

	std::copy(buffer.begin(), buffer.end(), std::back_inserter(ReceiveBuffer));

	uint8_t *buf = &ReceiveBuffer[0];

	// First, read the sync byte
	uint8_t sync = GetByte(&buf);
	if (sync == ENQUIRY) {
#ifdef DEBUG_CARD_PACKETS
		std::cout << " ENQ\n";
#endif
		ReceiveBuffer.erase(ReceiveBuffer.begin());
		UpdateStatusBytes();
		return ServerWaitingReply;
	} else if (sync != START_OF_TEXT) {
#ifdef DEBUG_CARD_PACKETS
		std::cerr << " Missing STX!\n";
#endif
		ReceiveBuffer.clear();
		return SyncError;
	} 

	count = GetByte(&buf);
	if (count > ReceiveBuffer.size() - 1) {
#ifdef DEBUG_CARD_PACKETS
		std::cout << " Waiting for more data\n";
#endif
		return SizeError;
	}

	UpdateStatusBytes();

	// Checksum is calcuated by xoring the entire packet excluding the start and the end.
	actual_checksum = count;

	// Decode the payload data
	ProcessedPacket.clear();
	for (int i = 0; i < count - 1; i++) { // NOTE: -1 to ignore sum byte
		uint8_t value = GetByte(&buf);
		ProcessedPacket.push_back(value);
		actual_checksum ^= value;
	}

	// Read the checksum from the last byte
	uint8_t packet_checksum = GetByte(&buf);

	ProcessedPacket.pop_back(); // Remove the END_OF_TEXT
	ReceiveBuffer.erase(ReceiveBuffer.begin(), ReceiveBuffer.begin() + count + 2); // Clear out the part of the buffer we've handled.

	// Verify checksum - skip packet if invalid
	if (packet_checksum != actual_checksum) {
#ifdef DEBUG_CARD_PACKETS
		std::cerr << " Checksum error!\n";
#endif
		return ChecksumError;
	}

#ifdef DEBUG_CARD_PACKETS
	for (const uint8_t n : ProcessedPacket) {
		std::printf(" %02X", n);
	}
	std::cout << "\n";
#endif

	// Clear out ResponseBuffer before fully processing the packet.
	ResponseBuffer.clear();

	HandlePacket(ProcessedPacket);

	// Put the ACK into the buffer, code above sends this to the server.
	if (!buffer.empty()) {
		buffer.clear();
	}
	//buffer.push_back(ACK);

	return Okay;
}

CardIo::StatusCode CardIo::BuildPacket(std::vector<uint8_t> &buffer)
{
#ifdef DEBUG_CARD_PACKETS
	std::cout << "CardIo::BuildPacket:";
#endif

	PutStatusInBuffer();

	uint8_t count = (ResponseBuffer.size() + 2) & 0xFF; // FIXME: +2 why?

	// Ensure our outgoing buffer is empty.
	if (!buffer.empty()) {
		buffer.clear();
	}

	// Send the header bytes
	buffer.emplace_back(START_OF_TEXT);
	buffer.emplace_back(count);

	// Calculate the checksum
	uint8_t packet_checksum = count;

	// Encode the payload data
	for (const uint8_t n : ResponseBuffer) {
		buffer.emplace_back(n);
		packet_checksum ^= n;
	}

	buffer.emplace_back(END_OF_TEXT);
	packet_checksum ^= END_OF_TEXT;

	// Write the checksum to the last byte
	buffer.emplace_back(packet_checksum);

	ResponseBuffer.clear();

#ifdef DEBUG_CARD_PACKETS
	for (const uint8_t n : buffer) {
		std::printf(" %02X", n);
	}
	std::cout << "\n";
#endif

	return Okay;
}
