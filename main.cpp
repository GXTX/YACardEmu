#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>

#include "CardIo.h"
#include "SerIo.h"

static const std::string serialName = "/dev/ttyUSB1";

constexpr const auto delay = std::chrono::milliseconds(5);

std::atomic<bool> running{true};

void sig_handle(int sig)
{
	if (sig == SIGINT || sig == SIGTERM) {
		running = false;
	}
}

int main()
{
	// Handle quitting gracefully via signals
	std::signal(SIGINT, sig_handle);
	std::signal(SIGTERM, sig_handle);

	std::unique_ptr<CardIo> CardHandler (std::make_unique<CardIo>());

	std::unique_ptr<SerIo> SerialHandler (std::make_unique<SerIo>(serialName.c_str()));
	if (!SerialHandler->IsInitialized) {
		std::cerr << "Coudln't initialize the serial controller.\n";
		return 1;
	}

	CardIo::StatusCode cardStatus;
	SerIo::Status serialStatus;
	std::vector<uint8_t> SerialBuffer;
	std::vector<uint8_t> OutgoingBuffer;

	//CardHandler->LoadCardFromFS();

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
			//SerialHandler->Write(OutgoingBuffer);
		} else if (cardStatus == CardIo::ServerWaitingReply) {
			// Our reply should've already been generated in BuildPacket(); as part of a multi-response command.
			SerialHandler->Write(OutgoingBuffer);
		}

		std::this_thread::sleep_for(delay);
	}

	return 0;
}
