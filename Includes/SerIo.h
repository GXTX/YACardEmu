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

#ifndef SERIO_H
#define SERIO_H

#ifdef _WIN32
#include <windows.h>
#endif

#include <iostream>
#include <vector>

#include <libserialport.h>

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/fmt/bin_to_hex.h"

extern std::shared_ptr<spdlog::async_logger> logger;

class SerIo
{
public:
	enum class Status {
		Okay,
		Timeout,
		ReadError,
		WriteError,
		ZeroSizeError,
	};

	struct Settings {
		std::string devicePath{};
		int baudRate = 9600;
		sp_parity parity = SP_PARITY_NONE;
	};

	SerIo::Settings *m_portSettings = nullptr;
	std::vector<uint8_t> m_readBuffer{};
	std::vector<uint8_t> m_writeBuffer{};

	SerIo(SerIo::Settings *settings);
	~SerIo();

	bool Open();
	SerIo::Status Read();
	SerIo::Status Write();
	void SendAck();
private:
#ifdef _WIN32
	HANDLE m_pipeHandle = INVALID_HANDLE_VALUE;
#endif

	bool m_isPipe = false;
	sp_port *m_portHandle = nullptr;
	sp_port_config *m_portConfig = nullptr;
};

#endif
