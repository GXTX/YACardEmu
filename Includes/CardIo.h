/*
    YACardEmu
    ----------------
    Copyright (C) 2020-2025 wutno (https://github.com/GXTX)


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
#include <atomic>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <regex>
#include <array>

#include "Printer.h"

#include "mini/ini.h"

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/fmt/bin_to_hex.h"

#include "ghc/filesystem.hpp"

extern std::shared_ptr<spdlog::async_logger> g_logger;

class CardIo
{
public:
	enum StatusCode {
		Okay,
		SendAck,
		SizeError,
		SyncError,
		SyntaxError,
		ChecksumError,
		EmptyResponseError,
		ServerWaitingReply,
		DontReply,
	};

	struct Settings {
		std::string cardName{};
		std::string cardPath{};
		bool insertedCard = false;
		bool hasCard = false;
		bool waitingForCard = false;
		bool reportDispenserEmpty = false;
		std::string mech = "C1231LR";
	};

	CardIo::StatusCode m_status = StatusCode::Okay;

	CardIo(CardIo::Settings *settings);
	virtual ~CardIo() = default;
	CardIo::StatusCode BuildPacket(std::vector<uint8_t> &readBuffer);
	CardIo::StatusCode ReceivePacket(std::vector<uint8_t> &writeBuffer);
	CardIo::StatusCode CardIo::Process(std::vector<uint8_t>& read, std::vector<uint8_t>& write);

	CardIo::Settings *m_cardSettings = nullptr;
	std::string printName = "print.bin";
protected:
	// Status bytes:
	//////////////////////////////////////////////
	enum class R { 
		NO_CARD, 
		EJECT, 
		READ_WRITE_HEAD, 
		THERMAL_HEAD, 
		DISPENSER_THERMAL, 
		MAX_POSITIONS,
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

	struct Status {
		R r = R::NO_CARD;
		P p = P::NO_ERR;
		S s = S::NO_JOB;

		void Reset()
		{
			p = P::NO_ERR;
			s = S::NO_JOB;
		}

		void SoftReset()
		{
			p = P::NO_ERR;
			s = S::NO_JOB;
		}
	};
	Status status = {};

	const uint8_t START_OF_TEXT = 0x02;
	const uint8_t END_OF_TEXT = 0x03;
	const uint8_t ENQUIRY = 0x05;
	const uint8_t ACK = 0x06;
	const uint8_t NACK = 0x15;
	const uint8_t CARD_SIZE = 0xCF;
	const uint8_t TRACK_SIZE = 0x45;
	const uint8_t NUM_TRACKS = 3;
	const std::string versionString = "AP:S1234-5678,OS:S9012-3456,0000";

	uint8_t GetByte(uint8_t **buffer);
	void HandlePacket();

	std::vector<std::vector<uint8_t>> cardData{{}, {}, {}};

	void UpdateStatusInBuffer();
	void SetPError(P error_code);
	void SetSError(S error_code);

	void ClearCardData();
	void ReadCard();
	void WriteCard();

	// Commands
	virtual void Command_10_Initalize();
	void Command_20_ReadStatus();
	void Command_33_ReadData2(); // multi-track read
	void Command_35_GetData(); // Spit out the entire card
	void Command_40_Cancel();
	void Command_53_WriteData2(); // multi-track write
	void Command_78_PrintSettings2();
	void Command_7A_RegisterFont();
	void Command_7B_PrintImage();
	void Command_7C_PrintL();
	void Command_7D_Erase(); // erase the printed image
	void Command_7E_PrintBarcode();
	void Command_80_EjectCard();
	void Command_A0_Clean();
	void Command_B0_DispenseCardS31();
	void Command_C0_ControlLED();
	void Command_C1_SetPrintRetry();
	virtual void Command_D0_ShutterControl();
	void Command_E1_SetRTC();
	void Command_F0_GetVersion();
	void Command_F1_GetRTC();
	void Command_F5_CheckBattery();

	int currentStep{};
	uint8_t currentCommand{};

	bool runningCommand{false};

	std::vector<uint8_t> currentPacket{};
	std::vector<uint8_t> commandBuffer{0, 0, 0, 0};
	std::vector<uint8_t> printBuffer{};

	std::time_t startTime;
	std::time_t setTime;

	std::unique_ptr<Printer> m_printer = std::make_unique<Printer>();

	// This is what actually maps the "R" position value to a number
	virtual uint8_t GetPositionValue() = 0;

	// Does different actions based on mech and where the card is supposed to be located
	virtual void ProcessNewPosition() = 0;

	virtual bool HasCard()
	{
		return status.r != R::NO_CARD;
	}
	virtual void MoveCard(CardIo::R to) {
		status.r = to;
	}
	virtual void EjectCard()
	{
		if (status.r != R::NO_CARD) {
			MoveCard(R::EJECT);
			WriteCard();
		}
	}
	virtual void DispenseCard()
	{
		if (status.r != R::NO_CARD)
			SetSError(S::ILLEGAL_COMMAND);
		else
			MoveCard(R::DISPENSER_THERMAL);
	}
};

#endif
