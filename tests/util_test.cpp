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

// Tests for the small header-only helpers (card names, JSON/URL escaping, config editing,
// number parsing, the CSRF origin check, the folder lookup and the printed rows). They need no SDL, serial port or
// web server, so they run anywhere the project can be compiled.

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <system_error>

#include "AppPaths.h"
#include "CardName.h"
#include "ConfigFile.h"
#include "NumberParse.h"
#include "PrintRows.h"
#include "SameOrigin.h"
#include "StringEscape.h"
#include "test.h"

namespace {
namespace fs = ghc::filesystem;

std::string Slurp(const fs::path& p)
{
	std::ifstream in(p.string(), std::ios::binary);
	std::stringstream s;
	s << in.rdbuf();
	return s.str();
}

void Put(const fs::path& p, const std::string& text)
{
	std::ofstream out(p.string(), std::ios::binary);
	out << text;
}

// A directory that is removed again when the test is done
struct TempDir {
	fs::path path;
	explicit TempDir(const std::string& name)
	{
		std::error_code ec;
		path = fs::temp_directory_path(ec) / ("yacardemu_" + name);
		fs::remove_all(path, ec);
		fs::create_directories(path, ec);
	}
	~TempDir()
	{
		std::error_code ec;
		fs::remove_all(path, ec);
	}
};

void TestCardName()
{
	for (const char* ok : { "card.bin", "a.bin", "my card.bin", "card.bin.1", "card.bin.123", "card.bin.0", "日本.bin", "Card.Name.bin", "x.bin.bin.7", "a.bin.123456789" })
		CHECK(CardName::IsValid(ok));

	for (const char* bad : { "", "card", "card.bin.", "card.bin.x", "card.bin.1a", "a.bin.1234567890", ".bin", "../card.bin", "..\\card.bin", "a/b.bin",
			"a\\b.bin", "/etc/passwd", "C:evil.bin", "a:b.bin", "con.bin", "CON.BIN.1", "nul.bin", "com1.bin", "LPT9.bin", "a..bin", "card.bin ", "card.png",
			"card.bin.png", "card.bin\"x", "a|b.bin", ".", "..", "a*.bin" })
		CHECK(!CardName::IsValid(bad));

	CHECK(!CardName::IsValid(std::string("a\0b.bin", 7)));
	CHECK(!CardName::IsValid("a\nb.bin"));
	CHECK(!CardName::IsValid(std::string(252, 'a') + ".bin")); // 256 characters
	CHECK(CardName::IsValid(std::string(251, 'a') + ".bin"));  // 255 characters
}

void TestEscape()
{
	CHECK_EQ(JsonEscape("card.bin"), std::string("card.bin"));
	CHECK_EQ(JsonEscape("a\"b"), std::string("a\\\"b"));
	CHECK_EQ(JsonEscape("a\\b"), std::string("a\\\\b"));
	CHECK_EQ(JsonEscape("a\nb\tc\r"), std::string("a\\nb\\tc\\r"));
	CHECK_EQ(JsonEscape(std::string("a\x01" "b", 3)), std::string("a\\u0001b"));
	CHECK_EQ(JsonEscape("日本.bin"), std::string("日本.bin"));
	CHECK_EQ(JsonEscape("\"},{\"name\":\"x"), std::string("\\\"},{\\\"name\\\":\\\"x"));

	CHECK_EQ(UrlEncode("card.bin"), std::string("card.bin"));
	CHECK_EQ(UrlEncode("my card (1).bin"), std::string("my%20card%20%281%29.bin"));
	CHECK_EQ(UrlEncode("a&b=c#d?.bin"), std::string("a%26b%3Dc%23d%3F.bin"));
	CHECK_EQ(UrlEncode("日.bin"), std::string("%E6%97%A5.bin"));
}

void TestConfigFile()
{
	TempDir dir("config");
	const std::string p = (dir.path / "config.ini").string();
	std::string err;

	// replaces in place, keeps comments, blank lines, other keys and other sections
	Put(p, "[config]\n; keep me\n;autoselectedcard = commented.bin\napiport = 8080\nautoselectedcard = card.bin\n\n[other]\nautoselectedcard = x\n");
	CHECK(ConfigFile::SetValue(p, "config", "autoselectedcard", "new.bin.3", &err));
	CHECK_EQ(Slurp(p), std::string("[config]\n; keep me\n;autoselectedcard = commented.bin\napiport = 8080\nautoselectedcard = new.bin.3\n\n[other]\nautoselectedcard = x\n"));

	// CRLF, no spaces around the '=', other case
	Put(p, "[Config]\r\nAutoSelectedCard=a.bin\r\nport=1\r\n");
	CHECK(ConfigFile::SetValue(p, "config", "autoselectedcard", "b.bin"));
	CHECK_EQ(Slurp(p), std::string("[Config]\r\nAutoSelectedCard=b.bin\r\nport=1\r\n"));

	// missing key is added at the end of its section, before the next one
	Put(p, "[config]\napiport = 8080\n\n; other stuff\n[other]\nk = v\n");
	CHECK(ConfigFile::SetValue(p, "config", "autoselectedcard", "c.bin"));
	CHECK_EQ(Slurp(p), std::string("[config]\napiport = 8080\nautoselectedcard = c.bin\n\n; other stuff\n[other]\nk = v\n"));

	// missing section, then missing file
	Put(p, "[other]\nk = v\n");
	CHECK(ConfigFile::SetValue(p, "config", "autoselectedcard", "d.bin"));
	CHECK_EQ(Slurp(p), std::string("[other]\nk = v\n\n[config]\nautoselectedcard = d.bin\n"));
	std::error_code ec;
	fs::remove(p, ec);
	CHECK(ConfigFile::SetValue(p, "config", "autoselectedcard", "e.bin"));
	CHECK_EQ(Slurp(p), std::string("[config]\nautoselectedcard = e.bin\n"));

	// unchanged value does not touch the file
	Put(p, "[config]\nautoselectedcard = same.bin\n");
	const auto before = fs::last_write_time(p, ec);
	CHECK(ConfigFile::SetValue(p, "config", "autoselectedcard", "same.bin"));
	CHECK(fs::last_write_time(p, ec) == before);

	// no trailing newline stays that way, duplicates are all updated, no temp file is left behind
	Put(p, "[config]\nautoselectedcard = a.bin");
	CHECK(ConfigFile::SetValue(p, "config", "autoselectedcard", "b.bin"));
	CHECK_EQ(Slurp(p), std::string("[config]\nautoselectedcard = b.bin"));
	Put(p, "[config]\nautoselectedcard = 1\nautoselectedcard = 2\n");
	CHECK(ConfigFile::SetValue(p, "config", "autoselectedcard", "z"));
	CHECK_EQ(Slurp(p), std::string("[config]\nautoselectedcard = z\nautoselectedcard = z\n"));
	CHECK(!fs::exists(p + ".tmp", ec));

	// bad input and an unwritable location are reported, not hidden
	CHECK(!ConfigFile::SetValue(p, "config", "k", "a\nb", &err));
	CHECK(!ConfigFile::SetValue((dir.path / "no_such_folder" / "config.ini").string(), "config", "k", "v", &err));
	CHECK(!err.empty());
}

void TestParseNumber()
{
	int v = -1;
	CHECK(ParseNumber("8080", 1, 65535, v) && v == 8080);
	CHECK(ParseNumber("1", 1, 65535, v) && v == 1);
	CHECK(ParseNumber("65535", 1, 65535, v) && v == 65535);
	CHECK(ParseNumber("115200", 1, 4000000, v) && v == 115200);
	for (const char* bad : { "0", "65536", "-1", "", "abc", "80x", "80 ", "0x10", "99999999999999999999" })
		CHECK(!ParseNumber(bad, 1, 65535, v));
}

struct FakeRequest {
	std::map<std::string, std::string> headers;
	bool has_header(const char* k) const { return headers.count(k) != 0; }
	std::string get_header_value(const char* k) const
	{
		const auto it = headers.find(k);
		return it == headers.end() ? std::string() : it->second;
	}
};

void TestSameOrigin()
{
	CHECK(IsSameOrigin(FakeRequest{ { { "Host", "localhost:8080" } } })); // curl and friends send no Origin
	CHECK(IsSameOrigin(FakeRequest{ { { "Host", "localhost:8080" }, { "Origin", "http://localhost:8080" } } }));
	CHECK(IsSameOrigin(FakeRequest{ { { "Host", "192.168.1.5:8080" }, { "Origin", "http://192.168.1.5:8080" } } }));
	CHECK(IsSameOrigin(FakeRequest{ { { "Host", "LocalHost:8080" }, { "Origin", "http://localhost:8080" } } }));
	CHECK(!IsSameOrigin(FakeRequest{ { { "Host", "localhost:8080" }, { "Origin", "http://evil.example" } } }));
	CHECK(!IsSameOrigin(FakeRequest{ { { "Host", "localhost:8080" }, { "Origin", "http://localhost:9999" } } }));
	CHECK(!IsSameOrigin(FakeRequest{ { { "Host", "localhost:8080" }, { "Origin", "null" } } }));
	CHECK(!IsSameOrigin(FakeRequest{ { { "Host", "localhost:8080" }, { "Origin", "http://localhost:8080.evil.com" } } }));
}

void TestAppPaths()
{
	std::error_code ec;
	const fs::path originalCwd = fs::current_path(ec);

	TempDir cwd("paths_cwd");
	const std::string name = "yacardemu_test_marker";

	// nothing anywhere
	fs::current_path(cwd.path, ec);
	CHECK(AppPaths::Find(name).empty());

	// in the current directory
	fs::create_directories(cwd.path / name, ec);
	CHECK(!AppPaths::Find(name).empty());

	// only next to the executable
	fs::remove_all(cwd.path / name, ec);
	const fs::path exeDir = AppPaths::ExecutableDir();
	CHECK(!exeDir.empty());
	if (!exeDir.empty()) {
		fs::create_directories(exeDir / name, ec);
		const fs::path found = AppPaths::Find(name);
		CHECK(!found.empty());
		CHECK(found.parent_path() == exeDir);

		// the current directory wins when it has one too
		fs::create_directories(cwd.path / name, ec);
		CHECK(AppPaths::Find(name).parent_path() == fs::current_path(ec));
		fs::remove_all(exeDir / name, ec);
	}

	fs::current_path(originalCwd, ec);
}

void TestPrintRows()
{
	using PrintRows::Used;
	auto rowsOf = [](uint8_t offset, const std::string& text) {
		const uint8_t* begin = reinterpret_cast<const uint8_t*>(text.data());
		return Used(offset, begin, begin + text.size());
	};

	// text on the row it starts on, every CR moves down a row
	auto rows = rowsOf(1, "AB\rCD");
	CHECK_EQ(rows.count(), static_cast<size_t>(2));
	CHECK(rows.test(1));
	CHECK(rows.test(2));

	// leading CRs skip rows, the offset is where counting starts
	rows = rowsOf(3, "\r\rX");
	CHECK_EQ(rows.count(), static_cast<size_t>(1));
	CHECK(rows.test(5));

	// spaces, control bytes and empty lines draw nothing
	rows = rowsOf(1, "   \r\r\x11\x14 \r");
	CHECK(rows.none());

	// a row is only counted once, however much text is on it
	CHECK_EQ(rowsOf(2, "abc def").count(), static_cast<size_t>(1));

	// rows past the end are ignored instead of overflowing the set
	std::string deep(PrintRows::kMaxRows + 5, '\r');
	deep += "X";
	CHECK(rowsOf(1, deep).none());

	// the top half and the bottom half of a card don't share rows, the same half printed twice does
	const auto top = rowsOf(1, "AB\rCD");
	const auto bottom = rowsOf(1, "\r\r\r\rEF\rGH");
	CHECK((top & bottom).none());
	CHECK((top & rowsOf(1, "XY\rZ")).any());
}
} // namespace

int main()
{
	TestCardName();
	TestEscape();
	TestConfigFile();
	TestParseNumber();
	TestSameOrigin();
	TestAppPaths();
	TestPrintRows();
	return test::Finish("util_test");
}
