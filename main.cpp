#include <iostream>
#include <chrono>
#include <thread>

#include "CardIo.h"
#include "SerIo.h"

static const std::string serialName = "/dev/ttyS0";

int main()
{
	std::unique_ptr<CardIo> CardHandler (std::make_unique<CardIo>());

	std::unique_ptr<SerIo> SerialHandler (std::make_unique<SerIo>(serialName));
	if (!SerialHandler->IsInitialized) {
		std::cerr << "Coudln't initialize the serial controller.\n";
		return 1;
	}

	CardIo::StatusCode cardStatus;
	SerIo::StatusCode serialStatus;
	std::vector<uint8_t> SerialBuffer;
	std::vector<uint8_t> OutgoingBuffer;

	while (true) {
		if (!SerialBuffer.empty()) {
			SerialBuffer.clear();
		}

		serialStatus = SerialHandler->Read(&SerialBuffer);
		if (serialStatus != SerIo::Okay) {
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
			continue;
		}

		cardStatus = CardHandler->ReceivePacket(&SerialBuffer);
		if (cardStatus == CardIo::Okay) {
			// Build our reply packet in preperation for the ENQ byte from the server.
			cardStatus = CardHandler->BuildPacket(&OutgoingBuffer);
			
			// ReceivePacket should've cleared out this buffer and appended ACK to it.
			SerialHandler->Write(&SerialBuffer);
		} else if (cardStatus == CardIo::ServerWaitingReply) {
			// Our reply should've already been generated in BuildPacket();
			SerialHandler->Write(&OutgoingBuffer);
		}

		// TODO: Experiment with longer waits.
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
	}

	return 0;
}
