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

#ifndef PRINTER_H
#define PRINTER_H

#include <vector>
#include <random>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <string>

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
	// Rendering and saving the card image happens on a single worker thread, so print jobs
	// never overlap and nothing keeps running (detached) after the Printer is gone.
	//
	// Every job gets its own copy of what it needs (print queue, image reference, glyphs),
	// taken on the thread that drives CardIo. Everything else in this class is only ever
	// touched by that thread, so no locking is needed outside of the job queue.
	Printer()
	{
		m_customGlyphs.resize(256);
		m_worker = std::thread(&Printer::WorkerLoop, this);
	}
	~Printer()
	{
		{
			std::lock_guard<std::mutex> lock(m_jobMutex);
			m_stopWorker = true;
		}
		m_jobCv.notify_all();
		if (m_worker.joinable())
			m_worker.join(); // Outstanding jobs are finished before the worker exits

		// Whatever card is still in the printer gets saved, same as when it's removed
		MakeFinishJob(m_localName)();
		m_localName.clear();
		SDL_Quit();
	}

	Printer(const Printer&) = delete;
	Printer& operator=(const Printer&) = delete;

	void InsertCard(std::string& cardName)
	{
		m_localName = cardName;
		LoadCardImage(m_localName);
	}

	void RemoveCard(std::string& cardName)
	{
		// Takes the queue and image away from us now, so the next card can start right away
		Enqueue(MakeFinishJob(cardName));
		m_localName.clear();
	}

	void Erase()
	{
		m_cardImage.reset();
	}

	bool RegisterFont(std::vector<uint8_t>& data);
	bool QueuePrintLine(std::vector<uint8_t>& data);
	std::string m_localName = {};

	// Default state is a vertical card on the mechs
	bool m_isHorizontalCard = false;

protected:
	static constexpr uint8_t maxCustomGylphDimensions = 30;

	struct PrintCommand {
		uint8_t offset = 0;
		std::vector<uint8_t> data = {};
	};

	// Surfaces are reference counted so a job that is still drawing on (or with) one keeps it
	// alive even if the card is removed or the game registers a new glyph in the meantime
	using Surface = std::shared_ptr<SDL_Surface>;
	static Surface MakeSurface(SDL_Surface* surface)
	{
		return surface != nullptr ? Surface(surface, SDL_FreeSurface) : Surface();
	}

	std::vector<PrintCommand> m_printQueue = {};
	std::vector<Surface> m_customGlyphs = {};

	Surface m_cardImage = nullptr;

	// Worker thread and its job queue
	std::mutex m_jobMutex = {};
	std::condition_variable m_jobCv = {};
	std::deque<std::function<void()>> m_jobs = {};
	bool m_stopWorker = false;
	std::thread m_worker = {};

	void Enqueue(std::function<void()> job);
	void WorkerLoop();

	// Draws the queued lines onto image, runs on the worker thread (or after it has stopped)
	void RenderLines(const std::vector<PrintCommand>& queue, SDL_Surface* image, const std::vector<Surface>& glyphs, bool horizontal);

	// Takes the pending lines and the card image and returns a job that prints and saves them
	std::function<void()> MakeFinishJob(const std::string& cardName);

	void LoadCardImage(std::string& cardName)
	{
		std::string temp = cardName;
		temp.append(".png");

		if (ghc::filesystem::exists(temp)) {
			m_cardImage = MakeSurface(IMG_Load(temp.c_str()));
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

			m_cardImage = MakeSurface(IMG_Load(images.at(rng(twist)).c_str()));
			if (m_cardImage != nullptr)
				return;
			g_logger->warn("Printer::LoadCardImage: IMG_Load failed to load the random base card image");
		}

		// Everything else failed, we *need* a surface... So let's generate a transparent one
		m_cardImage = MakeSurface(QuickCreateSurface((m_isHorizontalCard ? 1019 : 640), (m_isHorizontalCard ? 640 : 1019)));
	}

	static void SaveCardImage(SDL_Surface* image, const std::string& cardName)
	{
		std::string temp = cardName;
		temp.append(".png");
		IMG_SavePNG(image, temp.c_str());
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

	// FIXME: Doesn't need to be a 32-bit surface
	SDL_Surface* QuickCreateSurface(int x, int y)
	{
		return SDL_CreateRGBSurface(0, x, y, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	}
};

#endif
