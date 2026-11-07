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

#include "Printer.h"

#include "AppPaths.h"

namespace {
constexpr const char* kFontName = "kochi-gothic-subst.ttf";

// Look for the bundled font next to the executable, then in the working directory,
// then fall back to common system CJK fonts so Japanese text still renders.
TTF_Font* OpenPrinterFont(int size)
{
	std::vector<ghc::filesystem::path> candidates;
	const auto exeDir = AppPaths::ExecutableDir();
	if (!exeDir.empty()) {
		candidates.push_back(exeDir / kFontName);
		candidates.push_back(exeDir / "fonts" / kFontName);
	}
	candidates.push_back(ghc::filesystem::path(kFontName));
	candidates.push_back(ghc::filesystem::path("fonts") / kFontName);
#ifdef _WIN32
	candidates.push_back("C:\\Windows\\Fonts\\msgothic.ttc");
	candidates.push_back("C:\\Windows\\Fonts\\YuGothR.ttc");
	candidates.push_back("C:\\Windows\\Fonts\\meiryo.ttc");
#else
	candidates.push_back("/usr/share/fonts/truetype/kochi/kochi-gothic-subst.ttf");
	candidates.push_back("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc");
	candidates.push_back("/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc");
	candidates.push_back("/usr/share/fonts/truetype/fonts-japanese-gothic.ttf");
	candidates.push_back("/usr/share/fonts/truetype/takao-gothic/TakaoGothic.ttf");
	candidates.push_back("/usr/share/fonts/ipa-gothic/ipag.ttf");
	candidates.push_back("/System/Library/Fonts/Hiragino Sans GB.ttc");
#endif

	for (const auto& path : candidates) {
		std::error_code ec;
		if (!ghc::filesystem::exists(path, ec))
			continue;
		TTF_Font* font = TTF_OpenFont(path.string().c_str(), size);
		if (font != nullptr) {
			g_logger->info("Printer: using font \"{}\"", path.string());
			return font;
		}
		g_logger->warn("Printer: found \"{}\" but failed to load it: {}", path.string(), TTF_GetError());
	}
	return nullptr;
}
} // namespace

bool Printer::RegisterFont(std::vector<uint8_t>& data)
{
	// Real data size is 0x48, but the first byte indicates slot position
	constexpr uint8_t glyphDataLength = 0x49;
	if (data.size() != glyphDataLength)
		return false;

	uint8_t slot = data[0];
	data.erase(data.begin());
	std::vector<bool> bits = ConvertToBits(data);

	constexpr uint8_t dimensions = 24;
	SDL_Surface* glyph = QuickCreateSurface(dimensions, dimensions);
	if (glyph == nullptr)
		return false;

	SDL_LockSurface(glyph);

	uint32_t* pixels = static_cast<uint32_t*>(glyph->pixels);
	SDL_PixelFormat* pixelFormat = glyph->format;

	for (size_t i = 0; i < bits.size(); i++) {
		if (bits.at(i))
			pixels[i] = SDL_MapRGBA(pixelFormat, 0x64, 0x64, 0x96, 0xFF);
		else
			pixels[i] = SDL_MapRGBA(pixelFormat, 0, 0, 0, 0);
	}

	SDL_UnlockSurface(glyph);

	// The default 25x25 glyph is just slighly too small to match up with our font, so resize it
	SDL_Surface* scaledGlyph = QuickCreateSurface(maxCustomGylphDimensions, maxCustomGylphDimensions);
	SDL_BlitScaled(glyph, NULL, scaledGlyph, NULL);

	SDL_FreeSurface(glyph);
	if (scaledGlyph == nullptr)
		return false;

	// A running print job holds its own reference to the old glyph, it's freed once that's done
	m_customGlyphs.at(slot) = MakeSurface(scaledGlyph);

	return true;
}

bool Printer::QueuePrintLine(std::vector<uint8_t>& data)
{
	enum Mode {
		Now = '0',
		Wait = '1',
	};

	enum BufferControl {
		Clear = '0',
		Append = '1',
	};

	if (m_cardImage == nullptr)
		LoadCardImage();

	// Microsoft in their infinite wisdom defines min in windows.h, so we can't use std::min
	constexpr uint8_t maxOffset = 0x14;
	const uint8_t offset = data[2] < maxOffset ? data[2] : maxOffset;

	if (static_cast<BufferControl>(data[1]) == BufferControl::Clear)
		m_printQueue.clear();

	std::vector<uint8_t> temp = {};
	std::copy(data.begin() + 3, data.end(), std::back_inserter(temp));
	m_printQueue.push_back({ offset, temp });
	m_dirty = true;
	m_hasPrint = true;

	if (static_cast<Mode>(data[0]) == Mode::Now) {
		// The worker gets its own copy of the lines, glyphs and a reference to the image, and the
		// next line starts from an empty queue like it did when PrintLine cleared it afterwards
		Enqueue([this, queue = std::move(m_printQueue), image = m_cardImage, glyphs = m_customGlyphs, horizontal = m_isHorizontalCard]() {
			RenderLines(queue, image.get(), glyphs, horizontal);
		});
		m_printQueue.clear();
	}

	return true;
}

