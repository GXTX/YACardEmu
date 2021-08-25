#ifndef SERIO_H
#define SERIO_H

#include <iostream>
#include <vector>

#include <libserialport.h>

class SerIo
{
public:
	enum class Status {
		Okay,
		Timeout,
		ReadError,
		WriteError,
		ZeroSizeError,
	};

	bool IsInitialized{};

	SerIo(const char *devicePath);
	~SerIo();

	SerIo::Status Read(std::vector<uint8_t> &buffer);
	SerIo::Status Write(std::vector<uint8_t> &buffer);
private:
	sp_port *Port{nullptr};
	sp_port_config *PortConfig{nullptr};
};

#endif
