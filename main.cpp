#include <iostream>
#include <memory>

#include "JvsIo.h"
#include "SerIo.h"

#include <unistd.h>

int main()
{
	char serialName[13];
	std::sprintf(serialName, "/dev/ttyUSB0");

	std::vector<uint8_t> SerialBuffer;
	SerialBuffer.reserve(255 * 2); //max JVS packet * 2

	// TODO: probably doesn't need to be shared? we only need Inputs to be a shared ptr
	std::shared_ptr<JvsIo> JVSHandler (std::make_shared<JvsIo>());

	std::unique_ptr<SerIo> SerialHandler (std::make_unique<SerIo>(serialName));
	if (!SerialHandler->IsInitialized) {
		std::cerr << "Coudln't initiate the serial controller." << std::endl;
		return 1;
	}

	int ret;

	while (true) {
		ret = SerialHandler->Read(SerialBuffer);
	
		if (ret == SerIo::StatusCode::Okay) {
			ret = JVSHandler->ReceivePacket(SerialBuffer);

			if (ret > 1) {
				SerialBuffer.clear();
				ret = JVSHandler->SendPacket(SerialBuffer);

				if (ret > 0) {
					SerialHandler->Write(SerialBuffer);
				}
			}
		}

		SerialBuffer.clear();

		usleep(500);
	}

	return 0;
}
