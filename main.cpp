#include <iostream>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>

#include "CardIo.h"
#include "SerIo.h"

static const std::string serialName = "/dev/ttyS1";

int main()
{
	std::vector<uint8_t> *SerialBuffer = 0;
	SerialBuffer->reserve(255 * 2); //max packet * 2, not required, really..

	std::unique_ptr<CardIo> CardHandler (std::make_unique<CardIo>());

	std::unique_ptr<SerIo> SerialHandler (std::make_unique<SerIo>(serialName));
	if (!SerialHandler->IsInitialized) {
		std::cerr << "Coudln't initialize the serial controller.";
		return 1;
	}

	CardIo::StatusCode cardStatus;
	SerIo::StatusCode serialStatus;

	while (true) {
		SerialBuffer->clear();

		serialStatus = SerialHandler->Read(SerialBuffer);
		if (serialStatus != SerIo::Okay) {
			continue;
		}

		cardStatus = CardHandler->ReceivePacket(SerialBuffer);
		if (cardStatus == CardIo::Okay) {
			// Write our ACK
			serialStatus = SerialHandler->Write(SerialBuffer);
		} else if (cardStatus == CardIo::ServerWaitingReply) {
			// Write our actual reply once we get the signal the host is waiting.
			cardStatus = CardHandler->BuildPacket(SerialBuffer);
			if (cardStatus == CardIo::Okay) {
				serialStatus = SerialHandler->Write(SerialBuffer);
			}
		} else {
			// TODO: Could mean a couple of differnet things, should we consider exiting for some?
			continue;
		}

		// Reading too quickly from the port causes my test system to lockup, so we wait. (Pi3b+)
		std::this_thread::sleep_for(std::chrono::microseconds(1000));
	}

	return 0;
}