void Printer::Enqueue(std::function<void()> job)
{
	{
		std::lock_guard<std::mutex> lock(m_jobMutex);
		m_jobs.push_back(std::move(job));
	}
	m_jobCv.notify_one();
}

void Printer::WorkerLoop()
{
	for (;;) {
		std::function<void()> job;
		{
			std::unique_lock<std::mutex> lock(m_jobMutex);
			m_jobCv.wait(lock, [this] { return m_stopWorker || !m_jobs.empty(); });
			if (m_jobs.empty())
				return; // Asked to stop and everything queued has been done
			job = std::move(m_jobs.front());
			m_jobs.pop_front();
		}

		try {
			job();
		} catch (const std::exception& e) {
			g_logger->error("Printer::WorkerLoop: Print job failed: {}", e.what());
		} catch (...) {
			g_logger->error("Printer::WorkerLoop: Print job failed with an unknown error");
		}
	}
}

std::function<void()> Printer::MakeFinishJob(const std::string& cardName)
{
	auto queue = std::move(m_printQueue);
	m_printQueue.clear();
	auto image = std::move(m_cardImage);
	m_cardImage.reset();
	const bool save = m_dirty;
	m_dirty = false;
	m_hasPrint = false;

	return [this, queue = std::move(queue), image = std::move(image), glyphs = m_customGlyphs, horizontal = m_isHorizontalCard, name = cardName, save]() {
		if (image == nullptr)
			return;

		RenderLines(queue, image.get(), glyphs, horizontal);

		// Only rewrite the preview when the card was printed on or erased
		if (save && !name.empty())
			SaveCardImage(image.get(), name);
	};
}

