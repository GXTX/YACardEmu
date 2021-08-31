#include "CardIo.h"

#define DEBUG_CARD_PACKETS

CardIo::CardIo(std::atomic<bool> &insert)
{
	insertedCard = insert;
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
	// TODO: What do we do if we get this command and there's no card? Set a read error?

	// Extra: 32 31 30
	if (insertedCard) {
		std::copy(cardData.begin(), cardData.end(), std::back_inserter(ResponseBuffer));
	} else {
		RPS.s = S::UNK_34;
	}

	return;
}

void CardIo::WMMT_Command_40_Cancel()
{
	// TODO: Does cancel eject the card or just stops the current job?
	//RPS.Reset();
	RPS.p = P::NO_ERR;
	RPS.s = S::NO_JOB;
	return;
}

void CardIo::WMMT_Command_53_Write(std::vector<uint8_t> &packet)
{
	// Extra: 30 31 30
	cardData.clear();
	for (size_t i = 0; i != packet.size() - 3; i++) {
		//cardData.at(i) = packet[i+3];
		cardData.emplace_back(packet[i+3]);
	}

#ifdef DEBUG_CARD_PACKETS
	std::cout << "CardIo::WMMT_Command_53_Write:";
	for (const uint8_t n : cardData) {
		std::printf(" %02X", n);
	}
	std::cout << "\n";
#endif

	//SaveCardToFS();
	return;
}

void CardIo::WMMT_Command_78_PrintSetting()
{
	// Extra: 37 31 30/33/34 30 30 30
	return;
}

void CardIo::WMMT_Command_7C_String(std::vector<uint8_t> &packet)
{
	// Extra: 30 30 01
#ifdef DEBUG_CARD_PACKETS
	std::cout << "CardIo::WMMT_Command_7C_String:";
	for (size_t i = 0; i != packet.size() - 3; i++){ // Skip the extra bytes.
		std::printf(" %02X", packet[i+3]);
	}
	std::cout << "\n";
#endif

	return;
}

void CardIo::WMMT_Command_7D_Erase()
{
	// Extra: 01 17
	// We just need to relay that we've handed this.
	// On hardware this would cause the thermal printer to 'reset' the print on the card.
	return;
}

void CardIo::WMMT_Command_80_Eject()
{
	//RPS.r = R::EJECTING_CARD;
	RPS.r = R::NO_CARD;
	//RPS.r = R::HAS_CARD;
	//RPS.s = S::UNKNOWN_COMMAND;
	insertedCard = false;
	return;
}

void CardIo::WMMT_Command_B0_GetCard()
{
	// Extra: 31

	if (insertedCard) {
		// What do we do if we have a card in the tray already?
		RPS.s = S::UNKNOWN_COMMAND; // FIXME:
	} else {
		RPS.r = R::HAS_CARD; // Is there a status for 'dispensing card'?
		insertedCard = true;
	}

	return;
}

void CardIo::PutStatusInBuffer()
{
	ResponseBuffer.insert(ResponseBuffer.begin(), static_cast<uint8_t>(currentCommand));
	ResponseBuffer.insert(ResponseBuffer.begin()+1, static_cast<uint8_t>(RPS.r));
	ResponseBuffer.insert(ResponseBuffer.begin()+2, static_cast<uint8_t>(RPS.p));
	ResponseBuffer.insert(ResponseBuffer.begin()+3, static_cast<uint8_t>(RPS.s));

	if (insertedCard) {
		RPS.r = R::HAS_CARD;
		if (cardData.empty()) {
			LoadCardFromFS();
		}
	} else {
		if (!cardData.empty()) {
			cardData.clear();
		}
	}

	if (!multiActionCommand) {
		RPS.p = P::NO_ERR;
		RPS.s = S::NO_JOB;
	}
}

void CardIo::LoadCardFromFS()
{
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

	//std::printf("HERE, %d\n", size);
	//std::filesystem::file_size(cardName)
	//for (const char c : readBack) {
	//	std::printf(" %c", c);
	//}

	// FIXME: Create a card if one doesn't exist.
	return;
}

void CardIo::SaveCardToFS()
{
	// TODO: Write back the backupCardData
	std::ofstream card;

	std::string writeBack;
	//std::copy(cardData.begin(), cardData.end(), std::back_inserter(writeBack));

	if (!cardData.empty()) {
		card.open(cardName, std::ofstream::out | std::ofstream::binary);
		card.write(writeBack.c_str(), writeBack.size());
		card.close();
	}

	return;
} 

void CardIo::HandlePacket(std::vector<uint8_t> &packet)
{
	currentCommand = static_cast<Commands>(packet[0]);

	// This removes the current command and the master's RPS bytes.
	for (uint8_t i = 0; i != 4; i++) {
		packet.erase(packet.begin());
	}

	switch (static_cast<uint8_t>(currentCommand)) {
		case 0x10: WMMT_Command_10_Init(); return;
		case 0x20: WMMT_Command_20_GetStatus(); return;
		case 0x33: WMMT_Command_33_Read(); return;
		case 0x40: WMMT_Command_40_Cancel(); return;
		case 0x53: WMMT_Command_53_Write(packet); return;
		case 0x78: WMMT_Command_78_PrintSetting(); return;
		//case 0x7A: WMMT_Command_7A_ExtraCharacter(); return;
		case 0x7C: WMMT_Command_7C_String(packet); return;
		case 0x7D: WMMT_Command_7D_Erase(); return;
		case 0x80: WMMT_Command_80_Eject(); return;
		//case 0xA0: WMMT_Command_A0_Clean(); return;
		case 0xB0: WMMT_Command_B0_GetCard(); return;
		//case 0xF5: WMMT_Command_F5_BatteryCheck(); return; // WMMT3 specific?
		default:
			std::printf("CardIo::HandlePacket: Unhandled Command %02X\n", packet[0]);
			RPS.s = S::UNKNOWN_COMMAND;
			return;
	}
}

uint8_t CardIo::GetByte(uint8_t **buffer)
{
	uint8_t value = (*buffer)[0];
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
		// FIXME: We shouldn't nessicarily delete this..
		ReceiveBuffer.clear(); // We don't need this anymore, empty it out.
		return ServerWaitingReply;
	} else if (sync != START_OF_TEXT) {
#ifdef DEBUG_CARD_PACKETS
		std::cerr << " Missing STX!\n";
#endif
		ReceiveBuffer.clear(); // We don't need this anymore, empty it out.
		return SyncError;
	} 

	// FIXME: Can still get odd results.
	count = GetByte(&buf);
	if (count > ReceiveBuffer.size() - 1) {
		//waitingForMoreData = true;
#ifdef DEBUG_CARD_PACKETS
		std::cout << " Waiting for more data\n";
#endif
		return SizeError;
	}

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
	ReceiveBuffer.clear(); // We've processed enough, we can clear the receive buffer now.

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
