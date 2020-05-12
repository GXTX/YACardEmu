#ifndef JVSIO_H
#define JVSIO_H

#include <vector>
#include <iostream>

typedef struct {
	uint8_t sync;
	uint8_t count;
} jvs_packet_header_t;

#define JVS_MAX_PLAYERS (2)
#define JVS_MAX_ANALOG (8)
#define JVS_MAX_COINS (JVS_MAX_PLAYERS)

class JvsIo
{
public:
	JvsIo();
	size_t SendPacket(std::vector<uint8_t> &buffer);
	size_t ReceivePacket(std::vector<uint8_t> &buffer);

private:
	enum StatusCode {
		StatusOkay = 1,
		UnsupportedCommand = 2,
		ChecksumError = 3,
		AcknowledgeOverflow = 4,
	};

	const uint8_t SYNC_BYTE = 0x02;
	const uint8_t SERVER_WAITING_BYTE = 0x05;
	const uint8_t RESPONSE_ACK = 0x06;

	uint8_t GetByte(std::vector<uint8_t> &buffer);
	void HandlePacket(jvs_packet_header_t* header, std::vector<uint8_t>& packet);

	// Commands
	int WMMT_Command_10_Init();
	int WMMT_Command_20_Get_Card_State();
	int WMMT_Command_33_Read_Card();
	int WMMT_Command_40_Is_Card_Present();
	int WMMT_Command_53_Write_Card();

	int WMMT_Command_73_UNK();
	int WMMT_Command_7A_UNK();
	int WMMT_Command_7C_UNK(); //Write text?
	int WMMT_Command_7D_UNK();
	int WMMT_Command_80_UNK();

	int WMMT_Command_A0_Clean_Card();
	int WMMT_Command_B0_Load_Card();
	int WMMT_Command_D0_UNK();

	std::vector<uint8_t> ResponseBuffer;	// Command Response
};

#endif
