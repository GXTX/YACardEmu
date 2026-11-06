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
#include <cctype>
#include <system_error>

#include <SDL2/SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/fmt/bin_to_hex.h"

#include "ghc/filesystem.hpp"

#include "AppPaths.h"

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

	// Every card starts from a clean base image. The preview saved with the card only shows what
	// was last printed on it, it is never used as a canvas: the game is the one that decides what
	// ends up on the card, and drawing on top of the old print made new text pile up on the old.
	// The saved preview stays as it is until the game prints (or erases) something on the card.
	void InsertCard(std::string& cardName)
	{
		m_localName = cardName;
		m_printQueue.clear();
		m_dirty = false;
		LoadCardImage();
	}

	void RemoveCard(std::string& cardName)
	{
		// Takes the queue and image away from us now, so the next card can start right away
		Enqueue(MakeFinishJob(cardName));
		m_localName.clear();
	}

	// The game erased the card: start over from a clean base image and forget any lines that
	// were still waiting to be printed, they were meant for the old print.
	// The clean image is saved when the card is removed, so the preview shows the erased card.
	void Erase()
	{
		m_printQueue.clear();
		m_cardImage.reset();
		m_dirty = true;
		if (!m_localName.empty())
			LoadCardImage();
	}

	// Forget the card without saving anything
	void Discard()
	{
		m_cardImage.reset();
		m_printQueue.clear();
		m_localName.clear();
		m_dirty = false;
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

	// Something was printed or erased since the card went in, so the saved preview is out of date.
	// Cards that pass through untouched keep the preview they already have.
	bool m_dirty = false;

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

	// Starts a fresh card image from a random base image
	void LoadCardImage()
	{
		// Base card images live in "images", next to where we were started or next to the executable
		std::vector<std::string> images = {};
		const auto imageDir = AppPaths::Find("images");

		std::error_code ec;
		if (!imageDir.empty() && ghc::filesystem::is_directory(imageDir, ec)) {
			ghc::filesystem::directory_iterator it(imageDir, ec);
			for (const ghc::filesystem::directory_iterator end; !ec && it != end; it.increment(ec)) {
				std::string ext = it->path().extension().string();
				for (auto& c : ext)
					c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				if (ext == ".png" && it->is_regular_file(ec))
					images.emplace_back(it->path().string());
			}
		} else {
			// FIXME: Create the folder for the user
			g_logger->warn("Printer::LoadCardImage: Image folder does not exist, put an \"images\" folder with base card images (.png) next to the executable");
		}

		if (!images.empty()) {
			std::random_device dev;
			std::mt19937 twist(dev());
			std::uniform_int_distribution<std::size_t> rng(0, images.size() - 1);

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
