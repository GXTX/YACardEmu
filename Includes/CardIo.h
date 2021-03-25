#ifndef CARDIO_H
#define CARDIO_H

#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>

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
	CardIo::StatusCode BuildPacket(std::vector<uint8_t> *buffer);
	CardIo::StatusCode ReceivePacket(std::vector<uint8_t> *buffer);
private:
	const uint8_t SYNC_BYTE = 0x02;
	const uint8_t UNK_RESP_BYTE = 0x03;
	const uint8_t SERVER_WAITING_BYTE = 0x05;
	const uint8_t RESPONSE_ACK = 0x06;

	const uint8_t CARD_SIZE = 0x45;

	// 1/3 in the RPS reply.
	enum CardStatus {
		NoCard = 30,
		HasCard = 31,
		//UNK_32 = 32,
		//UNK_33 = 33,
		UNK_34 = 34, // Seen when cleaning the card right before we reply 30/30/30? Reply as a empty card?
	};

	// 2/3 in the reply, we always return 30.
	enum UNKStatus {
		UNK_30 = 30,
	};

	// 3/3 in the reply, assumption is read status.
	enum RWStatus {
		Idle = 30,
		//UNK_31 = 31,
		UNK_32 = 32, // Used during LoadCard, assumption is - reading.
		UNK_33 = 33, // 
		UKN_34 = 34, // Only seen in ReadCard if we don't have a card?
	};

	typedef struct {
		CardStatus unk_1 = CardStatus::NoCard;
		uint8_t unk_2 = UNKStatus::UNK_30;
		RWStatus unk_3 = RWStatus::Idle;

		void Init() {
			unk_1 = CardStatus::NoCard;
			unk_2 = UNKStatus::UNK_30;
			unk_3 = RWStatus::Idle;
		}
	} wmmt_status_t;

	wmmt_status_t Status;

	uint8_t GetByte(std::vector<uint8_t> *buffer);
	void HandlePacket(std::vector<uint8_t> *packet);

	const std::string card_name = "test.bin";
	std::vector<uint8_t> *card_data;

	void LoadCardFromFS(std::string card_name);
	void SaveCardToFS(std::string card_name);

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
