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

#include "CardIo.h"
#include "SerIo.h"

#include "httplib.h"

// Globals
static const std::string serialName = "/dev/ttyUSB1"; // TODO: runtime configure

static const auto delay = std::chrono::milliseconds(5);

std::atomic<bool> running{true};
std::atomic<bool> insertCard{false};

bool insertedCard = false;
std::string cardName{}; // Contains the FULL path
static const std::string cardPath{"/home/wutno/Projects/YACardEmu/build"}; // TODO: runtime configure
//

void sigHandler(int sig)
{
	if (sig == SIGINT || sig == SIGTERM) {
		running = false;
	}
}

void httpServer()
{
	httplib::Server svr;

	svr.Get("/list", [](const httplib::Request &, httplib::Response &res) {
		std::string list{};

		for (const auto &entry: std::filesystem::directory_iterator(cardPath)) {
			list.append(entry.path());
			list.append("\n");
		}

		res.set_content(list, "text/plain");
	});

	svr.Post("/actions", [](const httplib::Request &req, httplib::Response) {
		if (req.has_param("insert")) {
			insertedCard = true;
		} else if (req.has_param("remove")) {
			insertedCard = false;
		}

		if (req.has_param("name")) {
			cardName = req.get_param_value("name");
		}
	});

	svr.Get("/stopServer", [&svr](const httplib::Request &, httplib::Response &) {
		std::puts("Stopping API server...");
		svr.stop();
	});

	svr.Get("/stop", [](const httplib::Request &, httplib::Response &) {
		std::puts("Stopping application...");
		running = false;
	});

	svr.listen("0.0.0.0", 8080);
}

int main()
{
	// Handle quitting gracefully via signals
	std::signal(SIGINT, sigHandler);
	std::signal(SIGTERM, sigHandler);

	std::unique_ptr<CardIo> cardHandler (std::make_unique<CardIo>(&insertedCard, &cardName));

	std::unique_ptr<SerIo> serialHandler (std::make_unique<SerIo>(serialName.c_str()));
	if (!serialHandler->IsInitialized) {
		std::cerr << "Coudln't initialize the serial controller.\n";
		return 1;
	}

	// Handle card inserting.
	std::puts("Starting API server...");
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
