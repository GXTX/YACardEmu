#ifndef CARDIO_H
#define CARDIO_H

#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>

// Status bytes:
//////////////////////////////////////////////
enum class R {
	NO_CARD       = 0x30,
	HAS_CARD      = 0x31,
	UNK_32        = 0x32,
	UNK_33        = 0x33,
	EJECTING_CARD = 0x34,
};

// This seems wrong?
enum class P {
	NO_ERR      = 0x30,
	READ_ERR    = 0x31, // ?
	WRITE_ERR   = 0x32,
	BLOCK_ERR   = 0x33, // ?
	UNK_34      = 0x34,
	PRINT_ERR   = 0x35,
	UNK_36      = 0x36,
	UKN_37      = 0x37,
	ILLEGAL_ERR = 0x38,
	UNK_39      = 0x39,
	BATTERY_ERR = 0x40,
	SYSTEM_ERR  = 0x41,
	//UNK_51,52,53,54,55
};

enum class S {
	NO_JOB          = 0x30, // JOB_END
	UNK_31          = 0x31,
	UNKNOWN_COMMAND = 0x32, // Is this correct?
	UNK_33          = 0x33, // Running?
	UNK_34          = 0x34,
	DISPENSER_EMPTY = 0x35,
};
//////////////////////////////////////////////

struct Status{
	R r = R::NO_CARD;
	P p = P::NO_ERR;
	S s = S::NO_JOB;

	void Reset()
	{
		r = R::NO_CARD;
		p = P::NO_ERR;
		s = S::NO_JOB;
	}
};

class CardIo
{
public:
	enum StatusCode {
		Okay,
		SizeError,
		SyncError,
		ChecksumError,
		EmptyResponseError,
		ServerWaitingReply,
	};

	CardIo();
	CardIo::StatusCode BuildPacket(std::vector<uint8_t> &buffer);
	CardIo::StatusCode ReceivePacket(std::vector<uint8_t> &buffer);
	
	void LoadCardFromFS();
	void SaveCardToFS();
private:
	const uint8_t START_OF_TEXT = 0x02;
	const uint8_t END_OF_TEXT = 0x03;
	const uint8_t ENQUIRY = 0x05;
	const uint8_t ACK = 0x06;

	const uint8_t CARD_SIZE = 0x46;

	uint8_t GetByte(uint8_t **buffer);
	void HandlePacket(std::vector<uint8_t> &packet);

	const std::string cardName = "card.bin";
	std::vector<uint8_t>cardData{};
	std::vector<uint8_t>backupCardData{}; // Filled with the data when we first loaded the card.

	void PutStatusInBuffer();

	enum class Commands {
		NoCommand = 0x00, // Placeholder
		Init = 0x10,
		GetStatus = 0x20,
		Read = 0x33,
		Cancel = 0x40,
		Write = 0x53,
		PrintSetting = 0x78,
		ExtraCharacter = 0x7A,
		String = 0x7C,
		Erase = 0x7D,
		Eject = 0x80,
		Clean = 0xA0, // Multi-step
		GetCard = 0xB0, // Dispense card
	};

	// Commands
	void WMMT_Command_10_Init();
	void WMMT_Command_20_GetStatus();
	void WMMT_Command_33_Read();
	void WMMT_Command_40_Cancel();
	void WMMT_Command_53_Write(std::vector<uint8_t> &packet); // Write the mag strip
	void WMMT_Command_78_PrintSetting();
	void WMMT_Command_7A_ExtraCharacter();
	void WMMT_Command_7C_String(std::vector<uint8_t> &packet); // Write the text on the card
	void WMMT_Command_7D_Erase();
	void WMMT_Command_80_Eject();
	void WMMT_Command_A0_Clean(); // Clean print head
	void WMMT_Command_B0_GetCard();
	//int WMMT_Command_F5_BatteryCheck(); // Not used in WMMT2

	Status RPS;
	bool insertedCard{false};
	bool multiActionCommand{false};
	bool waitingForMoreData{false};
	Commands currentCommand{Commands::NoCommand};
	std::vector<uint8_t> ReceiveBuffer{};
	std::vector<uint8_t> ResponseBuffer{}; // Command Response
	std::vector<uint8_t> ProcessedPacket{};
};

#endif
