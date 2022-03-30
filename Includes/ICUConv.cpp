/*
    YACardEmu
    ----------------
    Copyright (C) 2022 wutno (https://github.com/GXTX)


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

#include "ICUConv.h"
#include <iostream>

ICUConv::ICUConv()
{
	// TODO: Check error
	ucnv = ucnv_open("Windows-31J", &err);
}

ICUConv::~ICUConv()
{
	ucnv_close(ucnv);
}

bool ICUConv::convertAndPrint(int lineOffset, std::vector<uint8_t> &in)
{
#if 0
	std::string incoming(in.begin(), in.end());

	char16_t buffer[255];

	// TODO: Check error
	ucnv_toUChars(ucnv, buffer, 255, incoming.c_str(), incoming.size(), &err);

	std::u16string u16(buffer);

	TTF_Init();
	TTF_Font *font = TTF_OpenFont("kochi-gothic-subst.ttf", 38);
	SDL_Color color = {0x100, 0x100, 0x150, 0xFF};
	SDL_Surface *cardImage = IMG_Load("2.png");

	// Initial offset
	int virt = 100 + (14 * lineOffset);
	int horz = 85;

	for (const char16_t c : u16) {
		if (c == 0x0D) {
			TTF_SetFontStyle(font, TTF_STYLE_NORMAL);
			virt += 14; // \n
			continue;
		}

		if (c == 0x11) {
			TTF_SetFontStyle(font, TTF_STYLE_BOLD);
		} else if (c == 0x14) {
			TTF_SetFontStyle(font, TTF_STYLE_NORMAL);
		}

		SDL_Rect location = {horz, virt, 0, 0};
		SDL_Surface *textSurf = TTF_RenderUNICODE_Solid(font, (const uint16_t *)&c, color);
		SDL_BlitSurface(textSurf, NULL, cardImage, &location);
		SDL_FreeSurface(textSurf);

		int advance = 0;
		TTF_GlyphMetrics(font, c, NULL, NULL, NULL, NULL, &advance);
		horz += advance;
	}

	IMG_SavePNG(cardImage, "file.png");

	SDL_FreeSurface(cardImage);
	TTF_Quit();
#endif

	return true;
}
