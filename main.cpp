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
#include "SerIo.h"
#include "WebIo.h"

#include "httplib.h"
#include "mini/ini.h"
#include "spdlog/spdlog.h"
#include "ghc/filesystem.hpp"

// Globals
static constexpr auto delay{std::chrono::microseconds(250)};
std::atomic<bool> running{true};
//

void sigHandler(int sig)
{
	if (sig == SIGINT || sig == SIGTERM) {
		running = false;
	}
}

bool readConfig(SerIo::Settings &serial, C1231LR::Settings &card, int *port)
{
	// Read in config values
	mINI::INIFile config("config.ini");

	mINI::INIStructure ini;

	std::string lport, lbaud, lparity;
	
	if (!config.read(ini)) {
		// TODO: Generate INI
		spdlog::critical("Unable to open config.ini!");
		return false;
	}

	if (ini.has("config")) {
		card.cardPath = ini["config"]["basepath"];
		card.cardName = ini["config"]["autoselectedcard"]; // can be empty, we can select via api
		lport = ini["config"]["apiport"];
		serial.devicePath = ini["config"]["serialpath"];
		lbaud = ini["config"]["serialbaud"];
		lparity = ini["config"]["serialparity"];
	}

	if (card.cardPath.empty()) {
		card.cardPath = ghc::filesystem::current_path().string();
	}

	if (lport.empty()) {
		*port = 8080;
	} else { 
		*port = std::stoi(lport);
	}

	if (serial.devicePath.empty()) {
		serial.devicePath = "/dev/ttyUSB1";
	}

	if (lbaud.empty()) {
		serial.baudrate = 9600;
	} else {
		serial.baudrate = std::stoi(lbaud);
	}

	if (lparity.empty()) {
		serial.parity = SP_PARITY_NONE;
	} else {
		if (lparity.find("even") != std::string::npos) {
			serial.parity = SP_PARITY_EVEN;
		} else {
			serial.parity = SP_PARITY_NONE;
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
	std::signal(SIGINT, sigHandler);
	std::signal(SIGTERM, sigHandler);

	C1231LR *cardHandler = new C1231LR();
	SerIo *serialHandler = new SerIo();
	int httpPort = 8080;

	if (!readConfig(serialHandler->portSettings, cardHandler->cardSettings, &httpPort)) {
		return 1;
	}

	if (!serialHandler->Open()) {
		spdlog::critical("Couldn't initalize the serial controller.");
		return 1;
	}

	//TODO: Make sure the service is actually running
	WebIo *web = new WebIo(&cardHandler->cardSettings, httpPort);

	SerIo::Status serialStatus;
	CardIo::StatusCode cardStatus;
	std::vector<uint8_t> readBuffer{};
	std::vector<uint8_t> writeBuffer{};

	while (running) {
		serialStatus = serialHandler->Read(readBuffer);

		if (serialStatus != SerIo::Status::Okay && readBuffer.empty()) {
			// TODO: device read/write should probably be a separate thread
			std::this_thread::sleep_for(delay);
			continue;
		}

		cardStatus = cardHandler->ReceivePacket(readBuffer);

		if (cardStatus == CardIo::Okay) {
			// We need to send our ACK as quick as possible even if it takes us time to handle the command.
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
