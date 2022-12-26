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

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <vector>
#include <iostream>
#include <fstream>
#include <atomic>
#include <ctime>
#include <sstream>
#include <iomanip>

#include "spdlog/spdlog.h"
#include "spdlog/fmt/bin_to_hex.h"
#include "ghc/filesystem.hpp"

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
		DontReply,
	};

	struct Settings {
		std::string cardName{};
		std::string cardPath{};
		bool insertedCard{false};
		bool reportDispenserEmpty{false};
	};

	CardIo();
	CardIo::StatusCode BuildPacket(std::vector<uint8_t> &readBuffer);
	CardIo::StatusCode ReceivePacket(std::vector<uint8_t> &writeBuffer);

	Settings cardSettings;
	std::string printName = "print.bin";
protected:
	// Status bytes:
	//////////////////////////////////////////////
	enum class MovePositions {
		NO_CARD,
		READ_WRITE_HEAD,
		THERMAL_HEAD,
		DISPENSER_THERMAL,
		EJECT
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

	const uint8_t START_OF_TEXT = 0x02;
	const uint8_t END_OF_TEXT = 0x03;
	const uint8_t ENQUIRY = 0x05;
	const uint8_t ACK = 0x06;
	const uint8_t CARD_SIZE = 0xCF;
	const uint8_t TRACK_SIZE = 0x45;
	const uint8_t NUM_TRACKS = 3;
	const std::string versionString = "AP:S1234-5678,OS:S9012-3456,0000";

	uint8_t GetByte(uint8_t **buffer);
	void HandlePacket();

	std::vector<std::vector<uint8_t>> cardData{{}, {}, {}};

	void UpdateStatusBytes();
	void UpdateStatusInBuffer();
	void SetPError(P error_code);
	void SetSError(S error_code);
	bool ReadTrack(std::vector<uint8_t> &trackData, int trackNumber);
	void WriteTrack(std::vector<uint8_t> &trackData, int trackNumber);

	// Commands
	void Command_10_Initalize();
	void Command_20_ReadStatus();
	void Command_33_ReadData2(); // multi-track read
	void Command_40_Cancel();
	void Command_53_WriteData2(); // multi-track write
	void Command_78_PrintSettings2();
	void Command_7A_RegisterFont();
	void Command_7B_PrintImage();
	void Command_7C_PrintL();
	void Command_7D_Erase(); // erase the printed image
	void Command_7E_PrintBarcode();
	void Command_80_EjectCard();
	virtual void Command_A0_Clean();
	void Command_B0_DispenseCardS31();
	virtual void Command_D0_ShutterControl();
	void Command_E1_SetRTC();
	void Command_F0_GetVersion();
	void Command_F1_GetRTC();
	void Command_F5_CheckBattery();

	Status status;
	int currentStep{};
	uint8_t currentCommand{};

	bool runningCommand{false};

	std::vector<uint8_t> currentPacket{};
	std::vector<uint8_t> commandBuffer{0, 0, 0, 0};
	std::vector<uint8_t> printBuffer{};

	std::time_t startTime{};
	std::time_t setTime{};

	virtual uint8_t GetRStatus() = 0;
	virtual void UpdateRStatus() = 0;

	virtual bool HasCard() = 0;
	virtual void DispenseCard() = 0;
	virtual void EjectCard() = 0;
	virtual void MoveCard(MovePositions position) = 0;
	virtual CardIo::MovePositions GetCardPos() = 0;
};

#endif
