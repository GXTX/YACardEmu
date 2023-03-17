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
#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "ghc/filesystem.hpp"

// Globals
struct Settings {
	CardIo::Settings card{};
	SerIo::Settings serial{};
	std::string webListenHost;
	int webPort = 8080;
};
Settings globalSettings{};
std::atomic<bool> running{true};
constexpr static auto delay = std::chrono::microseconds(250);
std::shared_ptr<spdlog::async_logger> logger;

const char *helptext = 
	"YACardEmu - A simulator for magnetic card readers\n"
	"Commandline arguments:\n"
	"-d : debug log level\n"
	"-t : trace log level\n"
	"-f : log to yacardemu.log\n"
	"-h : show this help text\n"
	"\n";

//


void SigHandler(int sig)
{
	if (sig == SIGINT || sig == SIGTERM) {
		logger->flush();
		running = false;
	}
}

bool ReadConfig()
{
	// Read in config values
	mINI::INIFile config("config.ini");
	mINI::INIStructure ini;
	std::string lhost, lport, lbaud, lparity;
	
	// TODO: Generate INI
	if (!config.read(ini)) {
		spdlog::critical("Unable to open config.ini!");
		return false;
	}

	if (ini.has("config")) {
		lhost = ini["config"]["apihost"];
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

	if (lhost.empty()) {
		globalSettings.webListenHost = "0.0.0.0";
	} else {
		globalSettings.webListenHost = lhost;
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

int main(int argc, char *argv[])
{
	// Handle quitting gracefully via signals
	std::signal(SIGINT, SigHandler);
	std::signal(SIGTERM, SigHandler);

	// Parse args

#ifdef NDEBUG
	spdlog::level::level_enum log_level = spdlog::level::info;
#else
	spdlog::level::level_enum log_level = spdlog::level::debug;
#endif

	bool file_log = false;
	if (argc > 1) {
		for (int i = 1; i < argc; i++) {
			std::string arg = argv[i];
			while (arg[0] == '-') arg.erase(0,1);
			switch (arg[0]) {
				case 'd': 
					log_level = spdlog::level::debug;
					break;
				case 't': 
					log_level = spdlog::level::trace;
					break;
				case 'f':
					file_log = true;
					break;
				case 'h':
				default:
					std::cout << helptext;
					return 0;
			}
		}
	}

	// Set up logger
	spdlog::init_thread_pool(8192, 1);
	auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt >();
	std::vector<spdlog::sink_ptr> sinks;

	if (file_log) {
		auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("yacardemu.log", true);
		sinks.push_back(file_sink);
	}

	sinks.push_back(stdout_sink);
	logger = std::make_shared<spdlog::async_logger>("main", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
	logger->set_level(log_level);
	logger->flush_on(spdlog::level::info);
	logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

	if (!ReadConfig()) {
		return 1;
	}

	std::unique_ptr<CardIo> cardHandler{};
	if (globalSettings.card.mech.compare("C1231LR") == 0 || globalSettings.card.mech.compare("S31R") == 0) {
		cardHandler = std::make_unique<C1231LR>(&globalSettings.card);
	} else if (globalSettings.card.mech.compare("C1231BR") == 0) {
		cardHandler = std::make_unique<C1231BR>(&globalSettings.card);
	} else {
		logger->critical("Invalid target device: {}", globalSettings.card.mech);
		return 1;
	}

	std::unique_ptr<SerIo> serialHandler = std::make_unique<SerIo>(&globalSettings.serial);
	if (!serialHandler->Open()) {
		return 1;
	}

	std::unique_ptr<WebIo> webHandler = std::make_unique<WebIo>(&globalSettings.card, globalSettings.webListenHost, globalSettings.webPort, &running);
	if (!webHandler->Spawn()) {
		return 1;
	}

	// TODO: These don't need to be here, put them in their respective classes
	SerIo::Status serialStatus = SerIo::Status::Okay;
	CardIo::StatusCode cardStatus = CardIo::StatusCode::Okay;

	while (running) {
		serialStatus = serialHandler->Read();

		// TODO: device read/write should probably be a separate thread
		if (serialStatus != SerIo::Status::Okay && serialHandler->m_readBuffer.empty()) {
			std::this_thread::sleep_for(delay);
			continue;
		}

		cardStatus = cardHandler->ReceivePacket(serialHandler->m_readBuffer);

		if (cardStatus == CardIo::Okay) {
			// We need to send our ACK as quick as possible
			serialHandler->SendAck();
		} else if (cardStatus == CardIo::ServerWaitingReply) {
			// Do not reply until we get this command
			cardHandler->BuildPacket(serialHandler->m_writeBuffer);
			serialHandler->Write();
		}
		
		std::this_thread::sleep_for(delay);
	}

	return 0;
}
