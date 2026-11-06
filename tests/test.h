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

#ifndef TESTS_TEST_H
#define TESTS_TEST_H

// A tiny test helper so the tests don't need any extra dependency

#include <cstdio>
#include <string>

namespace test {
inline int& Failures()
{
	static int failures = 0;
	return failures;
}

inline int& Checks()
{
	static int checks = 0;
	return checks;
}

inline int Finish(const char* suite)
{
	std::printf("%s: %d checks, %d failed\n", suite, Checks(), Failures());
	return Failures() == 0 ? 0 : 1;
}
} // namespace test

#define CHECK(cond) \
	do { \
		test::Checks()++; \
		if (!(cond)) { \
			test::Failures()++; \
			std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		} \
	} while (0)

#define CHECK_EQ(actual, expected) \
	do { \
		test::Checks()++; \
		const auto a_ = (actual); \
		const auto e_ = (expected); \
		if (!(a_ == e_)) { \
			test::Failures()++; \
			std::printf("FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #actual, #expected); \
		} \
	} while (0)

#endif // TESTS_TEST_H
