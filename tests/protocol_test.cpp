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

// Feeds the card reader protocol (the framing in CardIo::ReceivePacket/BuildPacket) with good,
// damaged and random data and checks that it answers sensibly, keeps making progress and never
// reads out of range. Run it with the sanitizers to get the most out of the fuzzing part.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <random>
#include <vector>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include "spdlog/async.h"
#include "spdlog/sinks/null_sink.h"
#include "spdlog/spdlog.h"

#include "C1231BR.h"
#include "C1231LR.h"
#include "test.h"

std::shared_ptr<spdlog::async_logger> g_logger;

namespace {
// Where the test is, so a crash can be placed. Written to stderr unbuffered, see main()
const char* g_stage = "startup";
int g_run = -1;
int g_iteration = -1;

constexpr uint8_t STX = 0x02, ETX = 0x03, ENQ = 0x05, ACK = 0x06, NAK = 0x15;
using Bytes = std::vector<uint8_t>;

// A frame as the game sends it: STX, count, command + 3 status bytes + parameters, ETX, checksum
Bytes Frame(uint8_t command, const Bytes& params = {})
{
	Bytes payload{ command, 0x30, 0x30, 0x30 };
	payload.insert(payload.end(), params.begin(), params.end());

	const uint8_t count = static_cast<uint8_t>(payload.size() + 2);
	Bytes frame{ STX, count };
	frame.insert(frame.end(), payload.begin(), payload.end());
	frame.push_back(ETX);

	uint8_t sum = count;
	for (const uint8_t b : payload)
		sum ^= b;
	sum ^= ETX;
	frame.push_back(sum);
	return frame;
}

// Splits what the emulator wrote back into its payload, empty if it isn't a valid frame
Bytes ParseReply(const Bytes& reply)
{
	if (reply.size() < 4 || reply[0] != STX)
		return {};
	const uint8_t count = reply[1];
	if (reply.size() != static_cast<size_t>(count) + 2 || reply[count] != ETX)
		return {};

	uint8_t sum = count;
	for (size_t i = 2; i <= count; i++)
		sum ^= reply[i];
	if (reply.back() != sum)
		return {};

	return Bytes(reply.begin() + 2, reply.begin() + count);
}

// Like the main loop: keep calling Process while there is data. It has to eat something on every
// call that doesn't just wait for more bytes, or the loop would spin forever on bad input.
void Pump(CardIo& io, Bytes& read, Bytes& write)
{
	for (int calls = 0; !read.empty(); calls++) {
		const size_t before = read.size();
		const auto status = io.Process(read, write);

		if (status == CardIo::SizeError || status == CardIo::SyntaxError)
			return; // waiting for the rest of the frame

		CHECK(read.size() < before);
		if (read.size() >= before || calls > 100000)
			return;
	}
}

struct Fixture {
	CardIo::Settings settings;
	std::unique_ptr<CardIo> io;

