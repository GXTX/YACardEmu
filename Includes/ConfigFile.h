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

#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include "ghc/filesystem.hpp"

namespace ConfigFile {

namespace detail {

inline std::string Trim(const std::string& in)
{
	const auto first = in.find_first_not_of(" \t");
	if (first == std::string::npos)
		return {};
	const auto last = in.find_last_not_of(" \t");
	return in.substr(first, last - first + 1);
}

inline std::string Lower(std::string in)
{
	for (auto& c : in)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return in;
}

} // namespace detail

// Sets "key = value" in [section] of an ini file by editing only that line, so comments,
// blank lines and the other settings stay as the user wrote them (a read/modify/write with
// mINI drops every comment). The file is replaced through a temporary file, a crash or a full
// disk can't leave a half written config.ini behind. Nothing is written if the value is
// already what we would set. Returns false and fills "error" if the file couldn't be updated.
inline bool SetValue(const std::string& path, const std::string& section, const std::string& key, const std::string& value, std::string* error = nullptr)
{
	using namespace detail;

	auto fail = [&](const std::string& message) {
		if (error != nullptr)
			*error = message;
		return false;
	};

	if (value.find_first_of("\r\n") != std::string::npos)
		return fail("value contains a line break");

	// Read the existing file, a missing file is just an empty one
	std::string text;
	{
		std::ifstream in(path, std::ios::in | std::ios::binary);
		if (in)
			text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	}

	const std::string eol = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
	const bool endsWithNewline = !text.empty() && text.back() == '\n';

	std::vector<std::string> lines;
	{
		std::size_t start = 0;
		while (start < text.size()) {
			auto end = text.find('\n', start);
			if (end == std::string::npos)
				end = text.size();
			std::string line = text.substr(start, end - start);
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			lines.push_back(std::move(line));
			start = end + 1;
		}
	}

	const std::string wantSection = Lower(section);
	const std::string wantKey = Lower(key);

	bool inSection = false;
	bool sectionFound = false;
	bool keyFound = false;
	bool changed = false;
	std::size_t insertAfter = 0; // index of the line to insert after (header or last key of the section)

	for (std::size_t i = 0; i < lines.size(); i++) {
		const std::string trimmed = Trim(lines[i]);
		if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#')
			continue;

		if (trimmed[0] == '[') {
			const auto close = trimmed.find(']');
			inSection = close != std::string::npos && Lower(Trim(trimmed.substr(1, close - 1))) == wantSection;
			if (inSection) {
				sectionFound = true;
				insertAfter = i;
			}
			continue;
		}

		if (!inSection)
			continue;

		insertAfter = i;

		const auto eq = lines[i].find('=');
		if (eq == std::string::npos || Lower(Trim(lines[i].substr(0, eq))) != wantKey)
			continue;

		keyFound = true;
		if (Trim(lines[i].substr(eq + 1)) == value)
			continue;

		// Keep the indentation and the spacing around the "="
		const std::string after = lines[i].substr(eq + 1);
		const auto valueStart = after.find_first_not_of(" \t");
		const std::string spacing = valueStart == std::string::npos ? after : after.substr(0, valueStart);
		lines[i] = lines[i].substr(0, eq + 1) + spacing + value;
		changed = true;
	}

	if (!keyFound) {
		const std::string newLine = key + " = " + value;
		if (sectionFound) {
			lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(insertAfter) + 1, newLine);
		} else {
			if (!lines.empty() && !Trim(lines.back()).empty())
				lines.push_back("");
			lines.push_back("[" + section + "]");
			lines.push_back(newLine);
		}
		changed = true;
	}

	if (!changed)
		return true;

	std::string out;
	for (std::size_t i = 0; i < lines.size(); i++) {
		out += lines[i];
		if (i + 1 < lines.size() || endsWithNewline || !keyFound)
			out += eol;
	}

	const std::string tmpPath = path + ".tmp";
	{
		std::ofstream tmp(tmpPath, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!tmp)
			return fail("unable to create " + tmpPath);
		tmp.write(out.data(), static_cast<std::streamsize>(out.size()));
		tmp.flush();
		if (!tmp) {
			tmp.close();
			std::error_code ignored;
			ghc::filesystem::remove(tmpPath, ignored);
			return fail("unable to write " + tmpPath);
		}
	}

	std::error_code ec;
	ghc::filesystem::rename(tmpPath, path, ec);
	if (ec) {
		std::error_code ignored;
		ghc::filesystem::remove(tmpPath, ignored);
		return fail(ec.message());
	}

	return true;
}

} // namespace ConfigFile

#endif // CONFIGFILE_H
