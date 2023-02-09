/*
    YACardEmu
    ----------------
    Copyright (C) 2020-2023 wutno (https://github.com/GXTX)


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

SerIo::SerIo(SerIo::Settings *settings)
{
	m_portSettings = settings;
	m_buffer.reserve(512); // max length * 2
}

SerIo::~SerIo()
{
	if (m_isPipe) {
#ifdef _WIN32	
		CloseHandle(m_pipeHandle);
#endif
	} else {
		sp_close(m_portHandle);
	}	
}

bool SerIo::Open()
{
#ifdef _WIN32
	if (m_portSettings->devicePath.find("pipe") != std::string::npos) {
		m_pipeHandle = CreateNamedPipeA(m_portSettings->devicePath.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 0, 0, NMPWAIT_USE_DEFAULT_WAIT, NULL);

		if (m_pipeHandle == INVALID_HANDLE_VALUE) {
			spdlog::critical("SerIo::Init: Failed to create pipe");
			return false;
		}

		m_isPipe = true;
		return true;
	}
#endif

	sp_new_config(&m_portConfig);
	sp_set_config_baudrate(m_portConfig, m_portSettings->baudRate);
	sp_set_config_bits(m_portConfig, 8);
	sp_set_config_parity(m_portConfig, m_portSettings->parity);
	sp_set_config_stopbits(m_portConfig, 1);
	sp_set_config_flowcontrol(m_portConfig, SP_FLOWCONTROL_NONE);

	sp_get_port_by_name(m_portSettings->devicePath.c_str(), &m_portHandle);

	if (sp_open(m_portHandle, SP_MODE_READ_WRITE) != SP_OK) {
		spdlog::critical("SerIo::Init: Failed to open {}", m_portSettings->devicePath);
		return false;
	}

	sp_set_config(m_portHandle, m_portConfig);

	return true;
}

void SerIo::SendAck()
{
	constexpr static const uint8_t ack = 0x06;

	spdlog::debug("SerIo::SendAck: 06");

#ifdef _WIN32
	if (m_isPipe) {
		DWORD dwRet = 0;
		WriteFile(m_pipeHandle, &ack, 1, &dwRet, NULL);
		return;
	}
#endif

	sp_blocking_write(m_portHandle, &ack, 1, 0);
	sp_drain(m_portHandle);

	return;
}

SerIo::Status SerIo::Write(std::vector<uint8_t> &buffer)
{
	if (buffer.empty()) {
		return Status::ZeroSizeError;
	}

	spdlog::debug("SerIo::Write: ");
	spdlog::debug("{:Xn}", spdlog::to_hex(buffer));

	int ret = 0;

	if (m_isPipe) {
#ifdef _WIN32
		DWORD dwRet = 0;
		WriteFile(m_pipeHandle, &buffer[0], buffer.size(), &dwRet, NULL);
		ret = dwRet;
#endif
	} else {
		ret = sp_blocking_write(m_portHandle, &buffer[0], buffer.size(), 0);
		sp_drain(m_portHandle);
	}

	// TODO: Should we care about write errors?
	if (ret <= 0) {
		return Status::WriteError;
	} else if (ret != static_cast<int>(buffer.size())) {
		spdlog::error("Only wrote {0:X} of {1:X} to the port!", ret, buffer.size());
		return Status::WriteError;
	}

	return Status::Okay;
}

SerIo::Status SerIo::Read(std::vector<uint8_t> &buffer)
{
	int bytes = 0;

	if (m_isPipe) {
#ifdef _WIN32
		DWORD dwBytes = 0;
		if (PeekNamedPipe(m_pipeHandle, 0, 0, 0, &dwBytes, 0) == 0) {
			DWORD error = ::GetLastError();
			if (error == ERROR_BROKEN_PIPE) {
				spdlog::warn("Pipe broken! Resetting...");
				DisconnectNamedPipe(m_pipeHandle);
				ConnectNamedPipe(m_pipeHandle, NULL);
			}
			return Status::ReadError;
		}
		bytes = dwBytes;
#endif
	} else {
		bytes = sp_input_waiting(m_portHandle);
	}

	if (bytes < 1) {
		return Status::ReadError;
	}

	m_buffer.clear();
	m_buffer.resize(static_cast<size_t>(bytes));

	int ret = 0;

	if (m_isPipe) {
#ifdef _WIN32
		DWORD dwRet = 0;
		BOOL bRet = ReadFile(m_pipeHandle, &m_buffer[0], m_buffer.size(), &dwRet, NULL);
		ret = bRet;
#endif
	} else {
		ret = sp_nonblocking_read(m_portHandle, &m_buffer[0], m_buffer.size());
	}

	if (ret <= 0) {
		return Status::ReadError;
	}

	std::copy(m_buffer.begin(), m_buffer.end(), std::back_inserter(buffer));

	spdlog::debug("SerIo::Read: ");
	spdlog::debug("{:Xn}", spdlog::to_hex(buffer));

	return Status::Okay;
}
