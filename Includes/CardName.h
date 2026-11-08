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

#ifndef CARDNAME_H
#define CARDNAME_H

#include <cstddef>
#include <string>

namespace CardName {

// Card names end up in "<cardPath><name>" for both reading and writing, so they
// must be plain file names. We accept "<name>.bin" and "<name>.bin.<number>"
// (the latter is what CardIo::WriteCard generates to avoid overwriting cards).
inline bool IsValid(const std::string& name)
{
	constexpr std::size_t maxLength = 255;

	if (name.empty() || name.size() > maxLength)
		return false;

	for (const char ch : name) {
		const auto c = static_cast<unsigned char>(ch);

		// Control characters, including NUL
		if (c < 0x20 || c == 0x7F)
			return false;

		// Path separators, drive/stream markers and characters Windows rejects
		switch (c) {
			case '/': case '\\': case ':': case '*': case '?':
			case '"': case '<': case '>': case '|':
				return false;
			default:
				break;
		}
	}

	if (name.find("..") != std::string::npos)
		return false;

	// Windows silently strips trailing dots and spaces
	if (name.front() == ' ' || name.back() == ' ' || name.back() == '.')
		return false;

	// Windows treats these as devices no matter the extension
	{
		std::string stem = name.substr(0, name.find('.'));
		while (!stem.empty() && stem.back() == ' ')
			stem.pop_back();
		for (auto& c : stem) {
			if (c >= 'a' && c <= 'z')
				c = static_cast<char>(c - 'a' + 'A');
		}
		if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL")
			return false;
		if (stem.size() == 4 && (stem.compare(0, 3, "COM") == 0 || stem.compare(0, 3, "LPT") == 0) && stem[3] >= '1' && stem[3] <= '9')
			return false;
	}

	// Must be "<stem>.bin" or "<stem>.bin.<digits>"
	const auto ext = name.rfind(".bin");
	if (ext == std::string::npos || ext == 0)
		return false;

	const auto rest = name.substr(ext + 4);
	if (rest.empty())
		return true;

	// Keep the number small enough for std::stoi in CardIo::WriteCard
	constexpr std::size_t maxDigits = 9;
	if (rest.size() < 2 || rest[0] != '.' || rest.size() - 1 > maxDigits)
		return false;

	for (std::size_t i = 1; i < rest.size(); i++) {
		if (rest[i] < '0' || rest[i] > '9')
			return false;
	}
	return true;
}

} // namespace CardName

#endif // CARDNAME_H
