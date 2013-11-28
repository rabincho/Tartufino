#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>

struct termio original_termio;

void enterInputMode()
{
	/* Settings for stdin (source: svgalib) */
	int fd = fileno(stdin);
	struct termio zap;
	ioctl(fd, TCGETA, &original_termio);
	zap = original_termio;
	zap.c_cc[VMIN] = 0;
	zap.c_cc[VTIME] = 0;
	zap.c_lflag = 0;
	ioctl(fd, TCSETA, &zap);
	
	printf("Telecommand:   Enabled\n");
}

void leaveInputMode()
{
	/* Restore original stdin */
	int fd = fileno(stdin);
	ioctl(fd, TCSETA, &original_termio);
	
	printf("Telecommand:   Disabled\n");
}

char readKey()
{
	int fd = fileno(stdin);

	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	select(fd + 1, &rfds, NULL, NULL, NULL);
	
	char c = '\0';
	read(fd, &c, 1);
	//while (c == '\0') {
	//	int e = read(fd, &c, 1);
	//	if (e == 0)
	//		c = '\0';
	//}
	
	return c;
}
