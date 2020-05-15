#include "JvsIo.h"

#define DEBUG_JVS_PACKETS

// We will emulate SEGA 837-13551 IO Board
JvsIo::JvsIo()
{

}

void JvsIo::PutStatusInBuffer()
{
	ResponseBuffer.push_back(Status.unk_1);
	ResponseBuffer.push_back(Status.unk_2);
	ResponseBuffer.push_back(Status.unk_3);
}

int JvsIo::WMMT_Command_10_Init()
{
	Status.Init();
	PutStatusInBuffer();
	return 6;
}

int JvsIo::WMMT_Command_20_Get_Card_State()
{
	PutStatusInBuffer();
	return 6;
}

// FIXME: This doesn't seem right..? But we need to send 30/30/30?
int JvsIo::WMMT_Command_40_Is_Card_Present()
{
	Status.Init();
	PutStatusInBuffer();
	return 6;
}

int JvsIo::WMMT_Command_B0_Load_Card()
{
	if (Status.unk_1 == CardStatus::NoCard) {
		Status.unk_1 = CardStatus::HasCard;
	}
	else if (Status.unk_1 == CardStatus::HasCard) {
		Status.unk_3 = ReadWritestatus::UNK_32;
	}
	
	PutStatusInBuffer();

	Status.unk_3 = ReadWritestatus::Idle;

	return 6;
}

uint8_t JvsIo::GetByte(std::vector<uint8_t> &buffer)
{
	uint8_t value = buffer.at(0);
	buffer.erase(buffer.begin());

#ifdef DEBUG_JVS_PACKETS
	std::printf(" %02X", value);
#endif
	return value;
}

void JvsIo::HandlePacket(jvs_packet_header_t* header, std::vector<uint8_t>& packet)
{
	// It's possible for a JVS packet to contain multiple commands, so we must iterate through it
	ResponseBuffer.push_back(RESPONSE_ACK); // Assume we'll handle the command just fine

	for (size_t i = 0; i < packet.size(); i++) {
		uint8_t* command_data = &packet.at(i);
		switch (packet.at(i)) {
			case 0x10: i += WMMT_Command_10_Init(); break;
			case 0x20: i += WMMT_Command_20_Get_Card_State(); break;
			//case 0x33: i += WMMT_Command_33_Read_Card(); break;
			case 0x40: i += WMMT_Command_40_Is_Card_Present(); break;
			/*case 0x53: i += WMMT_Command_53_Write_Card(); break;
			case 0x73: i += WMMT_Command_73_UNK(); break;
			case 0x7A: i += WMMT_Command_7A_UNK(); break;
			case 0x7B: i += WMMT_Command_7B_UNK(); break;
			case 0x7C: i += WMMT_Command_7C_UNK(); break;
			case 0x7D: i += WMMT_Command_7D_UNK(); break;
			case 0x80: i += WMMT_Command_80_UNK(); break;
			case 0xA0: i += WMMT_Command_A0_Clean_Card(); break;*/
			case 0xB0: i += WMMT_Command_B0_Load_Card(); break;
			//case 0xD0: i += WMMT_Command_D0_UNK(); break;
			default:
				// Overwrite the verly-optimistic StatusCode::StatusOkay with Status::Unsupported command
				// Don't process any further commands. Existing processed commands must still return their responses.
				//ResponseBuffer[0] = StatusCode::UnsupportedCommand;
				std::printf("JvsIo::HandlePacket: Unhandled Command %02X", packet[i]);
				std::cout << std::endl;
				return;
		}
	}
}

size_t JvsIo::ReceivePacket(std::vector<uint8_t> &buffer)
{
	// Scan the packet header
	jvs_packet_header_t header;

	// First, read the sync byte
#ifdef DEBUG_JVS_PACKETS
	std::cout << "JvsIo::ReceivePacket:";
#endif
	header.sync = GetByte(buffer); // Do not unescape the sync-byte!
	if (header.sync != SYNC_BYTE) {
#ifdef DEBUG_JVS_PACKETS
		std::cout << " [Missing SYNC_BYTE!]" << std::endl;
#endif
		// If it's wrong, return we've processed (actually, skipped) one byte
		return 1;
	}

	// Read the target and count bytes
	header.count = GetByte(buffer);

	// Calculate the checksum
	uint8_t actual_checksum = 0;
	actual_checksum ^= header.count;

	// Decode the payload data
	// TODO: don't put in another vector just to send off
	std::vector<uint8_t> packet;
	for (int i = 0; i < header.count - 1; i++) { // Note : -1 to avoid adding the checksum byte to the packet
		uint8_t value = GetByte(buffer);
		packet.push_back(value);
		actual_checksum ^= value;
	}

	// Read the checksum from the last byte
	uint8_t packet_checksum = GetByte(buffer);

	// Verify checksum - skip packet if invalid
	ResponseBuffer.clear();
	if (packet_checksum != actual_checksum) {
#ifdef DEBUG_JVS_PACKETS
		std::cout << " Checksum Error!" << std::endl;
#endif
	} else {
		// If the packet was intended for us, we need to handle it
		HandlePacket(&header, packet);
	}

#ifdef DEBUG_JVS_PACKETS
	std::cout << std::endl;
#endif

	return packet.size() + 1;
}

size_t JvsIo::SendPacket(std::vector<uint8_t> &buffer)
{
	if (ResponseBuffer.empty()) {
		return 0;
	}

	// Build a JVS response packet containing the payload
	jvs_packet_header_t header;
	header.sync = SYNC_BYTE;
	header.count = (uint8_t)ResponseBuffer.size() + 1; // Set data size to payload + 1 checksum byte
	// TODO : What if count overflows (meaning : responses are bigger than 255 bytes); Should we split it over multiple packets??

	// Send the header bytes
	buffer.push_back(header.sync);
	buffer.push_back(header.count);

	// Calculate the checksum
	// FIXME: checksum is missing count byte
	uint8_t packet_checksum = 0;

	// Encode the payload data
	for (size_t i = 0; i < ResponseBuffer.size(); i++) {
		uint8_t value = ResponseBuffer[i];
		buffer.push_back(value);
		packet_checksum ^= value;
	}

	// Write the checksum to the last byte
	buffer.push_back(packet_checksum);

	ResponseBuffer.clear();

#ifdef DEBUG_JVS_PACKETS
	std::cout << "JvsIo::SendPacket:";
	for (size_t i = 0; i < buffer.size(); i++) {
		std::printf(" %02X", buffer.at(i));
	}
	std::cout << std::endl;
#endif

	return buffer.size();
}
