#include <stdexcept>	// std::runtime_error

#include <fcntl.h>		// open / ttys flags
#include <termios.h>	// struct termios, tcgetattr, tcsetattr
#include <unistd.h>		// tcgetattr

#include "string.h"	// smart::ssprintf

#include "SerialPort.h"

namespace smart { namespace SerialPort {

static const unsigned int baudrates[] = {
		50, B50,
		75, B75,
		110, B110,
		134, B134,
		150, B150,
		200, B200,
		300, B300,
		600, B600,
		1200, B1200,
		1800, B1800,
		2400, B2400,
		4800, B4800,
		9600, B9600,
		19200, B19200,
		38400, B38400,
		57600, B57600,
		115200, B115200,
		230400, B230400,
		460800, B460800,
		500000, B500000,
		576000, B576000,
		921600, B921600,
		1000000, B1000000,
		1152000, B1152000,
		1500000, B1500000,
		2000000, B2000000,
		2500000, B2500000,
		3000000, B3000000,
		3500000, B3500000,
		4000000, B4000000,
		0, 0
};

int open(const char* devicePath, unsigned int baudrate)
{
	unsigned int	c_baudrate = 0;
	for (int i=0; baudrates[i]!=0; i+=2) {
		if (baudrates[i] == baudrate) {
			c_baudrate = baudrates[i+1];
			break;
		}
	}
	if (c_baudrate == 0) {
		throw std::runtime_error(ssprintf("Baudrate %u unknown when opening %s", baudrate, devicePath));
	}
	int _fd = ::open(devicePath, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (_fd < 0) {
		throw std::runtime_error(ssprintf("Cannot open serial port: %s", devicePath));
	}
	struct termios	_oldTermios;
	tcgetattr(_fd, &_oldTermios);	// Get current serial port settings

	struct termios new_termios = { 0 };

	// Control flags for the serial port
	new_termios.c_cflag = c_baudrate | CS8 | CLOCAL | CREAD;
	new_termios.c_iflag = IGNPAR;
	new_termios.c_oflag = 0;
	new_termios.c_lflag = 0;

	// Mask control-characters, to prevent them from interrupt the communication.
	// Then this is not a terminal-program.
	//new_termios.c_cc[VMIN]    = 1;	Blocking read until 1 character arrives
	new_termios.c_cc[VEOL] 		= 0;     // '\0'
	new_termios.c_cc[VEOL2] 	= 0;     // '\0'
	new_termios.c_cc[VSWTC] 	= 0;     // '\0'
	new_termios.c_cc[VKILL] 	= 0;     // @
	new_termios.c_cc[VTIME] 	= 0;     // inter-character timer unused
	new_termios.c_cc[VERASE] 	= 0;     // del
	new_termios.c_cc[VEOF] 		= 0;     // Ctrl-d
	new_termios.c_cc[VINTR] 	= 0;     // Ctrl-c
	new_termios.c_cc[VQUIT] 	= 0; 	 /* Ctrl-\ */
	new_termios.c_cc[VSTART]	= 0;     // Ctrl-q
	new_termios.c_cc[VSTOP] 	= 0;     // Ctrl-s
	new_termios.c_cc[VSUSP] 	= 0;     // Ctrl-z
	new_termios.c_cc[VREPRINT] 	= 0;     // Ctrl-r
	new_termios.c_cc[VDISCARD] 	= 0;     // Ctrl-u
	new_termios.c_cc[VWERASE] 	= 0;     // Ctrl-w
	new_termios.c_cc[VLNEXT] 	= 0;     // Ctrl-v

	tcflush(_fd, TCIOFLUSH);				// Clear the port
	tcsetattr(_fd, TCSANOW, &new_termios);	// Set new port settings

	return _fd;
}

} } // namespace smart::SerialPort
