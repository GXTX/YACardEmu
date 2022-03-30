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

ICUConv::ICUConv()
{
	// TODO: Check error
	ucnv = ucnv_open("Windows-31J", &err);
}

ICUConv::~ICUConv()
{
	ucnv_close(ucnv);
}

bool ICUConv::convertAndPrint(std::vector<uint8_t> &in)
{
	std::string incoming(in.begin(), in.end());

	char16_t buffer[255];

	// TODO: Check error
	ucnv_toUChars(ucnv, buffer, 255, incoming.c_str(), incoming.size(), &err);

	std::u16string u16(buffer);

	TTF_Init();
	TTF_Font *font = TTF_OpenFont("unifont_jp.ttf", 16);
	SDL_Color color = {0xFF, 0x00, 0x00, 0xFF};
	SDL_Surface *surf = TTF_RenderUNICODE_Blended_Wrapped(font, (const uint16_t *)u16.c_str(), color, 255);

    if (!surf) {
        return false;
    }

	IMG_SavePNG(surf, "file.png");

	SDL_FreeSurface(surf);
	TTF_Quit();

	return true;
}
