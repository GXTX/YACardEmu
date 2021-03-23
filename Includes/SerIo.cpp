#include "SerIo.h"

SerIo::SerIo(const std::string devicePath)
{
	sp_new_config(&PortConfig);
	sp_set_config_baudrate(PortConfig, 9600);
	sp_set_config_bits(PortConfig, 8);
	sp_set_config_parity(PortConfig, SP_PARITY_NONE);
	sp_set_config_stopbits(PortConfig, 1);
	sp_set_config_flowcontrol(PortConfig, SP_FLOWCONTROL_NONE);

	sp_get_port_by_name(devicePath.c_str(), &Port);

	sp_return ret = sp_open(Port, SP_MODE_READ_WRITE);

	if (ret != SP_OK) {
		std::printf("SerIo::Init: Failed to open %s.", devicePath.c_str());
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

SerIo::StatusCode SerIo::Write(std::vector<uint8_t> &buffer)
{
#ifdef DEBUG_SERIAL
	std::cout << "SerIo::Write:";
	for(uint8_t i = 0; i < buffer.size(); i++) {
		std::printf(" %02X", buffer.at(i));
	}
	std::cout << std::endl;
#endif

	if (buffer.size() == 0) {
		return StatusCode::ZeroSizeError;
	}

	int ret = sp_nonblocking_write(Port, buffer.data(), buffer.size());

	if (ret <= 0) {
		return StatusCode::WriteError;
	} else if (ret != (int)buffer.size()) {
#ifdef DEBUG_SERIAL
		std::printf("SerIo::Write: Only wrote %02X of %02X to the port!\n", ret, (int)buffer.size());
#endif
		return StatusCode::WriteError;
	}

	return StatusCode::Okay;
}

SerIo::StatusCode SerIo::Read(std::vector<uint8_t> &buffer)
{
	int bytes = sp_input_waiting(Port);

	if (bytes == 0) {
		return StatusCode::ZeroSizeError;
	} else if (bytes < 0) {
		return StatusCode::ReadError;
	}

	buffer.resize(bytes);

	int ret = sp_nonblocking_read(Port, buffer.data(), buffer.size());

	if (ret <= 0) {
		return StatusCode::ReadError;
	}

#ifdef DEBUG_SERIAL
	std::cout << "SerIo::Read:";
	for (size_t i = 0; i < buffer.size(); i++) {
		std::printf(" %02X", buffer.at(i));
	}
	std::cout << std::endl;
#endif

	return StatusCode::Okay;
}
