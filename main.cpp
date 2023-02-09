/*
    YACardEmu
    ----------------
    Copyright (C) 2020-2023 wutno (https://github.com/GXTX)


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

#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include <string>

#include "C1231LR.h"
#include "C1231BR.h"
#include "SerIo.h"
#include "WebIo.h"

#include "httplib.h"
#include "mini/ini.h"
#include "spdlog/spdlog.h"
#include "ghc/filesystem.hpp"

// Globals
struct Settings {
	CardIo::Settings card{};
	SerIo::Settings serial{};
	int webPort = 8080;
};
Settings globalSettings{};
std::atomic<bool> running{true};
constexpr static auto delay = std::chrono::microseconds(250);
//

void SigHandler(int sig)
{
	if (sig == SIGINT || sig == SIGTERM) {
		running = false;
	}
}

bool ReadConfig()
{
	// Read in config values
	mINI::INIFile config("config.ini");
	mINI::INIStructure ini;
	std::string lport, lbaud, lparity;
	
	// TODO: Generate INI
	if (!config.read(ini)) {
		spdlog::critical("Unable to open config.ini!");
		return false;
	}

	if (ini.has("config")) {
		lport = ini["config"]["apiport"];
		lbaud = ini["config"]["serialbaud"];
		lparity = ini["config"]["serialparity"];
		globalSettings.card.mech = ini["config"]["targetdevice"];
		globalSettings.card.cardPath = ini["config"]["basepath"];
		globalSettings.card.cardName = ini["config"]["autoselectedcard"];
		globalSettings.serial.devicePath = ini["config"]["serialpath"];
	}

	if (globalSettings.card.cardPath.empty()) {
		globalSettings.card.cardPath = ghc::filesystem::current_path().string();
	}

	if (lport.empty()) {
		globalSettings.webPort = 8080;
	} else { 
		globalSettings.webPort = std::stoi(lport);
	}

	if (globalSettings.serial.devicePath.empty()) {
		globalSettings.serial.devicePath = "/dev/ttyUSB1";
	}

	if (lbaud.empty()) {
		globalSettings.serial.baudRate = 9600;
	} else {
		globalSettings.serial.baudRate = std::stoi(lbaud);
	}

	if (lparity.empty()) {
		globalSettings.serial.parity = SP_PARITY_NONE;
	} else {
		if (lparity.find("even") != std::string::npos) {
			globalSettings.serial.parity = SP_PARITY_EVEN;
		} else {
			globalSettings.serial.parity = SP_PARITY_NONE;
		}
	}

	return true;
}

int main()
{
#ifdef NDEBUG
	spdlog::set_level(spdlog::level::info);
#else
	spdlog::set_level(spdlog::level::debug);
	spdlog::enable_backtrace(20);
#endif
	spdlog::set_pattern("[%^%l%$] %v");

	// Handle quitting gracefully via signals
	std::signal(SIGINT, SigHandler);
	std::signal(SIGTERM, SigHandler);

	if (!ReadConfig()) {
		return 1;
	}

	std::unique_ptr<CardIo> cardHandler{};
	if (globalSettings.card.mech.compare("C1231LR") == 0) {
		cardHandler = std::make_unique<C1231LR>(&globalSettings.card);
	} else if (globalSettings.card.mech.compare("C1231BR") == 0) {
		cardHandler = std::make_unique<C1231BR>(&globalSettings.card);
	} else {
		spdlog::critical("Invalid target device: {}", globalSettings.card.mech);
		return 1;
	}

	std::unique_ptr<SerIo> serialHandler = std::make_unique<SerIo>(&globalSettings.serial);
	if (!serialHandler->Open()) {
		return 1;
	}

	// TODO: Verify service is actually running
	std::unique_ptr<WebIo> webHandler = std::make_unique<WebIo>(&globalSettings.card, globalSettings.webPort, &running);
	webHandler->Spawn();

	// TODO: These don't need to be here, put them in their respective classes
	SerIo::Status serialStatus = SerIo::Status::Okay;
	CardIo::StatusCode cardStatus = CardIo::StatusCode::Okay;
	std::vector<uint8_t> readBuffer{};
	std::vector<uint8_t> writeBuffer{};

	while (running) {
		serialStatus = serialHandler->Read(readBuffer);

		// TODO: device read/write should probably be a separate thread
		if (serialStatus != SerIo::Status::Okay && readBuffer.empty()) {
			std::this_thread::sleep_for(delay);
			continue;
		}

		cardStatus = cardHandler->ReceivePacket(readBuffer);

		if (cardStatus == CardIo::Okay) {
			// We need to send our ACK as quick as possible
			serialHandler->SendAck();
		} else if (cardStatus == CardIo::ServerWaitingReply) {
			// Do not reply until we get this command
			writeBuffer.clear();
			cardHandler->BuildPacket(writeBuffer);
			serialHandler->Write(writeBuffer);
		}

		spdlog::dump_backtrace();
		std::this_thread::sleep_for(delay);
	}

	return 0;
}
