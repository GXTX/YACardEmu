#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <random>

#include <unicode/ucnv.h>
#include <unicode/translit.h>
#include <unicode/unistr.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include "ghc/filesystem.hpp"

#define DEFAULT_X 90
#define DEFAULT_Y 110

enum SizeSetting {
	one = 0x31,
	two = 0x32,
	three = 0x33,
	four = 0x34,
};

SizeSetting yScale{SizeSetting::one};
SizeSetting xScale{SizeSetting::one};

int main(int argc, char *argv[])
{
	if (argc != 4) {
		return 1;
	}

	std::string imagesDrive = argv[1];
	std::string cardBaseDrive = argv[2];
	std::string cardName = argv[3];

	std::vector<uint8_t> data(255, 0);

	std::string filename = cardBaseDrive;
	filename.append("print.bin");

	std::ifstream card(filename.c_str(), std::ifstream::in | std::ifstream::binary);
	card.read(reinterpret_cast<char *>(&data[0]), data.size());
	card.close();
	if (data.empty()) {
		std::cerr << "Could not open print.bin\n";
		return 1;
	}

	TTF_Init();
	TTF_Font *font = TTF_OpenFont("kochi-gothic-subst.ttf", 36);
	if (font == nullptr) {
		std::cerr << "Could not open font\n";
		return 1;
	}

	SDL_Color color = {0x64, 0x64, 0x96, 0xFF};

	std::vector<std::string> images{};

	for (const auto &entry: ghc::filesystem::directory_iterator(imagesDrive)) {
		std::string card{entry.path().string()};

		auto find = card.find(".png");

		if (find != std::string::npos) {
			images.emplace_back(card);
		}
	}

	if (images.empty()) {
		std::cerr << "Missing images\n";
		return 1;
	}

	size_t randImage{};
	std::random_device dev;
	std::mt19937 rng(dev());
	std::uniform_int_distribution<std::mt19937::result_type> ran(0,images.size());
	randImage = ran(rng);

	SDL_Surface *cardImage = IMG_Load(images.at(randImage).c_str());
	if (cardImage == nullptr) {
		std::cerr << "Could not open 15.png\n";
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
						yLocation += lineSkip * 2;
					} else {
						lastUsedScale = false;
						yLocation += lineSkip * 1.12;
					}

					xLocation = DEFAULT_X;
					usedScale = false;
					position++;
					yScale = SizeSetting::one;
					xScale = SizeSetting::one;
				}
				continue;
			case 0x11:
				// FIXME: Hack for MT3
				if (lastChar == 0x14) {
					yLocation += lineSkip * 1.50;
					xLocation = DEFAULT_X;
				}
				// FIXME: Single line titles cause issues so we need to skip an extra line but this could happen in other places
				if (lastUsedScale) {
					yLocation += lineSkip;
				}
				yScale = SizeSetting::two;
				//xScale = SizeSetting::two;
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
			glyph->clip_rect.w * (x_scale > 1 ? x_scale : 1),
			glyph->clip_rect.h * (y_scale > 1 ? y_scale : 1),
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

	cardName.append(".png");

	IMG_SavePNG(cardImage, cardName.c_str());
	TTF_Quit();

	return 0;
}
