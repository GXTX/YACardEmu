#include "CardIo.h"

#define DEBUG_CARD_PACKETS

CardIo::CardIo()
{
	ResponseBuffer.reserve(255);
	ProcessedPacket.reserve(255);
}

int CardIo::WMMT_Command_10_Init()
{
	// This command only seems to care about the "S" byte being 0x30.
	RPS.Status = UNK_Status::Idle;
	PutStatusInBuffer();
	return 6;
}

int CardIo::WMMT_Command_20_Get_Card_State()
{
	PutStatusInBuffer();
	return 0;
}

int CardIo::WMMT_Command_33_Read_Card()
{
	// Extra: 32 31 30
	std::copy(card_data.begin(), card_data.end(), std::back_inserter(ResponseBuffer));

	return 0;
}

int CardIo::WMMT_Command_40_Is_Card_Present()
{
	RPS.Reset();
	PutStatusInBuffer();
	return 6;
}

int CardIo::WMMT_Command_53_Write_Card(std::vector<uint8_t> &packet)
{
/*
Extra: 30 31 30 

Real data:
52 76 00 00 7C 8A 7A 88 89 0A 01 17 00 00 20 22 00 00 00 00 00 C8 00 00 00 00 
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
00 00 00 00 00 00 00 00 00 00 77 48 44 05 00 00 47 
*/

	for (uint8_t i = 0; i < packet.size(); i++) {
		card_data.at(i) = packet.at(i);
	}

	SaveCardToFS(card_name);

	return 0;
}

int CardIo::WMMT_Command_78_UNK()
{
	// Extra: 37 31 34 30 30 30
	PutStatusInBuffer();
	return 1;
}

int CardIo::WMMT_Command_7C_Write_Card_Text()
{
/*
Extra: 30 30

Real data:
Encoding is SHIFT-JIS, SOH is start of line, DC1 BOLD AND BIG, DC4 TINY

01 11 82 66 82 74 82 64 82 72 82 73 0D 4D 41 5A 44 41 0D 52 58 2D 37 20 5B 46 
44 33 53 5D 0D 11 82 6D 8B 89 0D 11 82 51 82 57 82 4F 94 6E 97 CD 81 5E 14 42 
0D 20 20 20 20 20 20 20 20 20 20 20 20 20 20 0D 20 20 20 20 20 20 20 20 20 20 
20 20 20 20 20 20 0D 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 0D 20 20 
20 20 20 20 20 20 20 20 20 20 20 20 20 20 0D 20 41 50 51 36 4E 4C 48 45 32 38 
37 39 5A 57 53
*/
	return 1;
}

int CardIo::WMMT_Command_7D_UNK()
{
	// Extra: 01 17
	return 1;
}

int CardIo::WMMT_Command_B0_Load_Card()
{
	// Extra: 31

	if (RPS.Reader == ReaderStatus::NoCard) {
		RPS.Reader = ReaderStatus::HasCard;
	} else if (RPS.Reader == ReaderStatus::HasCard) {
		RPS.Status = UNK_Status::UNK_32;
	}

	PutStatusInBuffer();

	RPS.Status = UNK_Status::Idle;

	return 6;
}

void CardIo::PutStatusInBuffer()
{
	ResponseBuffer.push_back(static_cast<uint8_t>(RPS.Reader));
	ResponseBuffer.push_back(static_cast<uint8_t>(RPS.Printer));
	ResponseBuffer.push_back(static_cast<uint8_t>(RPS.Status));
}

void CardIo::LoadCardFromFS(std::string card_name)
{
	constexpr const uintmax_t maxSizeCard = 0x45;
	std::ifstream card;

	if (std::filesystem::exists(card_name) && std::filesystem::file_size(card_name) == maxSizeCard) {
		card.open(card_name, std::ifstream::in | std::ifstream::binary);
		card.read(reinterpret_cast<char *>(&card_data[0]), std::filesystem::file_size(card_name));
		card.close();
		return;
	}

	// FIXME: Create a card if one doesn't exist.
}

void CardIo::SaveCardToFS(std::string card_name)
{
	std::ofstream card;

	if (!card_data.empty()) {
		card.open(card_name, std::ofstream::out | std::ofstream::binary);
		card.write(reinterpret_cast<char *>(&card_data[0]), card_data.size());
		card.close();
	}
} 

void CardIo::HandlePacket(std::vector<uint8_t> &packet)
{
	// We need to relay the command that we're replying to.
	ResponseBuffer.push_back(packet.at(0));

	uint8_t *buf = &packet[0];

	switch (GetByte(&buf)) {
		case 0x10: WMMT_Command_10_Init(); break;
		case 0x20: WMMT_Command_20_Get_Card_State(); break;
		case 0x33: WMMT_Command_33_Read_Card(); break;
		case 0x40: WMMT_Command_40_Is_Card_Present(); break;
		case 0x53: WMMT_Command_53_Write_Card(packet); break;
		//case 0x73: WMMT_Command_73_UNK(); break;
		case 0x78: WMMT_Command_78_UNK(); break;
		//case 0x7A: WMMT_Command_7A_UNK(); break;
		//case 0x7B: WMMT_Command_7B_UNK(); break;
		//case 0x7C: WMMT_Command_7C_UNK(); break;
		//case 0x7D: WMMT_Command_7D_UNK(); break;
		//case 0x80: WMMT_Command_80_UNK(); break;
		//case 0xA0: WMMT_Command_A0_Clean_Card(); break;
		case 0xB0: WMMT_Command_B0_Load_Card(); break;
		//case 0xD0: WMMT_Command_D0_UNK(); break;
		default:
			std::printf("CardIo::HandlePacket: Unhandled Command %02X\n", packet.at(0));
			PutStatusInBuffer(); // FIXME: We probably shouldn't reply at all.
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
	if (buffer.empty()) {
		return StatusCode::EmptyResponseError;
	}

	uint8_t *buf = &buffer[0];

#ifdef DEBUG_CARD_PACKETS
	std::cout << "CardIo::ReceivePacket:";
#endif

	// First, read the sync byte
	uint8_t sync = GetByte(&buf);
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
	uint8_t count = GetByte(&buf) - 3; // -1 to ignore sum, -2 to ignore the 2 other bytes before this

	// Checksum is calcuated by xoring the entire packet excluding the start and the end.
	uint8_t actual_checksum = count;

	// Decode the payload data
	ProcessedPacket.clear();
	for (int i = 0; i < count; i++) {
		uint8_t value = GetByte(&buf);
		ProcessedPacket.push_back(value);
		actual_checksum ^= value;
	}

	// Read the checksum from the last byte
	uint8_t packet_checksum = GetByte(&buf);

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
