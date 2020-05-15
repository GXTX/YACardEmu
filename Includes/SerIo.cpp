#include "SerIo.h"

SerIo::SerIo(char *devicePath)
{
	sp_new_config(&PortConfig);
	sp_set_config_baudrate(PortConfig, 9600);
	sp_set_config_bits(PortConfig, 8);
	sp_set_config_parity(PortConfig, SP_PARITY_NONE);
	sp_set_config_stopbits(PortConfig, 1);
	sp_set_config_flowcontrol(PortConfig, SP_FLOWCONTROL_NONE);

	sp_get_port_by_name(devicePath, &Port);

	sp_return ret = sp_open(Port, SP_MODE_READ_WRITE);

	if (ret != SP_OK) {
		std::printf("SerIo::Init: Failed to open %s.", devicePath);
		std::cout << std::endl;
		IsInitialized = false;
	}
	else {
		IsInitialized = true;
		sp_set_config(Port, PortConfig);
	}
}

SerIo::~SerIo()
{
	sp_close(Port);
}

int SerIo::Write(std::vector<uint8_t> &buffer)
{
#ifdef DEBUG_SERIAL
	std::cout << "SerIo::Write:";
	for(uint8_t i = 0; i < buffer.size(); i++) {
		std::printf(" %02X", buffer.at(i));
	}
	std::cout << std::endl;
#endif
	int ret = sp_nonblocking_write(Port, buffer.data(), buffer.size());

	if (ret <= 0) {
		return StatusCode::WriteError;
	}
	else if (ret != (int)buffer.size()) {
		std::printf("SerIo::Write: Only wrote %02X of %02X to the port!\n", ret, (int)buffer.size());
		return StatusCode::WriteError;
	}

	return StatusCode::Okay;
}

int SerIo::Read(std::vector<uint8_t> &buffer)
{
	// TODO: causing "high" cpu load
	int bytes = sp_input_waiting(Port);

	if (bytes <= 0) {
		// TODO: this doesn't have to mean a readerror, could just be a zero size waiting
		return StatusCode::ReadError;
	}

	buffer.resize(bytes);

	int ret = sp_nonblocking_read(Port, buffer.data(), buffer.size());

	if (ret <= 0) {
		return StatusCode::ReadError;
	}
	else {
		#ifdef DEBUG_SERIAL
		std::cout << "SerIo::Read:";
		for (size_t i = 0; i < buffer.size(); i++) {
			std::printf(" %02X", buffer.at(i));
		}
		std::cout << std::endl;
		#endif
	}

	return StatusCode::Okay;
}
