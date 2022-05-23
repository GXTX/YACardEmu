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
#include <filesystem>
#include <string>

#include "C1231LR.h"
#include "SerIo.h"
#include "base64.h"

#include "httplib.h"
#include "mini/ini.h"
#include "spdlog/spdlog.h"

// Globals
static const auto delay{std::chrono::milliseconds(25)};

std::atomic<bool> running{true};

bool insertedCard = false;
bool reportDispenserEmpty = false;
std::string cardName{};
std::string cardPath{}; // Full base path
std::string httpPort{};
std::string serialName{};
//

void sigHandler(int sig)
{
	if (sig == SIGINT || sig == SIGTERM) {
		running = false;
	}
}

std::string generateCardListJSON()
{
	std::string list{"["};

	for (const auto &entry: std::filesystem::directory_iterator(cardPath)) {
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

			if (std::filesystem::exists(card)) {
				std::ifstream img(card.c_str(), std::ifstream::in | std::ifstream::binary);

				base64.resize(std::filesystem::file_size(card));

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

void httpServer()
{
	httplib::Server svr;

	svr.set_mount_point("/", "public");

	svr.Get("/actions", [](const httplib::Request &req, httplib::Response &res) {
		if (req.has_param("list")) {
			res.set_content(generateCardListJSON(), "application/json");
		} else {
			if (req.has_param("insert")) {
				insertedCard = true;
			} else if (req.has_param("remove")) {
				insertedCard = false;
			}

			if (req.has_param("cardname")) {
				cardName = req.get_param_value("cardname");
			}

			if (req.has_param("dispenser")) {
				if (req.get_param_value("dispenser").compare("true") == 0) {
					reportDispenserEmpty = true;
				} else {
					reportDispenserEmpty = false;
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

	svr.listen("0.0.0.0", std::stoi(httpPort));
}

bool readConfig()
{
	// Read in config values
	mINI::INIFile config("config.ini");

	mINI::INIStructure ini;
	
	if (!config.read(ini)) {
		// TODO: Generate INI
		spdlog::critical("Unable to open config.ini!");
		return false;
	}

	if (ini.has("config")) {
		cardPath = ini["config"]["basepath"];
		cardName = ini["config"]["autoselectedcard"]; // can be empty, we can select via api
		httpPort = ini["config"]["apiport"];
		serialName = ini["config"]["serialpath"];
	}

	if (cardPath.empty()) {
		cardPath = std::filesystem::current_path().string();
	}

	if (httpPort.empty()) {
		httpPort = "8080";
	}

	if (serialName.empty()) {
		serialName = "/dev/ttyUSB1";
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

	// Handle quitting gracefully via signals
	std::signal(SIGINT, sigHandler);
	std::signal(SIGTERM, sigHandler);

	if (!readConfig()) {
		return 1;
	}

	std::unique_ptr<C1231LR> cardHandler (std::make_unique<C1231LR>(&insertedCard, &cardPath, &cardName, &reportDispenserEmpty));

	std::unique_ptr<SerIo> serialHandler (std::make_unique<SerIo>(serialName));
	if (!serialHandler->IsInitialized) {
		spdlog::critical("Couldn't initalize the serial controller.");
		return 1;
	}

	spdlog::info("Starting API server...");
	std::thread(httpServer).detach();

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

		std::this_thread::sleep_for(delay);
	}

	return 0;
}