	explicit Fixture(bool bothMechs = false)
	{
		settings.cardPath = "./";
		settings.configPath = "protocol_test_config.ini";
		if (bothMechs)
			io = std::make_unique<C1231BR>(&settings);
		else
			io = std::make_unique<C1231LR>(&settings);
	}
};

void TestFraming()
{
	{ // Initialize is acknowledged and then answered when the game asks (ENQ)
		Fixture f;
		Bytes read = Frame(0x10), write;
		CHECK_EQ(f.io->Process(read, write), CardIo::Okay);
		CHECK(write == Bytes{ ACK });
		CHECK(read.empty());

		write.clear();
		read = { ENQ };
		CHECK_EQ(f.io->Process(read, write), CardIo::ServerWaitingReply);
		const Bytes reply = ParseReply(write);
		CHECK(reply.size() >= 4);
		CHECK_EQ(reply.empty() ? 0 : reply[0], 0x10); // echoes the command
	}
	{ // An ENQ before any command must still produce a well formed reply
		Fixture f;
		Bytes read{ ENQ }, write;
		CHECK_EQ(f.io->Process(read, write), CardIo::ServerWaitingReply);
		CHECK(!ParseReply(write).empty());
	}
	{ // Get version returns the version string
		Fixture f;
		Bytes read = Frame(0xF0), write;
		f.io->Process(read, write);
		write.clear();
		for (int i = 0; i < 4; i++) {
			read = { ENQ };
			f.io->Process(read, write);
		}
		const std::string all(write.begin(), write.end());
		CHECK(all.find("AP:S1234-5678") != std::string::npos);
	}
	{ // Bad checksum: NAK and the frame is consumed
		Fixture f;
		Bytes read = Frame(0x10), write;
		read.back() ^= 0xFF;
		CHECK_EQ(f.io->Process(read, write), CardIo::ChecksumError);
		CHECK(write == Bytes{ NAK });
		CHECK(read.empty());
	}
	{ // Missing ETX: NAK and it moves on
		Fixture f;
		Bytes read = Frame(0x10), write;
		read[read.size() - 2] = 0x00;
		Pump(*f.io, read, write);
		CHECK(read.empty());
		CHECK(!write.empty() && write[0] == NAK);
	}
	{ // Garbage in front of a good frame is skipped, the command still runs
		Fixture f;
		Bytes read{ 0xAA, 0xBB, 0x00 }, write;
		const Bytes good = Frame(0x10);
		read.insert(read.end(), good.begin(), good.end());
		Pump(*f.io, read, write);
		CHECK(read.empty());
		CHECK(!write.empty() && write.back() == ACK);
	}
	{ // A frame that arrives in pieces is only handled once it is complete
		Fixture f;
		const Bytes good = Frame(0x20);
		Bytes read(good.begin(), good.begin() + 5), write;
		Pump(*f.io, read, write);
		CHECK_EQ(read.size(), static_cast<size_t>(5));
		CHECK(write.empty());
		read.insert(read.end(), good.begin() + 5, good.end());
		Pump(*f.io, read, write);
		CHECK(read.empty());
		CHECK(write == Bytes{ ACK });
	}
}

// Frames that are well formed enough to get past the first checks but are too short to be a
// command. These used to loop forever or index past the end of the packet.
void TestMalformedFrames()
{
	for (int count = 0; count < 8; count++) {
		for (int trailing = 0; trailing < 3; trailing++) {
			Fixture f;
			// STX, count, then filler, with an ETX where the count says it should be and a matching sum
			Bytes read{ STX, static_cast<uint8_t>(count) };
			while (read.size() < 10)
				read.push_back(0x31);
			if (static_cast<size_t>(count) < read.size())
				read[count] = ETX;
			uint8_t sum = static_cast<uint8_t>(count);
			for (size_t i = 2; i < read.size() && i < static_cast<size_t>(count) + 2; i++)
				sum ^= read[i];
			if (static_cast<size_t>(count) + 1 < read.size() && trailing == 1)
				read[count + 1] = sum;

			Bytes write;
			Pump(*f.io, read, write);
			CHECK(read.size() < 10 + 1); // finished (or is waiting), not spinning
		}
	}
	{ // count 0 on its own must not wedge the parser
		Fixture f;
		Bytes read{ STX, 0x00, 0, 0, 0, 0, 0, 0, 0, 0 }, write;
		Pump(*f.io, read, write);
		CHECK(read.size() < 10);
	}
	{ // a correct checksum on a payload that has no room for the master's status bytes
		Fixture f;
		Bytes read{ STX, 0x03, 0x10, ETX, static_cast<uint8_t>(0x03 ^ 0x10 ^ ETX), 0, 0, 0 }, write;
		Pump(*f.io, read, write);
		CHECK(read.size() < 8);
	}
	{ // an empty buffer is not a frame
		Fixture f;
		Bytes read, write;
		f.io->Process(read, write);
		CHECK(write.empty());
	}
}

// Random bytes, random frames (valid framing and checksum, random command and parameters) and
// lots of ENQs to drive multi step commands. Nothing may crash and the loop must always progress.
void TestFuzz()
{
	std::mt19937 rng(0xC0FFEE);
	auto byte = [&]() { return static_cast<uint8_t>(rng() & 0xFF); };
	auto pick = [&](std::initializer_list<uint8_t> values) { return *(values.begin() + rng() % values.size()); };

	for (int run = 0; run < 2; run++) {
		Fixture f(run == 1);
		Bytes read, write;

		for (int i = 0; i < 4000; i++) {
			g_run = run;
			g_iteration = i;
			if (i % 500 == 0)
				std::fprintf(stderr, "protocol_test: fuzz run %d iteration %d\n", run, i);
			f.settings.insertedCard = (rng() % 4) == 0;
			f.settings.hasCard = (rng() % 4) == 0;

			switch (rng() % 3) {
				case 0: { // raw noise, biased towards the framing bytes
					const size_t n = rng() % 40;
					for (size_t k = 0; k < n; k++)
						read.push_back(rng() % 3 == 0 ? pick({ STX, ETX, ENQ, 0x00, 0xFF }) : byte());
					break;
				}
				case 1: { // well formed frame, random command and parameters
					Bytes params(rng() % 90);
					for (auto& b : params)
						b = rng() % 2 ? byte() : pick({ 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36 });
					const uint8_t command = rng() % 2 ? byte() : pick({ 0x10, 0x20, 0x33, 0x35, 0x40, 0x53, 0x78, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x80, 0xA0, 0xB0, 0xC0, 0xC1, 0xD0, 0xE1, 0xF0, 0xF1, 0xF5 });
					const Bytes frame = Frame(command, params);
					read.insert(read.end(), frame.begin(), frame.end());
					break;
				}
				default: { // a frame with one corrupted byte
					Bytes frame = Frame(byte(), Bytes(rng() % 20, byte()));
					frame[rng() % frame.size()] = byte();
					read.insert(read.end(), frame.begin(), frame.end());
					break;
				}
			}

			for (size_t k = rng() % 12; k > 0; k--)
				read.push_back(ENQ);

			Pump(*f.io, read, write);
			if (read.size() > 4096)
				read.clear(); // a "frame" that waits for more bytes, don't let it grow without end
			write.clear();
		}
	}
}
} // namespace

