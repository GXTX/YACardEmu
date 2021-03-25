#ifndef SERIO_H
#define SERIO_H

#include <iostream>
#include <vector>

#include <libserialport.h>

class SerIo
{
public:
	enum StatusCode {
		Okay,
		Timeout,
		ReadError,
		WriteError,
		ZeroSizeError,
	};

	bool IsInitialized;

	SerIo(const std::string devicePath);
	~SerIo();

	SerIo::StatusCode Read(std::vector<uint8_t> *buffer);
	SerIo::StatusCode Write(std::vector<uint8_t> *buffer);
private:
	sp_port *Port;
	sp_port_config *PortConfig;
};

#endif