void Printer::RenderLines(const std::vector<PrintCommand>& queue, SDL_Surface* image, const std::vector<Surface>& glyphs, bool horizontal)
{
	if (queue.empty() || image == nullptr)
		return;

	TTF_Init();
	constexpr uint8_t defaultFontSize = 36;
	TTF_Font* font = OpenPrinterFont(defaultFontSize);
	if (font == nullptr) {
		g_logger->warn("Printer::RenderLines: Unable to find a usable font. Place \"{}\" next to the executable.", kFontName);
		TTF_Quit();
		return;
	}

	constexpr uint8_t defaultX = 95; // This is good for *most* cards
	const uint8_t defaultY = horizontal ? 85 : 120;
	constexpr SDL_Color color = { 0x64, 0x64, 0x96, 0xFF };
	constexpr uint8_t verticalCardOffset = 4;

	enum Commands {
		Return = '\r',
		Double = 0x11,
		ResetScale = 0x14,
		Escape = 0x1B,
		UseCustomGlyph = 0x67,
		SetScale = 0x73,
	};

	for (const auto& print : queue)
	{
		auto converted = SDL_iconv_string("UTF-8", "SHIFT-JIS", (const char*)print.data.data(), print.data.size());
		if (converted == nullptr) {
			g_logger->error("Printer::RenderLines: iconv couldn't convert the string while printing!");
			break; // Still needs to close the font below
		}

		int xPos = defaultX;
		int yPos = defaultY;
		std::string xScale = "1";
		std::string yScale = "1";
		bool yScaleCompensate = false;
		int maxYSizeForLine = 0;
		utf8_int32_t currentChar = '\0';

		// We don't run this for a single line skip as the FontLineSkip isn't the same as our defaultY
		if (print.offset > 1) {
			yPos += TTF_FontLineSkip(font) * (print.offset);
			if (!horizontal)
				yPos += (print.offset) * (verticalCardOffset - 2); // Don't ask
		}

		// TODO: Have converted be a custom type where we can just iterate with converted[n]
		for (auto i = utf8codepoint(converted, &currentChar); currentChar != '\0'; i = utf8codepoint(i, &currentChar)) {
			// We need to fix our yPos after resetting from a yScale on the same line
			if (yScaleCompensate && maxYSizeForLine) {
				yPos += maxYSizeForLine - TTF_FontLineSkip(font);
				yScaleCompensate = false;
			}

			// Process the character for command bytes
			switch (static_cast<Commands>(currentChar)) {
				case Return:
					TTF_SetFontSize(font, defaultFontSize * std::atoi(yScale.c_str()));
					yPos += TTF_FontLineSkip(font) + (horizontal ? 0 : verticalCardOffset);
					TTF_SetFontSize(font, defaultFontSize);
					xPos = defaultX;
					yScale = '1';
					xScale = '1';
					yScaleCompensate = false;
					maxYSizeForLine = 0;
					continue;
				case Double:
					yScale = '2';
					continue;
				case ResetScale:
					if (yScale[0] != '1')
						yScaleCompensate = true;
					yScale = '1';
					xScale = '1';
					continue;
				case Escape: // FIXME: Don't allow this to overflow
					i = utf8codepoint(i, &currentChar);
					if (currentChar == Commands::SetScale) {
						i = utf8codepoint(i, &currentChar);
						yScale = static_cast<uint8_t>(currentChar);
						i = utf8codepoint(i, &currentChar);
						xScale = static_cast<uint8_t>(currentChar);
					}
					else if (currentChar == Commands::UseCustomGlyph) {
						i = utf8codepoint(i, &currentChar);
						if (currentChar > 0xFF) {
							g_logger->warn("Printer::RenderLines: Attempted to use out of range glyph {:X}", currentChar);
							continue;
						}
						if (glyphs.at(currentChar) == nullptr) {
							g_logger->warn("Printer::RenderLines: Game never set glyph {:X} but tried to use it", currentChar);
							continue;
						}
						SDL_Rect location = { xPos, yPos, 0, 0 };

						if (xScale[0] != '1' || yScale[0] != '1') {
							auto scaledCustomGlyph = QuickCreateSurface(maxCustomGylphDimensions * std::atoi(xScale.c_str()), maxCustomGylphDimensions * std::atoi(yScale.c_str()));
							SDL_BlitScaled(glyphs.at(currentChar).get(), NULL, scaledCustomGlyph, NULL);
							if (yScale[0] != '1')
								location.y -= (scaledCustomGlyph->h / std::atoi(yScale.c_str()));
							SDL_BlitSurface(scaledCustomGlyph, NULL, image, &location);
							xPos += scaledCustomGlyph->w;
							SDL_FreeSurface(scaledCustomGlyph);
							scaledCustomGlyph = nullptr;
						}
						else {
							SDL_BlitSurface(glyphs.at(currentChar).get(), NULL, image, &location);
							xPos += glyphs.at(currentChar)->w;
						}
					}
					continue;
				default:
					// UseCustomGlyph ('g') and SetScale ('s') only mean something right after an
					// Escape, which is handled above. On their own they are ordinary letters and
					// get drawn like any other character.
					break;
			}

			// TODO: Solid produces a better glyph but with non-transparent backgrounds
			SDL_Surface* glyph = TTF_RenderGlyph32_Blended(font, currentChar, color);
			if (glyph == nullptr) {
				// The font can't draw this character, skip it instead of crashing the print thread
				g_logger->warn("Printer::RenderLines: Unable to render glyph {:X}: {}", currentChar, TTF_GetError());
				continue;
			}
			SDL_Surface* scaledGlyph = QuickCreateSurface(glyph->clip_rect.w * std::atoi(xScale.c_str()), glyph->clip_rect.h * std::atoi(yScale.c_str()));
			if (scaledGlyph == nullptr) {
				SDL_FreeSurface(glyph);
				continue;
			}
			SDL_BlitScaled(glyph, NULL, scaledGlyph, NULL);

			// Final blit
			SDL_Rect location = { xPos, yPos, 0, 0 };
			SDL_BlitSurface(scaledGlyph, NULL, image, &location);

			// Used with yScaleCompensate when we reset scale on the same line
			maxYSizeForLine = scaledGlyph->h > maxYSizeForLine ? scaledGlyph->h : maxYSizeForLine;

			int advance = 0;
			TTF_GlyphMetrics32(font, currentChar, NULL, NULL, NULL, NULL, &advance);
			// TODO: F-Zero AX has odd spacing, if we use the default spacing it's too much, but it works for every other game?
#if 0
			if (currentChar == 0x20)
				xPos += 15 * std::atoi(xScale.c_str());
			else
#endif
				xPos += advance * std::atoi(xScale.c_str());

			SDL_FreeSurface(glyph);
			SDL_FreeSurface(scaledGlyph);
		}
		SDL_free(converted);
	}
	TTF_CloseFont(font);
	TTF_Quit();
}
