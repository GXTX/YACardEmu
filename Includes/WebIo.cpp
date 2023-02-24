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

#include "WebIo.h"

WebIo::WebIo(CardIo::Settings *card, std::string host, int port, std::atomic<bool> *running)
{
	m_card = card;
	m_host = host;
	m_port = port;
	g_running = running;
	SetupRoutes();
}

WebIo::~WebIo()
{
	m_svr.stop();
}

bool WebIo::Spawn()
{
	std::thread(&WebIo::StartServer, this).detach();
	// Need to wait before checking the status otherwise we'll crash
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	return m_svr.is_running();
}

void WebIo::StartServer()
{
	spdlog::info("Starting API server...");
	m_svr.set_mount_point("/", "public");

	m_svr.Get(R"(/api/v1/(\w+))", [&](const httplib::Request& req, httplib::Response& res) {
		Router(req, res);
	});

	m_svr.Post(R"(/api/v1/(\w+))", [&](const httplib::Request& req, httplib::Response& res) {
		Router(req, res);
	});

	m_svr.Delete(R"(/api/v1/(\w+))", [&](const httplib::Request& req, httplib::Response& res) {
		Router(req, res);
	});

	if (!m_svr.listen(m_host.c_str(), m_port)) {
		spdlog::critical("Failed to start API server!");
	}
}

void WebIo::Router(const httplib::Request &req, httplib::Response &res)
{
	const auto route = req.matches[1].str();

	spdlog::debug("WebIo::Router: {0} -> {1}", req.method, route);

	switch (m_routeValues[route]) {
		case Routes::cards:
			res.set_content(GenerateCardListJSON(m_card->cardPath), "application/json");
			break;
		case Routes::hasCard:
			res.set_content(m_card->hasCard ? "true" : "false", "application/json");
			break;
		case Routes::readyCard:
			res.set_content(m_card->waitingForCard ? "true" : "false", "application/json");
		break;
		case Routes::insertedCard:
			InsertedCard(req, res);
			break;
		case Routes::dispenser:
			if (req.method == "DELETE") {
				m_card->reportDispenserEmpty = true;
			} else if (req.method == "POST") {
				m_card->reportDispenserEmpty = false;
			}
			res.set_content(m_card->reportDispenserEmpty ? "empty" : "full", "text/plain");
			break;
		case Routes::stop:
			m_svr.stop();
			// g_running controls the main loop, if we kill it before http is flushed then we'll crash
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			*g_running = false;
			break;
		default:
			spdlog::warn("WebIo::Router: Unsupported route \"{}\"", req.path);
			res.status = 404;
			break;
	}
}

void WebIo::InsertedCard(const httplib::Request& req, httplib::Response& res)
{
	if (req.method == "DELETE") {
		res.set_content("null", "application/json");
		return;
	}

	if (req.method == "POST") {
		if (req.has_param("loadonly")) {
			m_card->insertedCard = true;
		}

		if (req.has_param("cardname")) {
			m_card->cardName = req.get_param_value("cardname");
		}
	}

	if (req.has_param("redirect")) {
		res.set_redirect("/");
		return;
	}

	res.set_content(" { \"cardname\":\"" + m_card->cardName + "\", \"inserted\":" + (m_card->insertedCard ? "true" : "false") + " }", "application/json");
}

const std::string WebIo::GenerateCardListJSON(std::string basepath)
{
	std::string list{"["};

	if (!m_card->cardName.empty()) {
		list.append("{\"name\":\"" + m_card->cardName + "\",\"image\":\"""\"},");
	}

	for (const auto& entry : ghc::filesystem::directory_iterator(basepath)) {
		std::string card{entry.path().string()};

		if (card.find(m_card->cardName) != std::string::npos) {
			continue;
		}

		auto find = card.find(".track_0");

		if (find != std::string::npos) {
			card.replace(find, 8, "");
			list.append("{\"name\":\"");
#ifdef _WIN32
			list.append(card.substr(card.find_last_of("\\") + 1));
#else
			list.append(card.substr(card.find_last_of("/") + 1));
#endif
			// TODO: Support card images
			list.append("\",\"image\":\"");

#if 0
			auto find2 = card.find(".bin");
			if (find2 != std::string::npos) {
				card.replace(find2, 4, ".png");
			}

			std::string base64{};

			if (ghc::filesystem::exists(card)) {
				std::ifstream img(card.c_str(),
				                  std::ifstream::in |
				                          std::ifstream::binary);

				base64.resize(ghc::filesystem::file_size(card));

				img.read(reinterpret_cast<char*>(&base64[0]),
				         base64.size());
				img.close();
			}

			std::string encoded = base64_encode(base64, false);
			list.append("data:image/png;base64, ");
			list.append(encoded);
#endif
			list.append("\"},");
		}
	}

	// remove the errant comma
	if (list.compare("[") != 0) {
		list.pop_back();
	}

	list.append("]");
	return list;
}
