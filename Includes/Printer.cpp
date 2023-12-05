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
	constexpr const uint8_t glyphDataLength = 0x49;
	if (data.size() != glyphDataLength)
		return false;

	uint8_t slot = data[0];
	data.erase(data.begin());
	std::vector<bool> bits = ConvertToBits(data);

	constexpr const uint8_t maxWidth = 24;
	constexpr const uint8_t maxHeight = 24;
	// FIXME: Doesn't need to be a 32-bit surface
	SDL_Surface* glyph = SDL_CreateRGBSurface(
		0,
		maxWidth,
		maxHeight,
		32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
	);

	if (glyph == nullptr)
		return false;

	SDL_LockSurface(glyph);

	uint32_t* pixels = static_cast<uint32_t*>(glyph->pixels);
	SDL_PixelFormat* pixelFormat = glyph->format;

	for (auto i = 0; i < bits.size(); i++) {
		if (bits.at(i))
			pixels[i] = SDL_MapRGBA(pixelFormat, 64, 64, 96, 255);
		else
			pixels[i] = SDL_MapRGBA(pixelFormat, 0, 0, 0, 0);
	}

	SDL_UnlockSurface(glyph);

	m_customGlyphs.at(slot) = glyph;

	return true;
}

bool Printer::QueuePrintLine(std::vector<uint8_t>& data)
{
	enum Mode {
		Wait = '0',
		Now = '1',
	};

	enum BufferControl {
		Clear = '0',
		Append = '1',
	};

	// TODO: Handle wait state

	if (static_cast<BufferControl>(data[1]) == BufferControl::Clear)
		m_printQueue.clear();

	std::vector<uint8_t> temp = {};
	std::copy(data.begin() + 2, data.end(), std::back_inserter(temp));
	m_printQueue.emplace_back(temp);

	return true;
}

void Printer::PrintLine()
{
#if 0
	TTF_Init();

	constexpr const uint8_t defaultFontSize = 36;
	TTF_Font* font = TTF_OpenFont("kochi-gothic-subst.ttf", defaultFontSize);
	if (font == nullptr)
	{
		g_logger->warn("Printer::PrintLine: Unable to initialize TTF_Font with \"kochi-gothic-subst.ttf\"");
		return;
	}

	// FIXME: Allow a pool of images to be randomly chosen
	SDL_Surface* cardImage = IMG_Load("1.png");
	if (cardImage == nullptr)
	{
		g_logger->warn("Printer::PrintLine: Could not create surface from \"1.png\"");
		return;
	}

	constexpr const uint8_t defaultX = 95;
	constexpr const uint8_t defaultY = 120; // FIXME: Needs to be variable, 120 for vertical, 85 for horiz

	enum Commands {
		Return = '\r',
		Double = 0x11,
		ResetScale = 0x14,
		Escape = '\e',
		UseCustomGlyph = 0x67,
		SetScale = 0x73,
	};

	const SDL_Color color = { 0x64, 0x64, 0x96, 0xFF };

	for (const auto& x : m_printQueue)
	{
		std::string temp(x.begin(), x.end());
		icu::UnicodeString buffer(temp.c_str(), "shift_jis");

		UChar32 currentChar = {};
		int xPos = defaultX;
		int yPos = defaultY;
		uint8_t xScale = '1';
		uint8_t yScale = '1';
		bool yScaleCompensate = false;
		int maxYSizeForLine = 0;

		for (auto i = 0; i < buffer.length(); i++) {
			currentChar = buffer.char32At(i);

			// We need to fix our yPos after resetting from a yScale on the same line
			if (yScaleCompensate)
			{
				yPos += maxYSizeForLine - TTF_FontLineSkip(font);
				yScaleCompensate = false;
			}

			// Process the character for command bytes
			switch (static_cast<Commands>(currentChar)) {
				case Return:
				if (yScale != '1')
					TTF_SetFontSize(font, defaultFontSize * static_cast<int>(yScale - '0'));
				yPos += TTF_FontLineSkip(font) + 4; // FIXME: +4 for vertical cards : 0 for horiz
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
				if (yScale != '1')
					yScaleCompensate = true;
				yScale = '1';
				xScale = '1';
				continue;
			case Escape: // FIXME: Don't allow this to overflow
				currentChar = buffer.char32At(++i);
				if (currentChar == Commands::SetScale) {
					yScale = buffer.char32At(++i);
					xScale = buffer.char32At(++i);
				}
				continue;
			}

			// FIXME: Solid does not function as expected, scaled characters aren't blitted, Blended works, also doesn't need to be 32bit depth
			SDL_Surface* glyph = TTF_RenderGlyph32_Blended(font, currentChar, color);
			SDL_Surface* scaledGlyph = SDL_CreateRGBSurface(
				0,
				glyph->clip_rect.w * (static_cast<int>(xScale - '0')),
				glyph->clip_rect.h * (static_cast<int>(yScale - '0')),
				32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
			);
			SDL_BlitScaled(glyph, NULL, scaledGlyph, NULL);

			// Final blit
			SDL_Rect location = { xPos, yPos, 0, 0 };
			SDL_BlitSurface(scaledGlyph, NULL, cardImage, &location);

			// Used with yScaleCompensate when we reset scale on the same line
			maxYSizeForLine = scaledGlyph->h > maxYSizeForLine ? scaledGlyph->h : maxYSizeForLine;

			int advance = 0;
			TTF_GlyphMetrics(font, currentChar, NULL, NULL, NULL, NULL, &advance);
			if (xScale == '1')
				xPos += advance;
			else
				xPos += advance * static_cast<int>(xScale - '0');

			SDL_FreeSurface(glyph);
			SDL_FreeSurface(scaledGlyph);
		}

	}
#endif
}
