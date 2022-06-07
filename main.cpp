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

#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include <string>

#include "C1231LR.h"
#include "SerIo.h"
#include "base64.h"

#include "httplib.h"
#include "mini/ini.h"
#include "spdlog/spdlog.h"
#include "ghc/filesystem.hpp"

// Globals
static const auto delay{std::chrono::microseconds(250)};
std::atomic<bool> running{true};
//

void sigHandler(int sig)
{
	if (sig == SIGINT || sig == SIGTERM) {
		running = false;
	}
}

std::string generateCardListJSON(std::string basepath)
{
	std::string list{"["};

	for (const auto &entry: ghc::filesystem::directory_iterator(basepath)) {
		std::string card{entry.path().string()};

		auto find = card.find(".track_0");

		if (find != std::string::npos) {
			card.replace(find, 8, "");
			list.append("{\"name\":\"");
#ifdef _WIN32
			list.append(card.substr(card.find_last_of("\\") + 1));
#else
			list.append(card.substr(card.find_last_of("/") + 1));
#endif
			list.append("\",\"image\":\"");

			auto find2 = card.find(".bin");
			if (find2 != std::string::npos)
				card.replace(find2, 4, ".png");
			
			std::string base64{};

			if (ghc::filesystem::exists(card)) {
				std::ifstream img(card.c_str(), std::ifstream::in | std::ifstream::binary);

				base64.resize(ghc::filesystem::file_size(card));

				img.read(reinterpret_cast<char *>(&base64[0]), base64.size());
				img.close();
			}

			std::string encoded = base64_encode(base64, false);
			list.append("data:image/png;base64, ");
			list.append(encoded);
			list.append("\"},");
		}
	}

	// remove the errant comma
	if (list.compare("[") != 0) {
		list.pop_back();
	}

	list.append("]");
	return list;
}

void httpServer(int port, C1231LR::Settings *card)
{
	httplib::Server svr;

	svr.set_mount_point("/", "public");

	svr.Get("/actions", [&card](const httplib::Request &req, httplib::Response &res) {
		if (req.has_param("list")) {
			res.set_content(generateCardListJSON(card->cardPath), "application/json");
		} else {
			if (req.has_param("insert")) {
				card->insertedCard = true;
			} else if (req.has_param("remove")) {
				card->insertedCard = false;
			}

			if (req.has_param("cardname")) {
				card->cardName = req.get_param_value("cardname");
			}

			if (req.has_param("dispenser")) {
				if (req.get_param_value("dispenser").compare("true") == 0) {
					card->reportDispenserEmpty = true;
				} else {
					card->reportDispenserEmpty = false;
				}
			}

			res.set_redirect("/");
		}
	});

	svr.Get("/stop", [&svr](const httplib::Request &, httplib::Response &) {
		spdlog::info("Stopping application...");
		running = false;
		svr.stop();
	});

	svr.listen("0.0.0.0", port);
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
	spdlog::set_level(spdlog::level::warn);
#else
	spdlog::set_level(spdlog::level::debug);
#endif
	spdlog::set_pattern("[%^%l%$] %v");

	spdlog::enable_backtrace(15);

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

	spdlog::warn("Starting API server...");
	std::thread(httpServer, httpPort, &cardHandler->cardSettings).detach();

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
