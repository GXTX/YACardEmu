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

	SerIo(char *devicePath);
	~SerIo();

	int Read(std::vector<uint8_t> &buffer);
	int Write(std::vector<uint8_t> &buffer);
private:
	int SerialHandler;

	struct sp_port *Port;
	struct sp_port_config *PortConfig;
};

#endif
