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

#include "SerIo.h"

#define DEBUG_SERIAL

SerIo::SerIo(const char *devicePath)
{
#ifdef NDEBUG
	spdlog::set_level(spdlog::level::warn);
#else
	spdlog::set_level(spdlog::level::debug);
#endif
	spdlog::set_pattern("[%^%l%$] %v");

	sp_new_config(&PortConfig);
	sp_set_config_baudrate(PortConfig, 9600);
	sp_set_config_bits(PortConfig, 8);
	sp_set_config_parity(PortConfig, SP_PARITY_NONE);
	sp_set_config_stopbits(PortConfig, 1);
	sp_set_config_flowcontrol(PortConfig, SP_FLOWCONTROL_NONE);

	sp_get_port_by_name(devicePath, &Port);

	sp_return ret = sp_open(Port, SP_MODE_READ_WRITE);

	if (ret != SP_OK) {
		spdlog::critical("SerIo::Init: Failed to open {}", devicePath);
		IsInitialized = false;
	} else {
		sp_set_config(Port, PortConfig);
		IsInitialized = true;
	}
}

SerIo::~SerIo()
{
	sp_close(Port);
}

void SerIo::SendAck()
{
	constexpr static const uint8_t ack = 0x06;

	spdlog::debug("SerIo::SendAck: 06");

	sp_blocking_write(Port, &ack, 1, 0);
	sp_drain(Port);

	return;
}

SerIo::Status SerIo::Write(std::vector<uint8_t> &buffer)
{
	if (buffer.empty()) {
		return Status::ZeroSizeError;
	}

	spdlog::debug("SerIo::Write: ");
	spdlog::debug("{:Xn}", spdlog::to_hex(buffer));

	int ret = sp_blocking_write(Port, &buffer[0], buffer.size(), 0);
	sp_drain(Port);

	// TODO: Should we care about write errors?
	if (ret <= 0) {
		return Status::WriteError;
	} else if (ret != static_cast<int>(buffer.size())) {
		spdlog::error("Only wrote {} of {} to the port!", ret, buffer.size());
		return Status::WriteError;
	}

	return Status::Okay;
}

SerIo::Status SerIo::Read(std::vector<uint8_t> &buffer)
{
	int bytes = sp_input_waiting(Port);

	if (bytes < 1) {
		return Status::ReadError;
	}

	serialBuffer.clear();
	serialBuffer.resize(static_cast<size_t>(bytes));

	int ret = sp_nonblocking_read(Port, &serialBuffer[0], serialBuffer.size());

	if (ret <= 0) {
		return Status::ReadError;
	}

	std::copy(serialBuffer.begin(), serialBuffer.end(), std::back_inserter(buffer));

	spdlog::debug("SerIo::Read: ");
	spdlog::debug("{:Xn}", spdlog::to_hex(buffer));

	return Status::Okay;
}
