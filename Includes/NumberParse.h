/*
	YACardEmu
	----------------
	Copyright (C) 2020-2025 wutno (https://github.com/GXTX)


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

#ifndef NUMBERPARSE_H
#define NUMBERPARSE_H

#include <cstddef>
#include <exception>
#include <string>

// Parses a whole number from the config, anything else (empty, text, trailing junk, out of range) fails
inline bool ParseNumber(const std::string& text, long min, long max, int& out)
{
	try {
		std::size_t used = 0;
		const long value = std::stol(text, &used, 10);
		if (used != text.size() || value < min || value > max)
			return false;
		out = static_cast<int>(value);
		return true;
	} catch (const std::exception&) {
		return false;
	}
}

#endif // NUMBERPARSE_H
