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

#ifndef APPPATHS_H
#define APPPATHS_H

#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

#include "ghc/filesystem.hpp"

// Files that ship with the program (public/, images/, the font, config.ini) used to be
// opened relative to the current directory, so they were only found when the program was
// started from the folder it lives in (not from a shortcut, a service or another terminal).
namespace AppPaths {

// Folder that contains the executable, empty if it can't be determined
inline ghc::filesystem::path ExecutableDir()
{
	try {
#ifdef _WIN32
		wchar_t buf[MAX_PATH] = {};
		const DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
		if (len > 0 && len < MAX_PATH)
			return ghc::filesystem::path(buf).parent_path();
#else
		return ghc::filesystem::read_symlink("/proc/self/exe").parent_path();
#endif
	} catch (...) {
	}
	return {};
}

// Looks for "relative" in the current directory first (so existing setups keep working) and
// then next to the executable. Returns an empty path if it isn't in either place.
inline ghc::filesystem::path Find(const ghc::filesystem::path& relative)
{
	std::error_code ec;

	const auto cwd = ghc::filesystem::current_path(ec);
	if (!ec && ghc::filesystem::exists(cwd / relative, ec))
		return cwd / relative;

	const auto exeDir = ExecutableDir();
	if (!exeDir.empty() && ghc::filesystem::exists(exeDir / relative, ec))
		return exeDir / relative;

	return {};
}

} // namespace AppPaths

#endif // APPPATHS_H
