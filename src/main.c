#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>

static char *program_name;

/* verbose */
static bool verbose = false;
/* default serial device */
static char *serial_device = "/dev/ttyUSB0";
/* output dump file */
static char *output_dump_file = "./ram_dump.bin";
/* output IRQ/FIQ vectors dump file */
static char *output_vectors_dump_file = "./vector_dump.bin";
/* microsecond delay between block transfer */
static long blk_delay = 0;
/* default 115200 bps */
static int baud = 0xab;

/* baudrate table */
static const int baudrate_table[4] = { B115200, B230400, B460800, B921600 };

static const struct option longopts[] = {
	{"help",      0, NULL, 'h'},
	{"device",    1, NULL, 'd'},
	{"delay",     1, NULL, 'D'},
	{"baud-rate", 1, NULL, 'b'},
	{"verbose",   0, NULL, 'v'},
	{NULL, 0, NULL, 0}
};

/* print help to the terminal */
static void show_help(int err) {
	fprintf(err == 1 ? stderr : stdout,
			"Usage: %s [options] <output dump file> ...\n" \
			"\n" \
			"Default output dump file: ./dump.bin\n" \
			"\n" \
			"Available options:\n" \
			"  -h, --help                   - print help and exit.\n" \
			"  -d, --device=<file>          - serial device to operate on.\n" \
			"  -D, --delay=<ms>             - microsecond delay between each block transfer.\n" \
			"  -b, --baud-rate=<0,1,2,3>    - 0 = 115200 bps;\n" \
			"                                 1 = 230400 bps;\n" \
			"                                 2 = 460800 bps;\n" \
			"                                 3 = 921600 bps.\n" \
			"  -v, --verbose                - set verbose flag.\n",
			program_name);
}

/* ram dump function */
int ram_dump(FILE *dump_fd, FILE *vectors_dump_fd) {
	uint8_t b;

	uint32_t read_address;
	uint32_t read_length;

	/* open serial port */
	printf("Opening serial device %s...\n", serial_device);
	int serial_fd = open(serial_device, O_RDWR | O_NOCTTY);
	if(serial_fd < 0) {
		perror(serial_device);
		return 2;
	}
	struct termios tty;
	tcgetattr(serial_fd, &tty);
	cfsetispeed(&tty, B115200);
	cfsetospeed(&tty, B115200);
	tty.c_lflag &= ~(ISIG | ICANON | XCASE | IEXTEN | ECHO | ECHOK | ECHOKE | ECHOCTL);
	tty.c_iflag &= ~(IGNBRK | BRKINT | IGNPAR | PARMRK | INPCK | ISTRIP | INLCR | IGNCR | ICRNL | IUCLC | IXON | IXANY | IXOFF | IMAXBEL);
	tty.c_oflag &= ~(OPOST);
	tty.c_cc[CMIN] = 1;
	tty.c_cc[CTIME] = 0;
	tcsetattr(serial_fd, TCSANOW, &tty);

	ioctl(serial_fd, TCFLSH, TCIFLUSH);
	
	printf("Changing baudrates...\n");
	/* set baudrates */
	write(serial_fd, &baud, 1);
	read(serial_fd, &b, 1);

	int checksum = 0;

	if(b != 0x11) {
		fprintf(stderr, "Wrong response: 0x%02x\n", b);
		return 1;
	}

	cfsetispeed(&tty, baudrate_table[baud - 0xab]);
	cfsetospeed(&tty, baudrate_table[baud - 0xab]);
	tcsetattr(serial_fd, TCSANOW, &tty);
	/* OK */
	b = 0xab;
	write(serial_fd, &b, 1);
	read(serial_fd, &b, 1);

	int error_code;
	read(serial_fd, &error_code, 4);
	checksum = error_code + (error_code >> 8) + (error_code >> 16) + (error_code >> 24);

	/* version name */
	char vername[15];
	read(serial_fd, &vername, 15);
	for(int i = 0; i < 15; i++) {
		checksum += vername[i];
	}

	write(serial_fd, &checksum, 1);

	/* read check status */
	read(serial_fd, &b, 1);
	switch(b) {
		case 'w':
			printf("Version name: %s\n", vername);
			printf("Error code: 0x%08X\n", error_code);
			break;

		case 'D':
			close(serial_fd);
			return 'D';

		default:
			fprintf(stderr, "Wrong check status response (version name): 0x%02X\n", b);
			close(serial_fd);
			return 1;
	}

	/* start address */
	read(serial_fd, &read_address, 4);
	checksum = read_address + (read_address >> 8) + (read_address >> 16) + (read_address >> 24);
	write(serial_fd, &checksum, 1);

	/* read check status */
	read(serial_fd, &b, 1);
	switch(b) {
		case 'w':
			printf("Read address: 0x%08X\n", read_address);
			break;

		case 'D':
			close(serial_fd);
			return 'D';

		default:
			fprintf(stderr, "Wrong check status response (read address): 0x%02X\n", b);	
			close(serial_fd);
			return 1;
	}

	/* read length */
	read(serial_fd, &read_length, 4);
	checksum = read_length + (read_length >> 8) + (read_length >> 16) + (read_length >> 24);
	write(serial_fd, &checksum, 1);

	/* read check status */
	read(serial_fd, &b, 1);
	switch(b) {
		case 'w':
			printf("Read length: 0x%08X\n", read_length);
			break;

		case 'D':
			close(serial_fd);
			return 'D';

		default:
			fprintf(stderr, "Wrong check status response (read length): 0x%02X\n", b);
			close(serial_fd);
			return 1;
	}

	uint8_t data[5];
	int n_read;
	int total_read;

	/* dumping RAM */
	printf("\n\033[0;36m-->\033[0m Dumping RAM...\n\n");

	total_read = 0;
	do {
		if(verbose) printf("\033[0;36m-->\033[0m 0x%08X ", read_address + total_read);
		checksum = 0;
		
		uint8_t data[5];
		for(int i = 0; i < 0x1f40; i += 5) {
			n_read = 0;
			do {
				n_read += read(serial_fd, (uint8_t *)&data + n_read, 5 - n_read);
			} while(n_read < 5);

			checksum += *data + data[1] + data[2] + data[3] + data[4];

			if(!data[4]) {
				do {
					fwrite(data, 1, 4, dump_fd);
				} while((total_read += 4) < read_length);
				break;
			} else {
				for(int n = 0; n < data[4]; n++) {
					fwrite(data, 1, 4, dump_fd);
					total_read += 4;
				}
			}
		}

		write(serial_fd, &checksum, 1);

		/* read check status */
		read(serial_fd, &b, 1);
		switch(b) {
			case 'w':
				if(verbose) printf("OK (%d%%)\n", (int)((float)total_read * 100.0f / (float)read_length));
				break;
	
			case 'D':
				close(serial_fd);
				return 'D';
	
			default:
				fprintf(stderr, "Wrong check status response (RAM dumping): 0x%02X\n", b);
				close(serial_fd);
				return 1;
		}

		if(blk_delay > 0) usleep(blk_delay);

	} while(total_read < read_length);

	/* dumping IRQ/FIQ vectors */
	printf("\n\033[0;36m-->\033[0m Dumping IRQ/FIQ vectors...\n\n");

	total_read = 0;
	read_length = 0x2000;
	do {
		if(verbose) printf("\033[0;36m-->\033[0m 0x%08X ", total_read);

		checksum = 0;
		
		for(int i = 0; i < 0x1f40; i += 5) {
			n_read = 0;
			do {
				n_read += read(serial_fd, (uint8_t *)&data + n_read, 5 - n_read);
			} while(n_read < 5);

			checksum += *data + data[1] + data[2] + data[3] + data[4];

			if(!data[4]) {
				do {
					fwrite(data, 1, 4, vectors_dump_fd);
				} while((total_read += 4) < read_length);
				break;
			} else {
				for(int n = 0; n < data[4]; n++) {
					fwrite(data, 1, 4, vectors_dump_fd);
					total_read += 4;
				}
			}
		}

		write(serial_fd, &checksum, 1);

		/* read check status */
		read(serial_fd, &b, 1);
		switch(b) {
			case 'w':
				if(verbose) printf("OK (%d%%)\n", (int)((float)total_read * 100.0f / (float)read_length));
				break;
	
			case 'D':
				close(serial_fd);
				return 'D';
	
			default:
				fprintf(stderr, "Wrong check status response (IRQ/FIQ vectors dumping): 0x%02X\n", b);
				close(serial_fd);
				return 1;
		}

		if(blk_delay > 0) usleep(blk_delay);

	} while(total_read < read_length);

	/* final */
	n_read = 0;
	do {
		n_read += read(serial_fd, (uint8_t *)&data + n_read, 5 - n_read);
	} while(n_read < 5);
	checksum = *data + data[1] + data[2] + data[3] + data[4];
	write(serial_fd, &checksum, 1);

	read(serial_fd, &b, 1);
	switch(b) {
		case 'w':
			printf("\nDone. Phone will reboot now.\n");
			break;
	
		case 'D':
			close(serial_fd);
			return 'D';
	
		default:
			fprintf(stderr, "Wrong check status response (final): 0x%02X\n", b);
			close(serial_fd);
			return 1;
	}

	close(serial_fd);

	return 0;
}

