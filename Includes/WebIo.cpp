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
#include "AppPaths.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <system_error>
#include <vector>

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

	// The web UI is the "public" folder, in the current directory or next to the executable
	const auto publicDir = AppPaths::Find("public");
	if (publicDir.empty() || !m_svr.set_mount_point("/", publicDir.string()))
		g_logger->warn("WebIo::StartServer: Unable to find the \"public\" folder, the web page will not load (the API still works)");

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
		case Routes::cardImage:
			CardImage(req, res);
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
	const std::string selectedCard = m_card->GetCardName();

	// Only list files that can actually be inserted, the card folder may also hold
	// the generated .png previews and anything else the user keeps there
	std::vector<std::string> names;
	std::error_code ec;
	ghc::filesystem::directory_iterator it(basepath, ec);
	if (ec) {
		g_logger->warn("WebIo::GenerateCardListJSON: Unable to read card folder \"{0}\": {1}", basepath, ec.message());
		return "[]";
	}

	for (const ghc::filesystem::directory_iterator end; it != end; it.increment(ec)) {
		if (ec)
			break;

		if (!it->is_regular_file(ec))
			continue;

		const std::string name = it->path().filename().string();
		if (CardName::IsValid(name))
			names.push_back(name);
	}

	std::sort(names.begin(), names.end());

	// The images are not embedded, a card folder with a lot of cards would make this a
	// huge response. The page loads each one on its own from /api/v1/cardImage
	std::string list = "[";
	for (const auto& name : names) {
		std::error_code pngEc;
		const bool hasImage = ghc::filesystem::exists(ghc::filesystem::path(basepath) / (name + ".png"), pngEc);

		if (list.size() > 1)
			list.append(",");

		list.append("{\"name\":\"" + JsonEscape(name) + "\",\"image\":\"");
		if (hasImage)
			list.append("/api/v1/cardImage?name=" + UrlEncode(name));
		// Have the current card be the active item on the carousel
		list.append("\",\"active\":");
		list.append(name == selectedCard ? "true}" : "false}");
	}

	list.append("]");
	return list;
}

void WebIo::CardImage(const httplib::Request& req, httplib::Response& res)
{
	if (req.method != "GET") {
		res.status = 405;
		res.set_header("Allow", "GET");
		return;
	}

	const std::string name = req.has_param("name") ? req.get_param_value("name") : std::string();
	if (!CardName::IsValid(name)) {
		res.status = 400;
		return;
	}

	std::ifstream img(m_card->cardPath + name + ".png", std::ifstream::in | std::ifstream::binary);
	if (!img) {
		res.status = 404;
		return;
	}

	const std::string data((std::istreambuf_iterator<char>(img)), std::istreambuf_iterator<char>());

	// The preview is rewritten whenever the game prints on the card
	res.set_header("Cache-Control", "no-cache");
	res.set_content(data, "image/png");
}
