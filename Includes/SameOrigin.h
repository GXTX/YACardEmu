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

#ifndef SAMEORIGIN_H
#define SAMEORIGIN_H

#include <algorithm>
#include <cctype>
#include <string>

// Browsers attach an Origin header to cross-site POST/DELETE requests. Clients
// that don't send one (curl, scripts) are not a CSRF risk and are let through.
// Works with anything that has has_header() and get_header_value(), i.e. httplib::Request.
template <typename Request>
inline bool IsSameOrigin(const Request& req)
{
	if (!req.has_header("Origin"))
		return true;

	std::string origin = req.get_header_value("Origin");
	const auto scheme = origin.find("://");
	if (scheme == std::string::npos) // e.g. "null"
		return false;
	origin.erase(0, scheme + 3);

	const std::string host = req.get_header_value("Host");
	return origin.size() == host.size() &&
		std::equal(origin.begin(), origin.end(), host.begin(), [](char a, char b) {
			return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
		});
}

#endif // SAMEORIGIN_H
