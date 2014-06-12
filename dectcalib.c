
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



#define NVS "/etc/dect/nvs"
#define TESTMODE_OFFSET 0x80

static void usage_error(const char *progname) {
	
	fprintf(stderr, "Usage: %s\n" \
		"-e (enable) \n " \
		"-d (disable) \n " \
		, progname);
}


int main(int argc, char *argv[]) {
	
	int opt, enable, disable, fd;
	uint8_t test = 0;

	enable = false;
	disable = false;
	
	
	while ((opt = getopt(argc, argv, "ed")) != -1) {
		switch(opt) {
		case 'e':
			enable = true;
			break;
		case 'd':
			disable = true;
			break;
	        default:
			usage_error(argv[0]);
			exit(EXIT_FAILURE);
		}
	}


	if (!enable && !disable) {
		usage_error(argv[0]);
		exit(EXIT_FAILURE);
	}


	fd = open(NVS, O_RDWR);
	if (fd == -1) {
		printf("Error opening %s\n", NVS);
		perror("open");
		exit(EXIT_FAILURE);
	}

	if (lseek(fd, TESTMODE_OFFSET, SEEK_SET) == -1) {
		perror("lseek");
		exit(EXIT_FAILURE);
	}
	
	if (read(fd, &test, 1) == -1) {
		perror("read");
		exit(EXIT_FAILURE);
	}

	if (enable) {
		test |= 1 << 0;

	} else {
		test &= ~(1 << 0);
	}
	

	if (lseek(fd, TESTMODE_OFFSET, SEEK_SET) == -1) {
		perror("lseek");
		exit(EXIT_FAILURE);
	}
	
	if (write(fd, &test, 1) == -1) {
		perror("read");
		exit(EXIT_FAILURE);
	}

	if (enable) {
		printf("Testmode enabled in NVS\n");
	} else {
		printf("Testmode disabled in NVS\n");
	}

	return 0;
}
