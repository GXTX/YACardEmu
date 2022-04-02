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

#ifndef CARDIO_H
#define CARDIO_H

#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <atomic>
#include <ctime>

#include "spdlog/spdlog.h"
#include "spdlog/fmt/bin_to_hex.h"

#include "ICUConv.h"

// Status bytes:
//////////////////////////////////////////////
enum class R {
	NO_CARD           = 0x30,
	HAS_CARD_1        = 0x31,
	CARD_STATUS_ERROR = 0x32,
	HAS_CARD_2        = 0x33,
	EJECTING_CARD     = 0x34,
};

enum class P {
	NO_ERR                 = 0x30,
	READ_ERR               = 0x31,
	WRITE_ERR              = 0x32,
	CARD_JAM               = 0x33,
	MOTOR_ERR              = 0x34, // transport system motor error
	PRINT_ERR              = 0x35,
	ILLEGAL_ERR            = 0x38, // generic error
	BATTERY_ERR            = 0x40, // low battery voltage
	SYSTEM_ERR             = 0x41, // reader/writer system err
	TRACK_1_READ_ERR       = 0x51,
	TRACK_2_READ_ERR       = 0x52,
	TRACK_3_READ_ERR       = 0x53,
	TRACK_1_AND_2_READ_ERR = 0x54,
	TRACK_1_AND_3_READ_ERR = 0x55,
	TRACK_2_AND_3_READ_ERR = 0x56,
};

enum class S {
	NO_JOB           = 0x30,
	ILLEGAL_COMMAND  = 0x32,
	RUNNING_COMMAND  = 0x33,
	WAITING_FOR_CARD = 0x34,
	DISPENSER_EMPTY  = 0x35,
	NO_DISPENSER     = 0x36,
	CARD_FULL        = 0x37,
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

	void SoftReset()
	{
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
		SyntaxError,
		ChecksumError,
		EmptyResponseError,
		ServerWaitingReply,
	};

	CardIo(bool *insertedCard, std::string *basePath, std::string *cardName);
	CardIo::StatusCode BuildPacket(std::vector<uint8_t> &readBuffer);
	CardIo::StatusCode ReceivePacket(std::vector<uint8_t> &writeBuffer);

	bool *insertedCard;
	std::string *cardName;
	std::string *basePath;
	std::string printName = "print.bin";
	void LoadCardFromFS();
	void SaveCardToFS();
private:
	const uint8_t START_OF_TEXT = 0x02;
	const uint8_t END_OF_TEXT = 0x03;
	const uint8_t ENQUIRY = 0x05;
	const uint8_t ACK = 0x06;
	const uint8_t CARD_SIZE = 0x45; // track size
	const std::string versionString = "AP:S1234-5678,OS:S9012-3456,0000";

	uint8_t GetByte(uint8_t **buffer);
	void HandlePacket();

	std::vector<uint8_t>cardData{};
	std::vector<uint8_t>backupCardData{}; // Filled with the data when we first loaded the card.

	void UpdateStatusBytes();
	void UpdateStatusInBuffer();
	void SetPError(P error_code);
	void SetSError(S error_code);

	// Commands
	void Command_10_Initalize();
	void Command_20_ReadStatus();
	void Command_33_ReadData2(); // multi-track read
	void Command_40_Cancel();
	void Command_53_WriteData2(); // multi-track write
	void Command_78_PrintSettings2();
	void Command_7A_RegisterFont(); // "foreign characters"
	void Command_7B_PrintImage();
	void Command_7C_PrintL();
	void Command_7D_Erase(); // erase the printed image
	void Command_7E_PrintBarcode();
	void Command_80_EjectCard();
	void Command_A0_Clean();
	void Command_B0_DispenseCardS31();
	//void Command_E1_SetRTC(); // Req for WMMT3
	void Command_F0_GetVersion();
	void Command_F1_GetRTC();
	void Command_F5_CheckBattery();

	Status status;
	int currentStep{};
	uint8_t currentCommand{};

	bool runningCommand{false};

	std::vector<uint8_t> currentPacket{};
	std::vector<uint8_t> commandBuffer{};
	std::vector<uint8_t> printBuffer{};
};

#endif
