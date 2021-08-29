#include "CardIo.h"

#define DEBUG_CARD_PACKETS

CardIo::CardIo()
{
	ResponseBuffer.reserve(255);
	ProcessedPacket.reserve(255);
}

void CardIo::WMMT_Command_10_Init()
{
	// Game only cares if S is NO_JOB, but we should reset all anyway.
	RPS.Reset();
	return;
}

void CardIo::WMMT_Command_20_GetStatus()
{
	// BuildPacket already puts this in the buffer, we don't need to do custom handling.
	return;
}

void CardIo::WMMT_Command_33_Read()
{
	// NOTE: We may need to update a status if we receive this command and we don't have a card.

	// Extra: 32 31 30
	std::copy(cardData.begin(), cardData.end(), std::back_inserter(ResponseBuffer));
	return;
}

void CardIo::WMMT_Command_40_Cancel()
{
	// TODO: Does cancel eject the card or just stops the current job?
	RPS.Reset();
	return;
}

void CardIo::WMMT_Command_53_Write(std::vector<uint8_t> &packet)
{
	// Extra: 30 31 30 

	for (size_t i = 0; i != packet.size(); i++) {
		cardData.at(i) = packet[i];
	}

	SaveCardToFS();
	return;
}

void CardIo::WMMT_Command_78_PrintSetting()
{
	// Extra: 37 31 30/33/34 30 30 30
	return;
}

void CardIo::WMMT_Command_7C_String()
{
	// Extra: 30 30
	return;
}

void CardIo::WMMT_Command_7D_Erase()
{
	// Extra: 01 17
	// We just need to relay that we've handed this.
	// On hardware this would cause the thermal printer to 'reset' the print on the card.
	return;
}

void CardIo::WMMT_Command_B0_GetCard()
{
	// Extra: 31

	if (RPS.r == R::NO_CARD) {
		RPS.r = R::HAS_CARD;
	} else if (RPS.r == R::HAS_CARD) {
		//RPS.Status = UNK_Status::UNK_32;
	}

	//PutStatusInBuffer();

	RPS.s = S::NO_JOB;
	return;
}

void CardIo::PutStatusInBuffer()
{
	ResponseBuffer.push_back(static_cast<uint8_t>(RPS.r));
	ResponseBuffer.push_back(static_cast<uint8_t>(RPS.p));
	ResponseBuffer.push_back(static_cast<uint8_t>(RPS.s));
}

void CardIo::LoadCardFromFS()
{
	constexpr const uintmax_t maxSizeCard = 0x45;
	std::ifstream card;

	std::string readBack;
	readBack.reserve(CARD_SIZE+1);

	if (std::filesystem::exists(cardName) && std::filesystem::file_size(cardName) == maxSizeCard) {
		card.open(cardName, std::ifstream::in | std::ifstream::binary);
		card.read(&readBack[0], std::filesystem::file_size(cardName));
		card.close();
		std::copy(readBack.begin(), readBack.end(), std::back_inserter(cardData));
		std::copy(readBack.begin(), readBack.end(), std::back_inserter(backupCardData));
	}

	// FIXME: Create a card if one doesn't exist.
	return;
}

void CardIo::SaveCardToFS()
{
	// TODO: Write back the backupCardData
	std::ofstream card;

	std::string writeBack;
	std::copy(cardData.begin(), cardData.end(), std::back_inserter(writeBack));

	if (!cardData.empty()) {
		card.open(cardName, std::ofstream::out | std::ofstream::binary);
		card.write(writeBack.c_str(), writeBack.size());
		card.close();
	}

	return;
} 

void CardIo::HandlePacket(std::vector<uint8_t> &packet)
{
	// We need to relay the command that we're replying to.
	ResponseBuffer.push_back(packet[0]);

	currentCommand = static_cast<Commands>(GetByte(packet));

	switch (static_cast<uint8_t>(currentCommand)) {
		case 0x10: WMMT_Command_10_Init(); break;
		case 0x20: WMMT_Command_20_GetStatus(); break;
		case 0x33: WMMT_Command_33_Read(); break;
		case 0x40: WMMT_Command_40_Cancel(); break;
		case 0x53: WMMT_Command_53_Write(packet); break;
		case 0x78: WMMT_Command_78_PrintSetting(); break;
		//case 0x7A: WMMT_Command_7A_ExtraCharacter(); break;
		case 0x7C: WMMT_Command_7C_String(); break;
		case 0x7D: WMMT_Command_7D_Erase(); break;
		//case 0x80: WMMT_Command_80_Eject(); break;
		//case 0xA0: WMMT_Command_A0_Clean(); break;
		case 0xB0: WMMT_Command_B0_GetCard(); break;
		//case 0xF5: WMMT_Command_F5_BatteryCheck(); break; // Not used in WMMT2
		default:
			std::printf("CardIo::HandlePacket: Unhandled Command %02X\n", packet.at(0));
			//PutStatusInBuffer(); // FIXME: We probably shouldn't reply at all.
			return;
	}
}

uint8_t CardIo::GetByte(std::vector<uint8_t> &buffer)
{
	uint8_t value = buffer[0];
	buffer.erase(buffer.begin());

	return value;
}

CardIo::StatusCode CardIo::ReceivePacket(std::vector<uint8_t> &buffer)
{
	if (buffer.empty()) {
		return StatusCode::EmptyResponseError;
	}

#ifdef DEBUG_CARD_PACKETS
	std::cout << "CardIo::ReceivePacket:";
#endif

	// First, read the sync byte
	uint8_t sync = GetByte(buffer);
	if (sync == ENQUIRY) {
#ifdef DEBUG_CARD_PACKETS
		std::cout << " ENQ\n";
#endif
		return ServerWaitingReply;
	} else if (sync != START_OF_TEXT) {
#ifdef DEBUG_CARD_PACKETS
		std::cerr << " Missing STX!\n";
#endif
		return SyncError;
	} 

	// FIXME: Verify length.
	uint8_t count = GetByte(buffer) - 1; // -1 to ignore sum

	// Checksum is calcuated by xoring the entire packet excluding the start and the end.
	uint8_t actual_checksum = count;

	// Decode the payload data
	ProcessedPacket.clear();
	for (int i = 0; i < count; i++) {
		uint8_t value = GetByte(buffer);
		ProcessedPacket.push_back(value);
		actual_checksum ^= value;
	}

	// Read the checksum from the last byte
	uint8_t packet_checksum = GetByte(buffer);

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
	buffer.push_back(ACK);

	return Okay;
}

CardIo::StatusCode CardIo::BuildPacket(std::vector<uint8_t> &buffer)
{
#ifdef DEBUG_CARD_PACKETS
	std::cout << "CardIo::BuildPacket:";
#endif

	// Should not happen?
	if (ResponseBuffer.empty()) {
#ifdef DEBUG_CARD_PACKETS
		std::cout << " Empty response.\n";
#endif
		return EmptyResponseError;
	}

	uint8_t count = (ResponseBuffer.size() + 1) & 0xFF;

	// Ensure our buffer is empty.
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
