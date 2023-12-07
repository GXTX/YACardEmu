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

#ifndef PRINTER_H
#define PRINTER_H

#include <vector>

#include <SDL2/SDL.h>

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/fmt/bin_to_hex.h"

#include "utf8.h"

extern std::shared_ptr<spdlog::async_logger> g_logger;

class Printer
{
public:
	bool RegisterFont(std::vector<uint8_t>& data);
	bool QueuePrintLine(std::vector<uint8_t>& data);
protected:
	std::vector<std::vector<uint8_t>> m_printQueue = {};
	std::vector<SDL_Surface *> m_customGlyphs = { 256, nullptr };
	
	void PrintLine();

	std::vector<bool> ConvertToBits(std::vector<uint8_t>& vector)
	{
		std::vector<bool> temp{};
		for (const auto x : vector) {
			uint8_t mask = 1 << 7;
			for (int i = 0; i < 8; ++i) {
				temp.push_back((x & mask) == 0 ? false : true);
				mask >>= 1;
			}
		}
		return temp;
	}
};

#endif
