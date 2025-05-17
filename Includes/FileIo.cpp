#include "FileIo.h"

bool FileIo::Read()
{
	std::string fullPath = "m_cardSettings->cardPath + m_cardSettings->cardName";
	g_logger->debug("CardIo::ReadCard: Reading card data from - {}", fullPath);

	// TODO: Should we actually be seeding zero's when the file doesn't exist?
	std::string readBack = {};

	m_data.clear();

	if (ghc::filesystem::exists(m_fullPath->c_str())) {
		auto fileSize = ghc::filesystem::file_size(m_fullPath->c_str());
		if (fileSize > 0 && !(fileSize % TRACK_SIZE)) {
			std::ifstream card(m_fullPath->c_str(), std::ifstream::in | std::ifstream::binary);
			readBack.resize(fileSize);
			card.read(&readBack[0], fileSize);
			card.close();
			g_logger->trace("CardIo::ReadCard: {:Xn}", spdlog::to_hex(readBack));
		}
		else {
			g_logger->warn("Incorrect card size");
			return false;
		}
	}

	auto offset = 0;
	for (size_t i = 0; i < (readBack.size() / TRACK_SIZE); i++) {
		std::copy(readBack.begin() + offset, readBack.begin() + offset + TRACK_SIZE, std::back_inserter(m_data.at(i)));
		offset += TRACK_SIZE;
	}

	m_backupData = m_data;
	// TODO: Actually do something with this if something happens...
	//if (backupData.empty())
	//	std::copy(data.begin(), data.end(), std::back_inserter(backupData));

	return true;
}

bool FileIo::Write()
{
	// FIXME: I don't want to require multiple parts of the parents cardSettings struct
#if 0
		// Should only happen when issued from dispenser, we want to avoid overwriting a previous card
	if (!m_cardSettings->insertedCard) {
		auto i = 0;
		while (i < 1000) {
			// TODO: It would be nicer if the generated name was abc.123.bin instead of abc.bin.123
			std::string newCardName = m_cardSettings->cardName;

			// $3 contains any numbers a sqeuence of "*.bin."
			auto find = std::regex_replace(newCardName, std::regex("(.*)(\\.bin\\.)(\\d+)"), "$3");

			if (newCardName == find) {
				// Add ".n" after ".bin"
				newCardName.append("." + std::to_string(i));
			}
			else {
				// Increment the numbers contained in $3
				auto strSize = find.size();
				find = std::to_string(std::stoi(find) + i);
				newCardName = newCardName.substr(0, newCardName.size() - strSize) + find;
			}

			// Perfenece to not having a number...
			if (i == 0) {
				newCardName = m_cardSettings->cardName;
			}

			auto fullName = m_cardSettings->cardPath + newCardName;

			if (!ghc::filesystem::exists(fullName.c_str())) {
				m_cardSettings->cardName = newCardName;
				break;
			}
			i++;
		}
	}
#endif

	//auto fullPath = m_cardSettings->cardPath + m_cardSettings->cardName;
	g_logger->debug("CardIo::WriteCard: Writing card data to {}", m_fullPath->c_str());

	std::string writeBack;
	for (const auto& track : m_data) {
		if (track.empty())
			continue;
		std::copy(track.begin(), track.end(), std::back_inserter(writeBack));
	}

	g_logger->trace("CardIo::WriteCard: 0:{0:Xn} 1:{1:Xn} 2:{2:Xn}", spdlog::to_hex(m_data.at(0)), spdlog::to_hex(m_data.at(1)), spdlog::to_hex(m_data.at(2)));
	g_logger->trace("CardIo::WriteCard: {:Xn}", spdlog::to_hex(writeBack));

	if (writeBack.empty()) {
		g_logger->warn("CardIo::WriteCard: Attempted to write a zero sized card!");
	}
	else {
		std::ofstream card;
		card.open(m_fullPath->c_str(), std::ofstream::out | std::ofstream::binary);
		card.write(writeBack.c_str(), writeBack.size());
		card.close();

#if 0
		// FIXME: This is dirty
		mINI::INIFile config("config.ini");
		mINI::INIStructure ini;
		config.read(ini);
		ini["config"]["autoselectedcard"] = m_cardSettings->cardName;
		config.write(ini);
#endif
	}
}