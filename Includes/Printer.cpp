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

#include "Printer.h"

bool Printer::RegisterFont(std::vector<uint8_t>& data)
{
	// Real data size is 0x48, but the first byte indicates slot position
	constexpr uint8_t glyphDataLength = 0x49;
	if (data.size() != glyphDataLength)
		return false;

	uint8_t slot = data[0];
	data.erase(data.begin());
	std::vector<bool> bits = ConvertToBits(data);

	constexpr uint8_t maxDimentions = 24;
	// FIXME: Doesn't need to be a 32-bit surface
	SDL_Surface* glyph = SDL_CreateRGBSurface(
		0,
		maxDimentions,
		maxDimentions,
		32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
	);

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
	constexpr uint8_t newDimentions = 30;
	SDL_Surface* scaledGlyph = SDL_CreateRGBSurface(
		0,
		newDimentions,
		newDimentions,
		32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
	);
	SDL_BlitScaled(glyph, NULL, scaledGlyph, NULL);

	SDL_FreeSurface(glyph);
	if (m_customGlyphs.at(slot) != nullptr)
		SDL_FreeSurface(m_customGlyphs.at(slot));
	m_customGlyphs.at(slot) = scaledGlyph;

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
		LoadCardImage(m_localName);

	// Microsoft in their infinite wisdom defines min in windows.h, so we can't use std::min
	constexpr uint8_t maxOffset = 0x14;
	const uint8_t offset = data[2] < maxOffset ? data[2] : maxOffset;

	if (static_cast<BufferControl>(data[1]) == BufferControl::Clear)
		m_printQueue.clear();

	std::vector<uint8_t> temp = {};
	std::copy(data.begin() + 3, data.end(), std::back_inserter(temp));
	m_printQueue.push_back({ offset, temp });

	if (static_cast<Mode>(data[0]) == Mode::Now)
		PrintLine();

	return true;
}

void Printer::PrintLine()
{
	if (m_printQueue.empty())
		return;

	TTF_Init();
	constexpr uint8_t defaultFontSize = 36;
	TTF_Font* font = TTF_OpenFont("kochi-gothic-subst.ttf", defaultFontSize);
	if (font == nullptr) {
		g_logger->warn("Printer::PrintLine: Unable to initialize TTF_Font with \"kochi-gothic-subst.ttf\"");
		return;
	}

	constexpr uint8_t defaultX = 95; // This is good for *most* cards
	const uint8_t defaultY = m_horizontalCard ? 85 : 120;
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

	for (const auto& print : m_printQueue)
	{
		auto converted = SDL_iconv_string("UTF-8", "SHIFT-JIS", (const char*)&print.data[0], print.data.size() + 1);
		if (converted == nullptr) {
			g_logger->error("Printer::PrintLine: iconv couldn't convert the string while printing!");
			return;
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
			yPos += TTF_FontLineSkip(font) * (print.offset - 1);
			if (!m_horizontalCard)
				yPos += (print.offset - 1) * verticalCardOffset;
		}

		// TODO: Have converted be a custom type where we can just iterate with converted[n]
		for (auto i = utf8codepoint(converted, &currentChar); currentChar != '\0'; i = utf8codepoint(i, &currentChar)) {
			// We need to fix our yPos after resetting from a yScale on the same line
			if (yScaleCompensate) {
				yPos += maxYSizeForLine - TTF_FontLineSkip(font);
				yScaleCompensate = false;
			}

			// Process the character for command bytes
			switch (static_cast<Commands>(currentChar)) {
				case Return:
					TTF_SetFontSize(font, defaultFontSize * std::atoi(yScale.c_str()));
					yPos += TTF_FontLineSkip(font) + (m_horizontalCard ? 0 : verticalCardOffset);
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
							g_logger->warn("Printer::PrintLine: Attempted to use out of range glyph {:X}", currentChar);
							continue;
						}
						if (m_customGlyphs.at(currentChar) == nullptr) {
							g_logger->warn("Printer::PrintLine: Game never set glyph {:X} but tried to use it", currentChar);
							continue;
						}
						SDL_Rect location = { xPos, yPos, 0, 0 };
						SDL_BlitSurface(m_customGlyphs.at(currentChar), NULL, m_cardImage, &location);
						xPos += m_customGlyphs.at(currentChar)->w;
					}
					continue;
			}

			// TODO: Solid produces a better glyph but with non-transparent backgrounds
			SDL_Surface* glyph = TTF_RenderGlyph32_Blended(font, currentChar, color);
			SDL_Surface* scaledGlyph = SDL_CreateRGBSurface(
				0,
				glyph->clip_rect.w * std::atoi(xScale.c_str()),
				glyph->clip_rect.h * std::atoi(yScale.c_str()),
				32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
			);
			SDL_BlitScaled(glyph, NULL, scaledGlyph, NULL);

			// Final blit
			SDL_Rect location = { xPos, yPos, 0, 0 };
			SDL_BlitSurface(scaledGlyph, NULL, m_cardImage, &location);

			// Used with yScaleCompensate when we reset scale on the same line
			maxYSizeForLine = scaledGlyph->h > maxYSizeForLine ? scaledGlyph->h : maxYSizeForLine;

			int advance = 0;
			TTF_GlyphMetrics32(font, currentChar, NULL, NULL, NULL, NULL, &advance);
			// TODO: F-Zero AX has odd spacing, if we use the default spacing it's too much, but it works for every other game?
#if 0
			if (currentChar == 0x20)
				xPos += 15 * std::atoi(&xScale);
			else
#endif
			xPos += advance * std::atoi(xScale.c_str());

			SDL_FreeSurface(glyph);
			SDL_FreeSurface(scaledGlyph);
		}
		SDL_free(converted);
	}
	m_printQueue.clear();
	TTF_CloseFont(font);
	TTF_Quit();
}
