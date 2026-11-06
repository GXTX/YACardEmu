/*
    YACardEmu
    ----------------
    Copyright (C) 2020-2025 wutno (https://github.com/GXTX)


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
#include "CardName.h"
#include "StringEscape.h"

#include <algorithm>
#include <cctype>

namespace {
// Browsers attach an Origin header to cross-site POST/DELETE requests. Clients
// that don't send one (curl, scripts) are not a CSRF risk and are let through.
bool IsSameOrigin(const httplib::Request& req)
{
	if (!req.has_header("Origin"))
		return true;

	std::string origin = req.get_header_value("Origin");
	const auto scheme = origin.find("://");
	if (scheme == std::string::npos) // e.g. "null"
		return false;
	origin.erase(0, scheme + 3);

	const std::string host = req.get_header_value("Host");
	return origin.size() == host.size() &&
		std::equal(origin.begin(), origin.end(), host.begin(), [](char a, char b) {
			return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
		});
}
} // namespace

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
	g_logger->info("Starting API server...");
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

	if (!m_svr.listen(m_host.c_str(), m_port))
		g_logger->critical("Failed to start API server!");
}

void WebIo::Router(const httplib::Request &req, httplib::Response &res)
{
	const auto route = req.matches[1].str();

	g_logger->debug("WebIo::Router: {0} -> {1}", req.method, route);

	// Stop a web page the user happens to visit from driving the emulator
	if (req.method != "GET" && !IsSameOrigin(req)) {
		g_logger->warn("WebIo::Router: Rejected cross-origin {0} request for \"{1}\"", req.method, route);
		res.status = 403;
		res.set_content("Cross-origin requests are not allowed", "text/plain");
		return;
	}

	const auto routeIt = m_routeValues.find(route);
	switch (routeIt != m_routeValues.end() ? routeIt->second : Routes::undefined) {
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
			if (req.method == "DELETE")
				m_card->reportDispenserEmpty = true;
			else if (req.method == "POST")
				m_card->reportDispenserEmpty = false;
			res.set_content(m_card->reportDispenserEmpty ? "empty" : "full", "text/plain");
			break;
		case Routes::stop:
			// Shutting down must be an explicit POST, a plain GET (link, <img>, prefetch) must not do it
			if (req.method != "POST") {
				res.status = 405;
				res.set_header("Allow", "POST");
				break;
			}
			m_svr.stop();
			// g_running controls the main loop, if we kill it before http is flushed then we'll crash
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			*g_running = false;
			break;
		default:
			g_logger->warn("WebIo::Router: Unsupported route \"{}\"", req.path);
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
		// Validate first so a rejected request doesn't change any state
		if (req.has_param("cardname") && !CardName::IsValid(req.get_param_value("cardname"))) {
			g_logger->warn("WebIo::InsertedCard: Rejected invalid card name");
			res.status = 400;
			res.set_content("{ \"error\":\"invalid cardname\" }", "application/json");
			return;
		}

		if (req.has_param("loadonly"))
			m_card->insertedCard = true;
		if (req.has_param("cardname"))
			m_card->SetCardName(req.get_param_value("cardname"));
	}

	if (req.has_param("redirect")) {
		res.set_redirect("/");
		return;
	}

	res.set_content(" { \"cardname\":\"" + JsonEscape(m_card->GetCardName()) + "\", \"inserted\":" + (m_card->insertedCard ? "true" : "false") + " }", "application/json");
}

const std::string WebIo::GenerateCardListJSON(std::string basepath)
{
	std::string list = "[";
	const std::string selectedCard = m_card->GetCardName();

	for (const auto& entry : ghc::filesystem::directory_iterator(basepath)) {
		std::string fullCardPath{entry.path().string()};

		if (fullCardPath.find(".png") != std::string::npos)
			continue;

		auto find = fullCardPath.find(".bin");

		if (find != std::string::npos) {
			list.append("{\"name\":\"");

#ifdef _WIN32
			std::string card = fullCardPath.substr(fullCardPath.find_last_of("\\") + 1);
			list.append(JsonEscape(card));
#else
			std::string card = fullCardPath.substr(fullCardPath.find_last_of("/") + 1);
			list.append(JsonEscape(card));
#endif

			list.append("\",\"image\":\"");
			fullCardPath.append(".png");

			std::string base64 = {};
			if (ghc::filesystem::exists(fullCardPath)) {
				std::ifstream img(fullCardPath.c_str(), std::ifstream::in | std::ifstream::binary);

				base64.resize(ghc::filesystem::file_size(fullCardPath));

				img.read(reinterpret_cast<char*>(&base64[0]), base64.size());
				img.close();
			}

			std::string encoded = base64_encode(base64, false);
			list.append("data:image/png;base64, ");
			list.append(encoded);

			// Have the current card be the active item on the carousel
			list.append("\", \"active\":");
			if (card == selectedCard)
				list.append("true},");
			else
				list.append("false},");
		}
	}

	// remove the errant comma
	if (list.compare("[") != 0)
		list.pop_back();

	list.append("]");
	return list;
}
