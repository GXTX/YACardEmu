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

#ifndef WEBIO_H
#define WEBIO_H

#include <map>
#include <string>
#include <iostream>
#include <utility>
#include <thread>
#include <atomic>
#include <thread>

#include "CardIo.h"
#include "base64.h"
#include "httplib.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/bin_to_hex.h"

class WebIo
{
public:
	WebIo(CardIo::Settings *card, int port, std::atomic<bool> *running);
	~WebIo();

	void StartServer();
	bool Spawn();

	int m_port = 0;
	CardIo::Settings *m_card = nullptr;
	std::atomic<bool> *g_running = nullptr;
private:
	httplib::Server m_svr{};

	void Router(const httplib::Request &req, httplib::Response &res);
	const std::string GenerateCardListJSON(std::string basepath);

	enum class Routes
	{
		undefined, // All invalid routes will point here
		cards,
		hasCard,
		readyCard,
		insertedCard,
		dispenser,
		stop,
	};

	std::map<std::string, Routes> m_routeValues{};

	void SetupRoutes()
	{
		m_routeValues["cards"]        = Routes::cards;
		m_routeValues["hasCard"]      = Routes::hasCard;
		m_routeValues["readyCard"]    = Routes::readyCard;
		m_routeValues["insertedCard"] = Routes::insertedCard;
		m_routeValues["dispenser"]    = Routes::dispenser;
		m_routeValues["stop"]         = Routes::stop;
	}

	void InsertedCard(const httplib::Request& req, httplib::Response& res);
};

#endif //WEBIO_H
