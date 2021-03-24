#ifndef CARDIO_H
#define CARDIO_H

#include <vector>
#include <iostream>

enum CardStatus {
	NoCard = 30,
	HasCard = 31,
	UNK_34 = 34, // clean card? eject?
};

enum ReadWritestatus {
	Idle = 30,
	UNK_32 = 32,
	UNK_33 = 33,
	UKN_34 = 34, // card inserted? blank?
};

typedef struct {
	CardStatus unk_1 = CardStatus::NoCard;
	uint8_t unk_2 = 0x30;
	ReadWritestatus unk_3 = ReadWritestatus::Idle;

	void Init() {
		unk_1 = CardStatus::NoCard;
		unk_2 = 0x30;
		unk_3 = ReadWritestatus::Idle;
	}
} wmmt_status_t;

class CardIo
{
public:
	enum StatusCode {
		Okay,
		SyncError,
		ChecksumError,
		EmptyResponseError,
		ServerWaitingReply,
	};

	CardIo();
	CardIo::StatusCode SendPacket(std::vector<uint8_t> *buffer);
	CardIo::StatusCode ReceivePacket(std::vector<uint8_t> *buffer);

	wmmt_status_t Status;

private:
	const uint8_t SYNC_BYTE = 0x02;
	const uint8_t SERVER_WAITING_BYTE = 0x05;
	const uint8_t RESPONSE_ACK = 0x06;

	uint8_t GetByte(std::vector<uint8_t> *buffer);
	void HandlePacket(std::vector<uint8_t> *packet);

	void PutStatusInBuffer();

	// Commands
	int WMMT_Command_10_Init();
	int WMMT_Command_20_Get_Card_State();
	int WMMT_Command_33_Read_Card();
	int WMMT_Command_40_Is_Card_Present();
	int WMMT_Command_53_Write_Card();

	int WMMT_Command_73_UNK();
	int WMMT_Command_7A_UNK();
	int WMMT_Command_7B_UNK(); // card inserted?
	int WMMT_Command_7C_UNK(); // Write text?
	int WMMT_Command_7D_UNK();
	int WMMT_Command_80_UNK(); // Eject?

	int WMMT_Command_A0_Clean_Card();
	int WMMT_Command_B0_Load_Card();
	int WMMT_Command_D0_UNK();

	std::vector<uint8_t> ResponseBuffer;	// Command Response
};

#endif
