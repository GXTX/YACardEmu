#include <iostream>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>


#include "JvsIo.h"
#include "SerIo.h"

#include <unistd.h>

static const std::string serialName = "/dev/ttyS1";

int main()
{
	std::vector<uint8_t> SerialBuffer;
	SerialBuffer.reserve(255 * 2); //max JVS packet * 2

	// TODO: probably doesn't need to be shared? we only need Inputs to be a shared ptr
	std::shared_ptr<JvsIo> JVSHandler (std::make_shared<JvsIo>());

	std::unique_ptr<SerIo> SerialHandler (std::make_unique<SerIo>(serialName));
	if (!SerialHandler->IsInitialized) {
		std::cerr << "Coudln't initiate the serial controller." << std::endl;
		return 1;
	}

	// TODO: real errors, right now we just return the size processed or the size of the buffer
	int card_status;

	while (true) {
		SerialBuffer.clear();

		if (SerialHandler->Read(SerialBuffer) != SerIo::StatusCode::Okay) {
			// TODO: Handle errors?
			continue;
		}

		card_status = JVSHandler->ReceivePacket(SerialBuffer);
		if (JVSHandler->ReceivePacket(SerialBuffer) < 1) {
			continue;
		}

		// Clear out the buffer before we fill it up again.
		// TODO: Required? JvsIo::GetByte() should be deleting entries as they're processed.
		SerialBuffer.clear();
		card_status = JVSHandler->SendPacket(SerialBuffer);
		if (card_status > 0) {
			// TODO: Handle errors?
			SerialHandler->Write(SerialBuffer);
		} else {
			// TODO: We need to reply with ACK or something.. we can't just not reply.
		}

		// Reading too quickly from the port causes my test system to lockup, so we wait. (Pi3b+)
		std::this_thread::sleep_for(std::chrono::microseconds(1000));
	}

	return 0;
}
