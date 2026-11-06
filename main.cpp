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

#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include <string>

#include "C1231LR.h"
#include "C1231BR.h"
#include "SerIo.h"
#include "WebIo.h"
#include "CardName.h"
#include "AppPaths.h"
#include "ConfigFile.h"
#include "NumberParse.h"

#include "httplib.h"
#include "mini/ini.h"
#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "ghc/filesystem.hpp"

// Globals
struct Settings {
	CardIo::Settings card = {};
	SerIo::Settings serial = {};
	std::string webListenHost;
	int webPort = 8080;
};
Settings globalSettings{};
std::atomic<bool> running{true};
constexpr static auto delay = std::chrono::microseconds(250);
std::shared_ptr<spdlog::async_logger> g_logger;

const char *helptext = 
	"YACardEmu - A simulator for magnetic card readers\n"
	"Commandline arguments:\n"
	"-d : debug log level\n"
	"-t : trace log level\n"
	"-f : log to yacardemu.log\n"
	"-c <file> : use this config file instead of config.ini\n"
	"-h : show this help text\n"
	"\n";

//

void WaitForInput()
{
#ifdef _WIN32
	g_logger->flush();
	std::cout << "Press any key to close the application...";
	std::cin.get();
#endif
}

// Only async-signal-safe things may happen in here, no logging or flushing. A lock free
// atomic store is fine, the main loop notices it and flushes the log on its way out.
static_assert(ATOMIC_BOOL_LOCK_FREE == 2, "std::atomic<bool> must be lock free to be used from a signal handler");

void SigHandler(int sig)
{
	if (sig == SIGINT || sig == SIGTERM)
		running = false;
}

// Used when there is no config.ini, so there is always something to edit
const char *defaultConfig = R"([config]
; Your path MUST end in a slash, and we do not create folders when none exists
; Linux
;basepath = /home/user/cards/
; Windows
;basepath = C:\Temp\cards\

; Linux
;serialpath = /dev/ttyUSB0
; Windows
;serialpath = COM3
;serialpath = \\.\pipe\YACardEmu

; Which device should we emulate?
; C1231LR / C1231BR / S31R
;targetdevice = C1231LR

;
; Optional
;
;serialbaud = 9600
; Acceptable none/even/odd
;serialparity = none
; Address the web UI / API listens on. The default is every interface (0.0.0.0) so
; the page can be used from another device. There is NO authentication, if you only
; use it on this machine set this to 127.0.0.1
;apihost = 127.0.0.1
apiport = 8080
autoselectedcard = card.bin
)";

// Creates a config.ini next to the executable (or in the current directory if we can't tell
// where that is), copied from config.ini.sample when it's there
bool CreateDefaultConfig(std::string &createdPath)
{
	const auto exeDir = AppPaths::ExecutableDir();
	const auto target = (exeDir.empty() ? ghc::filesystem::current_path() : exeDir) / "config.ini";
	createdPath = target.string();

	std::error_code ec;
	const auto sample = AppPaths::Find("config.ini.sample");
	if (!sample.empty()) {
		ghc::filesystem::copy_file(sample, target, ec);
		return !ec;
	}

	std::ofstream out(target.string(), std::ios::out | std::ios::binary);
	out << defaultConfig;
	return static_cast<bool>(out);
}