int main()
{
	// A fast fail (exit code 0xc0000409 on Windows) throws away buffered output, so make sure
	// progress reaches the log and say what was going on if the C++ runtime terminates us.
#ifdef _MSC_VER
	// The debug CRT answers a failed assert / invalid parameter / abort with a message box, which
	// nobody can click on a CI machine and hangs the test forever. Print to stderr and die instead.
	_set_abort_behavior(_WRITE_ABORT_MSG, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	for (const int type : { _CRT_WARN, _CRT_ERROR, _CRT_ASSERT }) {
		_CrtSetReportMode(type, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(type, _CRTDBG_FILE_STDERR);
	}
#endif

	std::setvbuf(stdout, nullptr, _IONBF, 0);
	std::setvbuf(stderr, nullptr, _IONBF, 0);
	std::set_terminate([]() {
		std::fprintf(stderr, "protocol_test: std::terminate in stage \"%s\" (fuzz run %d, iteration %d)\n", g_stage, g_run, g_iteration);
		try {
			const std::exception_ptr current = std::current_exception();
			if (current)
				std::rethrow_exception(current);
		} catch (const std::exception& e) {
			std::fprintf(stderr, "protocol_test: uncaught exception: %s\n", e.what());
		} catch (...) {
			std::fprintf(stderr, "protocol_test: uncaught exception of unknown type\n");
		}
		std::abort();
	});

	spdlog::init_thread_pool(8192, 1);
	std::vector<spdlog::sink_ptr> sinks{ std::make_shared<spdlog::sinks::null_sink_mt>() };
	g_logger = std::make_shared<spdlog::async_logger>("test", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
	g_logger->set_level(spdlog::level::off);

	g_stage = "TestFraming";
	std::fprintf(stderr, "protocol_test: %s\n", g_stage);
	TestFraming();
	g_stage = "TestMalformedFrames";
	std::fprintf(stderr, "protocol_test: %s\n", g_stage);
	TestMalformedFrames();
	g_stage = "TestFuzz";
	std::fprintf(stderr, "protocol_test: %s\n", g_stage);
	TestFuzz();
	g_stage = "finished";
	std::fprintf(stderr, "protocol_test: %s\n", g_stage);
	return test::Finish("protocol_test");
}
