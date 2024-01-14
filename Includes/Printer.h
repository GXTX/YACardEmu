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
#include <random>

#include <SDL2/SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/fmt/bin_to_hex.h"

#include "ghc/filesystem.hpp"

#include "utf8.h"

extern std::shared_ptr<spdlog::async_logger> g_logger;

class Printer
{
public:
	Printer()
	{
		m_customGlyphs.resize(256);
	}
	~Printer()
	{
		SDL_Quit();
	}

	void InsertCard(std::string& cardName)
	{
		m_localName = cardName;
		LoadCardImage(m_localName);
	}

	void RemoveCard(std::string& cardName)
	{
		m_localName = cardName;
		PrintLine();
		if (m_cardImage != nullptr) {
			SaveCardImage(m_localName);
			SDL_FreeSurface(m_cardImage);
			m_cardImage = nullptr;
		}
		m_localName = "";
	}

	void Erase()
	{
		if (m_cardImage != nullptr) {
			SDL_FreeSurface(m_cardImage);
			m_cardImage = nullptr;
		}
	}

	bool RegisterFont(std::vector<uint8_t>& data);
	bool QueuePrintLine(std::vector<uint8_t>& data);
	std::string m_localName = {};

	// Default state is a vertical card on the mechs
	bool m_horizontalCard = false;
protected:
	struct PrintCommand {
		uint8_t offset = 0;
		std::vector<uint8_t> data = {};
	};

	std::vector<PrintCommand> m_printQueue = {};
	std::vector<SDL_Surface*> m_customGlyphs = {};

	SDL_Surface* m_cardImage = nullptr;

	void PrintLine();

	void LoadCardImage(std::string& cardName)
	{
		std::string temp = cardName;
		temp.append(".png");

		if (ghc::filesystem::exists(temp)) {
			m_cardImage = IMG_Load(temp.c_str());
			if (m_cardImage != nullptr)
				return;
			g_logger->warn("Printer::LoadCardImage: Found card image but IMG_Load couldn't create the surface, generating a transparent surface");
		}

		std::string cwd = ghc::filesystem::current_path().string();
#ifdef _WIN32
		if (cwd.back() != '\\')
			cwd.append("\\");
		cwd.append("images\\");
#else
		if (cwd.back() != '/')
			cwd.append("/");
		cwd.append("images/");
#endif
		std::vector<std::string> images = {};

		if (ghc::filesystem::is_directory(cwd)) {
			for (const auto& entry : ghc::filesystem::directory_iterator(cwd)) {
				if (entry.path().string().find(".png") != std::string::npos)
					images.emplace_back(entry.path().string());
			}
		} else {
			// FIXME: Create the folder for the user
			g_logger->warn("Printer::LoadCardImage: Image folder does not exist");
		}

		if (!images.empty()) {
			std::random_device dev;
			std::mt19937 twist(dev());
			std::uniform_int_distribution<std::mt19937::result_type> rng(0, static_cast<uint8_t>(images.size()) - 1);

			m_cardImage = IMG_Load(images.at(rng(twist)).c_str());
			if (m_cardImage != nullptr)
				return;
			g_logger->warn("Printer::LoadCardImage: IMG_Load failed to load the random base card image");
		}

		// Everything else failed, we *need* a surface... So let's generate a transparent one
		m_cardImage = SDL_CreateRGBSurface(
			0,
			(m_horizontalCard ? 1019 : 640),
			(m_horizontalCard ? 640 : 1019),
			32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
		);
	}

	void SaveCardImage(std::string& cardName)
	{
		std::string temp = cardName;
		temp.append(".png");
		IMG_SavePNG(m_cardImage, temp.c_str());
	}

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
