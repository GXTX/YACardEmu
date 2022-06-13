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

enum SizeSetting {
	one = 0x31,
	two = 0x32,
	three = 0x33,
	four = 0x34,
};

SizeSetting y{SizeSetting::one};
SizeSetting x{SizeSetting::one};

bool contr{false};

bool findControl(UChar32 str)
{
	if (str == 0x1B) {
		contr = true;
		return true;
	}

	if (str == 0x11) {
		y = SizeSetting::two;
		x = SizeSetting::two;
		return true;
	}

	if (str == 0x14) {
		y = SizeSetting::one;
		x = SizeSetting::one;
		return true;
	}

	return false;
}

int resize(int size, SizeSetting scale)
{
	if (scale == SizeSetting::one) {
		return size;
	}

	return size * ((static_cast<int>(scale) - 0x30) * 0.75);
}

int main()
{
	std::vector<uint8_t> data(255, 0);
	std::string filename{"/home/wutno/cardsb/print.bin"};
	std::ifstream card(filename.c_str(), std::ifstream::in | std::ifstream::binary);
	card.read(reinterpret_cast<char *>(&data[0]), data.size());
	card.close();
	if (data.empty()) {
		std::cerr << "Could not open print.bin\n";
		return 1;
	}

	TTF_Init();
	TTF_Font *font = TTF_OpenFont("/home/wutno/cardsb/kochi-gothic-subst.ttf", 42);
	if (font == nullptr) {
		std::cerr << "Could not open font\n";
		return 1;
	}

	SDL_Color color = {0x64, 0x64, 0x96, 0xFF};
	SDL_Surface *cardImage = IMG_Load("15.png");
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

	std::vector<std::string> lines(255, {0});
	int i = 0;
	while (true) {
		auto location = std::find(data.begin(), data.end(), 0x0D);

		if (location != std::end(data)) {
			lines.at(i).assign(data.begin(), location);
			data.erase(data.begin(), location + 1);
		} else {
			if (!data.empty()) {
				lines.at(i).assign(data.begin(), data.end());
				data.erase(data.begin(), data.end()); // remove \r
			}
			break;
		}

		i++;
	}

	int horz = 85, virt = 110, cbuffsize = 255;
	int advance = TTF_FontLineSkip(font);
	bool hasDoubleSize{false};
	bool fourteen{false};

	for (const auto &src : lines) {
		icu::UnicodeString str(src.c_str(), "shift_jis");

		size_t position{};
		while (position < str.length()) {
			std::printf("%d", position);
			std::cout << ":" << str.length() << std::endl;

			UChar32 lastChar{};
			UChar32 currentChar{};
			while (findControl(str.char32At(position))) {
				currentChar = str.char32At(position);
				if (contr) {
					if (str.char32At(++position) == 0x73) { // font size
						y = static_cast<SizeSetting>(str.char32At(++position));
						x = static_cast<SizeSetting>(str.char32At(++position));
						str.remove((position = position - 3), 4);
					}
					// FIXME: font selection
				} else {
					if (str.char32At(position) == 0x11) {
						hasDoubleSize = true;
						if (lastChar == 0x14) {
							horz = 85;
							virt += advance + 1 + (hasDoubleSize ? 17 : 0);
						}
					}
					str.remove(position, 1);
				}
				lastChar = currentChar;
			}
			contr = false;

			bool isSpace{false};
			if (str.char32At(position)) {
				isSpace = true;
			}

			SDL_Surface *textSurf = TTF_RenderGlyph32_Blended(font, str.char32At(position), color);
			position++;
			if (textSurf == nullptr) {
				continue;
			}

			// surface we use to use to resize textSurf
			SDL_Surface *newText = SDL_CreateRGBSurface(
				0,
				resize(textSurf->clip_rect.w, x),
				resize(textSurf->clip_rect.h, y),
				32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
			);

			SDL_BlitScaled(textSurf, NULL, newText, NULL);

			SDL_Rect location = {horz, virt, 0, 0};
			SDL_BlitSurface(newText, NULL, cardImage, &location);
			SDL_FreeSurface(textSurf);

			horz += (newText->clip_rect.w / 1.25) + 2;
		}

		virt += advance + 1 + (hasDoubleSize ? 17 : 0);
		horz = 85;
		hasDoubleSize = false;
	}

	IMG_SavePNG(cardImage, "file.png");
	TTF_Quit();

	return 0;
}