/* main function */
int main(int argc, char *argv[]) {
	if(argv[0] == NULL)
		program_name = "spush";
	else
		program_name = argv[0];

	int c;
	while((c = getopt_long(argc, argv, "hd:D:b:v", longopts, NULL)) != -1) {
		
		switch(c) {
			/* --help */
			case 'h':
				show_help(0);
				return 0;

			/* --device */
			case 'd':
				serial_device = strdup(optarg);
				break;

			/* --delay */
			case 'D':
				blk_delay = atol(optarg);
				break;

			/* --baud-rate */
			case 'b':
				int baud_i = atoi(optarg);
				if(baud_i & ~3) {
					fprintf(stderr, "Incorrect baud selection: %d\n", baud_i);
					return 1;
				}
				baud = 0xab + baud_i;
				break;

			/* --verbose */
			case 'v':
				verbose = true;
				break;

			default:
				show_help(1);
				return 1;
		}

	}

	argv += optind;
	argc -= optind;

	if(argc > 0) output_dump_file = argv[0];
	if(argc > 1) output_vectors_dump_file = argv[1];

	FILE *dump_fd = fopen(output_dump_file, "wb");
	if(dump_fd == NULL) {
		perror(output_dump_file);
		return 1;
	}

	FILE *vectors_dump_fd = fopen(output_vectors_dump_file, "wb");
	if(vectors_dump_fd == NULL) {
		perror(output_vectors_dump_file);
		return 1;
	}

	int status = 0;
	switch(ram_dump(dump_fd, vectors_dump_fd)) {
		case 0:
			status = 0;
			break;

		case 1:
			status = 1;
			break;

		case 2:
			status = errno;
			break;

		case 'D':
			fprintf(stderr, "Wrong checksum!\n");
			status = 1;
			break;
	}

	fclose(dump_fd);
	fclose(vectors_dump_fd);

	return status;
}
