#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <iostream>

#include <unicode/ucnv.h>
#include <unicode/translit.h>
#include <unicode/unistr.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#define DEFAULT_X 90
#define DEFAULT_Y 90

enum SizeSetting {
	one = 0x31,
	two = 0x32,
	three = 0x33,
	four = 0x34,
};

SizeSetting yScale{SizeSetting::one};
SizeSetting xScale{SizeSetting::one};

int main()
{
	std::vector<uint8_t> data(255, 0);
	std::string filename{"/home/wutno/cards/print.bin"};
	std::ifstream card(filename.c_str(), std::ifstream::in | std::ifstream::binary);
	card.read(reinterpret_cast<char *>(&data[0]), data.size());
	card.close();
	if (data.empty()) {
		std::cerr << "Could not open print.bin\n";
		return 1;
	}

	TTF_Init();
	TTF_Font *font = TTF_OpenFont("/home/wutno/cardsb/kochi-gothic-subst.ttf", 34);
	if (font == nullptr) {
		std::cerr << "Could not open font\n";
		return 1;
	}

	SDL_Color color = {0x64, 0x64, 0x96, 0xFF};
	SDL_Surface *cardImage = IMG_Load("1.png");
	if (cardImage == nullptr) {
		std::cerr << "Could not open 15.png\n";
		return 1;
	}

	UErrorCode err{U_ZERO_ERROR};
	UConverter *ucnv = ucnv_open("ibm-942", &err);
	if (err != U_ZERO_ERROR) {
		std::cerr << "Could not start ICU\n";
		return 1;
	}


	std::string test(data.begin(), data.end());
	icu::UnicodeString str(test.c_str(), "shift_jis");

	size_t position{};
	UChar32 lastChar{};
	UChar32 currentChar{};

	// Modified in loop
	int xLocation = DEFAULT_X;
	int yLocation = DEFAULT_Y;

	int tallestGlyph{};

	bool lastUsedScale{false};
	bool usedScale{false};

	int lineSkip = TTF_FontHeight(font);

	while (position < str.length()) {
		currentChar = str.char32At(position);
		SDL_Surface *glyph = TTF_RenderGlyph32_Blended(font, currentChar, color);

		switch (currentChar) {
			case 0x0D:
				{
					if (usedScale) {
						lastUsedScale = true;
						yLocation += lineSkip * 1.75;
					} else {
						lastUsedScale = false;
						yLocation += lineSkip * 1.15;
					}

					xLocation = DEFAULT_X;
					usedScale = false;
					yScale = SizeSetting::one;
					xScale = SizeSetting::one;
					position++;
				}
				continue;
			case 0x11:
				// FIXME: Hack for MT3
				if (lastChar == 0x14) {
					yLocation += lineSkip * 1.65;
					xLocation = DEFAULT_X;
				}
				// FIXME: Single line titles cause issues so we need to skip an extra line but this could happen in other places
				if (lastUsedScale) {
					yLocation += lineSkip * 1.05;
				}
				yScale = SizeSetting::two;
				xScale = SizeSetting::two;
				position++;
				lastChar = currentChar;
				usedScale = true;
				lastUsedScale = true;
				continue;
			case 0x14:
				yScale = SizeSetting::one;
				xScale = SizeSetting::one;
				position++;
				lastChar = currentChar;
				continue;
			case 0x1B:
				{
					if (str.char32At(++position) == 0x73) { // font size
						yScale = static_cast<SizeSetting>(str.char32At(++position));
						xScale = static_cast<SizeSetting>(str.char32At(++position));
						str.remove((position = position - 2), 4);

						if (yScale != SizeSetting::one) {
							usedScale = true;
						}
					}
				}
				continue;
			default:
				break;
		}

		int x_scale = static_cast<int>(xScale) - 0x30;
		int y_scale = static_cast<int>(yScale) - 0x30;

		SDL_Surface *newGlyph = SDL_CreateRGBSurface(
			0,
			glyph->clip_rect.w * (x_scale > 1 ? x_scale * 0.75 : 1),
			glyph->clip_rect.h * (y_scale > 1 ? y_scale * 0.75 : 1),
			32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
		);

		SDL_BlitScaled(glyph, NULL, newGlyph, NULL);

		SDL_Rect location = {xLocation, yLocation, 0, 0};
		SDL_BlitSurface(newGlyph, NULL, cardImage, &location);

		xLocation += newGlyph->clip_rect.w;

		tallestGlyph = (tallestGlyph < newGlyph->clip_rect.h ? newGlyph->clip_rect.h : tallestGlyph);

		SDL_FreeSurface(glyph);
		SDL_FreeSurface(newGlyph);

		lastChar = currentChar;
		position++;
	}

	IMG_SavePNG(cardImage, "file.png");
	TTF_Quit();

	return 0;
}