// TODO: Handle this elsewhere, feels dirty
bool ReadConfig(const std::string &requestedPath)
{
	// Where config.ini is: -c, then the current directory, then next to the executable
	std::string configPath = requestedPath;
	if (configPath.empty()) {
		const auto found = AppPaths::Find("config.ini");
		configPath = found.empty() ? std::string() : found.string();
	}

	if (configPath.empty() && requestedPath.empty()) {
		std::string created;
		if (CreateDefaultConfig(created))
			g_logger->critical("There was no config.ini, so one was created at \"{}\". Edit it (basepath, serialpath and targetdevice at least) and start again.", created);
		else
			g_logger->critical("Unable to find config.ini and unable to create \"{}\"!", created);
		return false;
	}

	// Read in config values
	globalSettings.card.configPath = configPath;
	mINI::INIFile config(configPath);
	mINI::INIStructure ini;
	std::string lhost, lport, lbaud, lparity;

	if (!config.read(ini)) {
		g_logger->critical("Unable to open \"{}\"!", configPath);
		return false;
	}
	g_logger->info("Using config \"{}\"", configPath);

	if (ini.has("config")) {
		lhost = ini["config"]["apihost"];
		lport = ini["config"]["apiport"];
		lbaud = ini["config"]["serialbaud"];
		lparity = ini["config"]["serialparity"];
		// Keep the default when targetdevice isn't set, an empty string is never a valid device
		if (!ini["config"]["targetdevice"].empty())
			globalSettings.card.mech = ini["config"]["targetdevice"];
		globalSettings.card.cardPath = ini["config"]["basepath"];
		globalSettings.card.SetCardName(ini["config"]["autoselectedcard"]);
		globalSettings.serial.devicePath = ini["config"]["serialpath"];
	}

	{
		const std::string selected = globalSettings.card.GetCardName();
		if (!selected.empty() && !CardName::IsValid(selected)) {
			g_logger->warn("Ignoring invalid autoselectedcard \"{}\" in config.ini", selected);
			globalSettings.card.SetCardName("");
		}
	}

	if (globalSettings.card.cardPath.empty()) {
		globalSettings.card.cardPath = ghc::filesystem::current_path().string();
	}

	{
		std::error_code ec;
		if (!ghc::filesystem::is_directory(globalSettings.card.cardPath, ec))
			g_logger->warn("basepath \"{}\" is not a folder, no cards will be listed or saved until it exists", globalSettings.card.cardPath);
	}

#ifdef _WIN32
	if (globalSettings.card.cardPath.back() != '\\')
		globalSettings.card.cardPath.append("\\");
#else
	if (globalSettings.card.cardPath.back() != '/')
		globalSettings.card.cardPath.append("/");
#endif

	if (lhost.empty()) {
		globalSettings.webListenHost = "0.0.0.0";
	} else {
		globalSettings.webListenHost = lhost;
	}

	globalSettings.webPort = 8080;
	if (!lport.empty() && !ParseNumber(lport, 1, 65535, globalSettings.webPort))
		g_logger->warn("Invalid apiport \"{}\" in config.ini, using 8080", lport);

	if (globalSettings.webListenHost != "127.0.0.1" && globalSettings.webListenHost != "localhost" && globalSettings.webListenHost != "::1") {
		g_logger->warn("The web UI/API listens on {0}:{1} and has no authentication, anyone who can reach it can control the emulator. Set \"apihost = 127.0.0.1\" in config.ini to only allow this machine.", globalSettings.webListenHost, globalSettings.webPort);
	}

	if (globalSettings.serial.devicePath.empty()) {
		globalSettings.serial.devicePath = "/dev/ttyUSB1";
	}

	globalSettings.serial.baudRate = 9600;
	if (!lbaud.empty() && !ParseNumber(lbaud, 1, 4000000, globalSettings.serial.baudRate))
		g_logger->warn("Invalid serialbaud \"{}\" in config.ini, using 9600", lbaud);

	if (lparity.empty()) {
		globalSettings.serial.parity = SP_PARITY_NONE;
	} else {
		if (lparity.find("even") != std::string::npos) {
			globalSettings.serial.parity = SP_PARITY_EVEN;
		} else if (lparity.find("odd") != std::string::npos) {
			globalSettings.serial.parity = SP_PARITY_ODD;
		} else {
			globalSettings.serial.parity = SP_PARITY_NONE;
		}
	}

	return true;
}

int main(int argc, char *argv[])
{
	// Handle quitting gracefully via signals
	std::signal(SIGINT, SigHandler);
	std::signal(SIGTERM, SigHandler);

	// Parse args

#ifdef NDEBUG
	spdlog::level::level_enum log_level = spdlog::level::info;
#else
	spdlog::level::level_enum log_level = spdlog::level::debug;
#endif

	bool file_log = false;
	std::string configArg;
	if (argc > 1) {
		for (int i = 1; i < argc; i++) {
			std::string arg = argv[i];
			while (arg[0] == '-') arg.erase(0,1);
			switch (arg[0]) {
				case 'd': 
					log_level = spdlog::level::debug;
					break;
				case 't': 
					log_level = spdlog::level::trace;
					break;
				case 'f':
					file_log = true;
					break;
				case 'c':
					if (i + 1 >= argc) {
						std::cout << "-c needs a file name\n\n" << helptext;
						return 1;
					}
					configArg = argv[++i];
					break;
				case 'h':
				default:
					std::cout << helptext;
					return 0;
			}
		}
	}

	// Set up logger
	spdlog::init_thread_pool(8192, 1);
	auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt >();
	std::vector<spdlog::sink_ptr> sinks;

	if (file_log) {
		auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("yacardemu.log", true);
		sinks.push_back(file_sink);
	}

	sinks.push_back(stdout_sink);
	g_logger = std::make_shared<spdlog::async_logger>("main", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
	g_logger->set_level(log_level);
	g_logger->flush_on(spdlog::level::info);
	g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

	if (!ReadConfig(configArg)) {
		WaitForInput();
		return 1;
	}

	std::unique_ptr<CardIo> cardHandler{};
	if (globalSettings.card.mech.compare("C1231LR") == 0 || globalSettings.card.mech.compare("S31R") == 0) {
		cardHandler = std::make_unique<C1231LR>(&globalSettings.card);
	} else if (globalSettings.card.mech.compare("C1231BR") == 0) {
		cardHandler = std::make_unique<C1231BR>(&globalSettings.card);
	} else {
		g_logger->critical("Invalid target device: {}", globalSettings.card.mech);
		WaitForInput();
		return 1;
	}

	std::unique_ptr<SerIo> serialHandler = std::make_unique<SerIo>(&globalSettings.serial);
	if (!serialHandler->Open()) {
		WaitForInput();
		return 1;
	}

	std::unique_ptr<WebIo> webHandler = std::make_unique<WebIo>(&globalSettings.card, globalSettings.webListenHost, globalSettings.webPort, &running);
	if (!webHandler->Spawn()) {
		WaitForInput();
		return 1;
	}

	g_logger->info("Running...");
	while (running) {
		// TODO: device read/write should probably be a separate thread
		if (serialHandler->Read() != SerIo::Status::Okay && serialHandler->m_readBuffer.empty()) {
			std::this_thread::sleep_for(delay);
			continue;
		}

		cardHandler->Process(serialHandler->m_readBuffer, serialHandler->m_writeBuffer);
		serialHandler->Write();

		std::this_thread::sleep_for(delay);
	}

	g_logger->info("Shutting down...");
	g_logger->flush();

	return 0;
}
