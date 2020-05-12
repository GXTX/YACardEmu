#include <iostream>
#include <memory>

#include "JvsIo.h"
#include "SerIo.h"

#include <sched.h>

int main()
{
	char serialName[13];
	std::sprintf(serialName, "/dev/ttyUSB4");

	// Set thread priority to RT. We don't care if this
	// fails but may be required for some systems.
	struct sched_param params;
	params.sched_priority = sched_get_priority_max(SCHED_FIFO);
	pthread_setschedparam(pthread_self(), SCHED_FIFO, &params);

	// TODO: maybe put set these as shared in serio?
	std::vector<uint8_t> ReadBuffer;
	std::vector<uint8_t> WriteBuffer;

	// TODO: probably doesn't need to be shared? we only need Inputs to be a shared ptr
	std::shared_ptr<JvsIo> JVSHandler (std::make_shared<JvsIo>());

	std::unique_ptr<SerIo> SerialHandler (std::make_unique<SerIo>(serialName));
	if (!SerialHandler->IsInitialized) {
		std::cerr << "Coudln't initiate the serial controller." << std::endl;
		return 1;
	}

	while (true) {
		ReadBuffer.resize(512);
		SerialHandler->Read(ReadBuffer.data());

		if (ReadBuffer.size() > 0) {
			int temp = JVSHandler->ReceivePacket(ReadBuffer.data());
			// TODO: Why without this does it fault?
			WriteBuffer.resize(ReadBuffer.size());
			ReadBuffer.clear();

			int ret = JVSHandler->SendPacket(WriteBuffer.data());

			if (ret > 0) {
				SerialHandler->Write(WriteBuffer.data(), ret);
				WriteBuffer.clear();
			}
		}
	}

	return 0;
}
