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

#ifndef PRINTROWS_H
#define PRINTROWS_H

// Works out which text rows of a card a print line draws on. The printer uses it to tell a game
// that prints the next part of the card (other rows) from one that prints the same rows again.
// No SDL needed, so it can be tested on its own.

#include <bitset>
#include <cstddef>
#include <cstdint>

namespace PrintRows {

constexpr std::size_t kMaxRows = 64;
using RowSet = std::bitset<kMaxRows>;

// offset is the row the line starts on, [begin, end) the text of the line. Every CR moves down one
// row. Only characters that draw something count: spaces and control bytes (the scale and escape
// codes) leave the row untouched. This is a rough count, it does not know about the height of
// scaled text.
inline RowSet Used(uint8_t offset, const uint8_t* begin, const uint8_t* end)
{
	RowSet rows;
	std::size_t row = offset;

	for (const uint8_t* p = begin; p != end; ++p) {
		if (*p == '\r') {
			row++;
		} else if (*p > 0x20 && row < kMaxRows) {
			rows.set(row);
		}
	}

	return rows;
}

} // namespace PrintRows

#endif // PRINTROWS_H
