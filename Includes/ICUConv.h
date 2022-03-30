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

#ifndef ICUCONV_H
#define ICUCONV_H

#include <vector>
#include <codecvt>
#include <locale>
#include <unicode/ucnv.h>

#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"
#include "SDL2/SDL_image.h"

class ICUConv
{
public:
    ICUConv();
    ~ICUConv();

	bool convertAndPrint(std::vector<uint8_t> &in);

	UErrorCode err{U_ZERO_ERROR};
private:
	UConverter *ucnv;
};

#endif
