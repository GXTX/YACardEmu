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

#ifndef STRINGESCAPE_H
#define STRINGESCAPE_H

#include <cstdio>
#include <string>

// Helpers for building the small JSON/URL strings the web API hands out

// Escapes text so it can be placed between the quotes of a JSON string.
// Bytes >= 0x80 are passed through untouched, they are part of UTF-8 sequences.
inline std::string JsonEscape(const std::string& in)
{
	std::string out;
	out.reserve(in.size());

	for (const char ch : in) {
		const auto c = static_cast<unsigned char>(ch);
		switch (c) {
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\b': out += "\\b";  break;
			case '\f': out += "\\f";  break;
			case '\n': out += "\\n";  break;
			case '\r': out += "\\r";  break;
			case '\t': out += "\\t";  break;
			default:
				if (c < 0x20 || c == 0x7F) {
					char buf[8];
					std::snprintf(buf, sizeof(buf), "\\u%04x", c);
					out += buf;
				} else {
					out += ch;
				}
				break;
		}
	}
	return out;
}

// Percent-encodes everything except the RFC 3986 unreserved characters
inline std::string UrlEncode(const std::string& in)
{
	static const char hex[] = "0123456789ABCDEF";
	std::string out;
	out.reserve(in.size());

	for (const char ch : in) {
		const auto c = static_cast<unsigned char>(ch);
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
			c == '-' || c == '_' || c == '.' || c == '~') {
			out += ch;
		} else {
			out += '%';
			out += hex[c >> 4];
			out += hex[c & 0x0F];
		}
	}
	return out;
}

#endif // STRINGESCAPE_H
