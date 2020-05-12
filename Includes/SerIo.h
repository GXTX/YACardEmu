#ifndef SERIO_H
#define SERIO_H

#include <iostream>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/serial.h>

class SerIo
{
public:
	enum StatusCode {
		StatusOkay,
		SerialTimeout,
		SerialReadError,
	};

	bool IsInitialized;

	SerIo(char *devicePath);
	~SerIo();

	int Read(uint8_t *buffer);
	int Write(uint8_t *write_buffer, uint8_t bytes_to_write);
private:
	int SerialHandler;

	void SetAttributes(int SerialHandler, int baud);
	int SetLowLatency(int SerialHandler);
};

#endif
