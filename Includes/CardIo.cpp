#include "CardIo.h"

#define DEBUG_CARD_PACKETS

CardIo::CardIo()
{

}

void CardIo::PutStatusInBuffer()
{
	ResponseBuffer.push_back(Status.unk_1);
	ResponseBuffer.push_back(Status.unk_2);
	ResponseBuffer.push_back(Status.unk_3);
}

int CardIo::WMMT_Command_10_Init()
{
	Status.Init();
	PutStatusInBuffer();
	return 6;
}

int CardIo::WMMT_Command_20_Get_Card_State()
{
	PutStatusInBuffer();
	return 6;
}

int CardIo::WMMT_Command_33_Read_Card()
{
	for (uint8_t i = 0; i < CARD_SIZE; i++) {
		ResponseBuffer.push_back(card_data->at(i));
	}

	return 0;
}

int CardIo::WMMT_Command_40_Is_Card_Present()
{
	Status.Init();
	PutStatusInBuffer();
	return 6;
}

int CardIo::WMMT_Command_53_Write_Card(std::vector<uint8_t> *packet)
{
	for (auto i : *packet) {
		card_data->at(i) = packet->at(i);
	}

	SaveCardToFS(card_name);

	return 0;
}

int CardIo::WMMT_Command_B0_Load_Card()
{
	if (Status.unk_1 == CardStatus::NoCard) {
		Status.unk_1 = CardStatus::HasCard;
	}
	else if (Status.unk_1 == CardStatus::HasCard) {
		Status.unk_3 = RWStatus::UNK_32;
	}
	
	PutStatusInBuffer();

	Status.unk_3 = RWStatus::Idle;

	return 6;
}

void CardIo::LoadCardFromFS(std::string card_name)
{
	std::ifstream card;

	if (std::filesystem::exists(card_name) && std::filesystem::file_size(card_name) == CARD_SIZE) {
		card.open(card_name, std::ifstream::in | std::ifstream::binary);
		card.read(reinterpret_cast<char*>(card_data->data()), std::filesystem::file_size(card_name));
		card.close();
	} else {
		// TODO: If the card supplied is larger than 0x45 we might want to open a *new* card with a new name
		// otherwise we might be overwriting the card data. Oops.
	}
}

void CardIo::SaveCardToFS(std::string card_name)
{
	std::ofstream card;

	// TODO: Is there ever a time when the system would give us a zero sized card?
	if (!card_data->empty()) {
		card.open(card_name, std::ofstream::out | std::ofstream::binary);
		card.write(reinterpret_cast<char*>(card_data->data()), card_data->size());
		card.close();
	}
} 

uint8_t CardIo::GetByte(std::vector<uint8_t> *buffer)
{
	uint8_t value = buffer->at(0);
	buffer->erase(buffer->begin());

#ifdef DEBUG_CARD_PACKETS
	//std::printf(" %02X", value);
#endif

	return value;
}

void CardIo::HandlePacket(std::vector<uint8_t> *packet)
{
	// At this point [0] should be our command, we've already processed the header in CardIo::ReceivePacket()
	switch (packet->at(0)) {
		case 0x10: WMMT_Command_10_Init(); break;
		case 0x20: WMMT_Command_20_Get_Card_State(); break;
		case 0x33: WMMT_Command_33_Read_Card(); break;
		case 0x40: WMMT_Command_40_Is_Card_Present(); break;
		case 0x53: WMMT_Command_53_Write_Card(packet); break;
		//case 0x73: WMMT_Command_73_UNK(); break;
		//case 0x7A: WMMT_Command_7A_UNK(); break;
		//case 0x7B: WMMT_Command_7B_UNK(); break;
		//case 0x7C: WMMT_Command_7C_UNK(); break;
		//case 0x7D: WMMT_Command_7D_UNK(); break;
		//case 0x80: WMMT_Command_80_UNK(); break;
		//case 0xA0: WMMT_Command_A0_Clean_Card(); break;
		case 0xB0: WMMT_Command_B0_Load_Card(); break;
		//case 0xD0: WMMT_Command_D0_UNK(); break;
		default:
			std::printf("CardIo::HandlePacket: Unhandled Command %02X", packet->at(0));
			std::cout << std::endl;
			return;
	}
}

CardIo::StatusCode CardIo::ReceivePacket(std::vector<uint8_t> *buffer)
{
	// If the packet we're trying to process is the waiting byte
	// then just return, we should've already created the reply.
	if (buffer->at(0) == SERVER_WAITING_BYTE) {
		buffer->clear();
		return ServerWaitingReply;
	}

	// First, read the sync byte
#ifdef DEBUG_CARD_PACKETS
	std::cout << "CardIo::ReceivePacket:";
#endif
	if (GetByte(buffer) != SYNC_BYTE) {
		std::cerr << " Missing SYNC_BYTE!";
		return SyncError;
	}

	uint8_t count = GetByte(buffer);

	// Checksum is calcuated by xoring the entire packet excluding the start and the end.
	uint8_t actual_checksum = count;

	// Decode the payload data
	std::vector<uint8_t> packet;
	for (int i = 0; i < count - 1; i++) { // NOTE: -1 to avoid adding the checksum byte to the packet
		uint8_t value = GetByte(buffer);
		packet.push_back(value);
		actual_checksum ^= value;
	}

	// Read the checksum from the last byte
	uint8_t packet_checksum = GetByte(buffer);

	// Verify checksum - skip packet if invalid
	ResponseBuffer.clear();
	if (packet_checksum != actual_checksum) {
#ifdef DEBUG_CARD_PACKETS
		std::cerr << " Checksum error!";
#endif
		return ChecksumError;
	}

	HandlePacket(&packet);

#ifdef DEBUG_CARD_PACKETS
	std::cout << std::endl;
#endif

	// TODO: check that HandlePacket() can actually handle what we're eventually going to send.
	buffer->clear();
	buffer->push_back(RESPONSE_ACK);

	return Okay;
}

// TODO: We expect buffer to be .empty(), is this safe to assume?
CardIo::StatusCode CardIo::BuildPacket(std::vector<uint8_t> *buffer)
{
	// Should not happen?
	if (ResponseBuffer.empty()) {
		return EmptyResponseError;
	}

	uint8_t count = (ResponseBuffer.size() + 1) & 0xFF;

	// Send the header bytes
	buffer->push_back(SYNC_BYTE);
	buffer->push_back(count);

	// Calculate the checksum
	uint8_t packet_checksum = count;

	// Encode the payload data
	for (auto i : ResponseBuffer) {
		buffer->push_back(ResponseBuffer.at(i));
		packet_checksum ^= ResponseBuffer.at(i);
	}

	// TODO: Figure out what this actually is.
	buffer->push_back(UNK_RESP_BYTE);

	// Write the checksum to the last byte
	buffer->push_back(packet_checksum);

	ResponseBuffer.clear();

#ifdef DEBUG_CARD_PACKETS
	std::cout << "CardIo::SendPacket:";
	for (auto i : *buffer) {
		std::printf(" %02X", buffer->at(i));
	}
	std::cout << std::endl;
#endif

	return Okay;
}
