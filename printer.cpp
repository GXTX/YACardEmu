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

int main()
{
	std::vector<uint8_t> data(255, 0);
	std::string filename{"/home/wutno/cards/print.bin"};

	std::ifstream card(filename.c_str(), std::ifstream::in | std::ifstream::binary);

	card.read(reinterpret_cast<char *>(&data[0]), data.size());
	card.close();

	if (data.empty()) {
		return 1;
	}

	std::vector<std::string> lines(255, {0});

	int i = 0;
	while (true) {
		auto loca = std::find(data.begin(), data.end(), 0x0D);

		if (loca != std::end(data)) {
			lines.at(i).assign(data.begin(), loca);
			data.erase(data.begin(), loca + 1);
		} else {
			if (!data.empty()) {
				lines.at(i).assign(data.begin(), data.end());
				data.erase(data.begin(), data.end()); //cleanup
			}
			break;
		}

		i++;
	}

	icu::UnicodeString fullu = ":: Fullwidth-Halfwidth;";
	icu::UnicodeString halfu = ":: Halfwidth-Fullwidth;";

	TTF_Init();
	TTF_Font *font = TTF_OpenFont("/home/wutno/Projects/YACardEmu/build/kochi-gothic-subst.ttf", 30);
	if (font == nullptr) {
		return 1;
	}

	SDL_Color color = {0x64, 0x64, 0x96, 0xFF};
	SDL_Surface *cardImage = IMG_Load("1.png");
	if (cardImage == nullptr) {
		return 1;
	}

	UErrorCode err{U_ZERO_ERROR};
	UConverter *ucnv = ucnv_open("ibm-942", &err);

	if (err != U_ZERO_ERROR) {
		return 1;
	}

	int horz = 90, virt = 100, cbuffsize = 255;

	for (const auto &src : lines) {
		icu::UnicodeString str{};

		size_t reqSize = sizeof(UChar) * src.size();
		UChar *srcBuffer = (UChar *)std::malloc(sizeof(UChar) * src.size());
		ucnv_toUChars(ucnv, srcBuffer, reqSize, src.c_str(), src.size(), &err);
		str.setTo(srcBuffer, reqSize);
		free(srcBuffer);


		if (src.find('\x11') != std::string::npos) {
			//literate(halfu, &str);
			//TTF_SetFontStyle(font, TTF_STYLE_BOLD);
			TTF_SetFontSize(font, 42);

			//UErrorCode status = U_ZERO_ERROR;
			//icu::Transliterator *trans = icu::Transliterator::createInstance(halfu, UTRANS_FORWARD, status);
			//trans->transliterate(str, 0, str.length());


			//UTransliterator *trans = utrans_open(half.c_str(), UTRANS_FORWARD, nullptr, -1, nullptr, &err);


			//icu::Transliterator *trans = icu::Transliterator::createInstance();

			//utrans_transUChars(trans, dest, &cbuffsize, 255, 0, &cbuffsize, &err);
			//utrans_close(trans);
		} else {
			//UTransliterator *trans = utrans_open(full.c_str(), UTRANS_FORWARD, nullptr, -1, nullptr, &err);
			//utrans_transUChars(trans, dest, &cbuffsize, 255, 0, &cbuffsize, &err);
			//utrans_close(trans);
			//literate(fullu, &str);
			TTF_SetFontStyle(font, TTF_STYLE_NORMAL);
			TTF_SetFontSize(font, 30);
		}

		SDL_Rect location = {horz, virt, 0, 0};
		SDL_Surface *textSurf = TTF_RenderUNICODE_Blended(font, (const Uint16 *)str.getBuffer(), color);
		SDL_BlitSurface(textSurf, NULL, cardImage, &location);
		SDL_FreeSurface(textSurf);

		int advance = 0;
		TTF_GlyphMetrics(font, '\r', NULL, NULL, NULL, NULL, &advance);
		virt += advance + 10;
	}

	IMG_SavePNG(cardImage, "file.png");
	TTF_Quit();

	return 0;
}
