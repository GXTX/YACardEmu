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

#include "CardIo.h"
#include "SerIo.h"

static const std::string serialName = "/dev/ttyUSB1";

static const auto delay = std::chrono::milliseconds(10);

std::atomic<bool> running{true};
std::atomic<bool> insertCard{false};

void sig_handle(int sig)
{
	if (sig == SIGINT || sig == SIGTERM) {
		running = false;
	}
}

void cin_handler()
{
	std::string in{};

	while (1) {
		in.clear();
		std::getline(std::cin, in);
		if (in.compare("i") == 0) {
			insertCard = true;
		} else if (in.compare("o") == 0) {
			insertCard = false;
		}
		std::this_thread::sleep_for(delay);
	}
}

int main()
{
	// Handle quitting gracefully via signals
	std::signal(SIGINT, sig_handle);
	std::signal(SIGTERM, sig_handle);

	std::unique_ptr<CardIo> CardHandler (std::make_unique<CardIo>(&insertCard));

	std::unique_ptr<SerIo> SerialHandler (std::make_unique<SerIo>(serialName.c_str()));
	if (!SerialHandler->IsInitialized) {
		std::cerr << "Coudln't initialize the serial controller.\n";
		return 1;
	}

	CardIo::StatusCode cardStatus;
	SerIo::Status serialStatus;
	std::vector<uint8_t> SerialBuffer;
	std::vector<uint8_t> OutgoingBuffer;

	// Handle card inserting.
	std::thread(cin_handler).detach();

	while (running) {
		if (!SerialBuffer.empty()) {
			SerialBuffer.clear();
		}

		serialStatus = SerialHandler->Read(SerialBuffer);
		if (serialStatus != SerIo::Status::Okay) {
			std::this_thread::sleep_for(delay);
			continue;
		}

		cardStatus = CardHandler->ReceivePacket(SerialBuffer);
		if (cardStatus == CardIo::Okay) {
			// Build our reply packet in preperation for the ENQ byte from the server.
			cardStatus = CardHandler->BuildPacket(OutgoingBuffer);
			
			// ReceivePacket should've cleared out this buffer and appended ACK to it.
			SerialHandler->Write(SerialBuffer);
			std::this_thread::sleep_for(delay / 2);
			//SerialHandler->Write(OutgoingBuffer); // Is this correct, should we send our reply directly after ACKing?
		} else if (cardStatus == CardIo::ServerWaitingReply) {
			// Our reply should've already been generated in BuildPacket(); as part of a multi-response command.
			SerialHandler->Write(OutgoingBuffer);
		}

		std::this_thread::sleep_for(delay);
	}

	return 0;
}
