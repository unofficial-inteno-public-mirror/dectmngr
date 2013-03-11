#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>


#define ARRAY_SIZE(a) sizeof(a) / sizeof((a)[0])

#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */



typedef struct nvs_entry {
	const char *name;
	int offset;
	int size;
} nvs_entry;


static void print_header(void)
{
	printf("Name\t\t\t\tOffset\tValue\n");
}


static const nvs_entry nvs_entry_table[] = {
	{
		.name = "FixedEmc",
		.offset = 10,
		.size = 2,
	},
	{
		.name = "Rfpi",
		.offset = 0,
		.size = 5,
	},
	{
		.name = "BcmModulationDeviation",
		.offset = 5,
		.size = 1,
	},
};
		

static void usage_error(const char *progname) {

	fprintf(stderr, "Usage: %s\n" \
		"\t-i <infile> \n" \
		"\t-o <outfile> \n" \
		, progname);
}

int main(int argc, char *argv[])
{
	nvs_entry entry;
	int i, j, fp, opt, flags, insize, ret;
	char *infile, *outfile;
	FILE *fp_i, *fp_o;
	unsigned char *buf;
	struct stat st;

	flags = 0;

	while ((opt = getopt(argc, argv, "i:o:")) != -1) {
		switch (opt) {
		case 'i':
			infile = malloc(strlen(optarg) + 1);
			if (!infile) {
				perror("malloc");
				exit(EXIT_FAILURE);
			}
				
			strcpy(infile, optarg);
			break;
		case 'o':
			outfile = malloc(strlen(optarg) + 1);
			if (!outfile) {
				perror("malloc");
				exit(EXIT_FAILURE);
			}
				
			strcpy(outfile, optarg);
			break;
		default:
			usage_error(argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	
	if (!infile || !outfile) {
		usage_error(argv[0]);
		exit(EXIT_FAILURE);
	}
	

	
	/* Read input file into buffer */
	fp_i = fopen(infile, "r+");
	if (!fp_i) {
		fprintf(stderr, "Error opening input file: %s\n", infile);
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	
	ret = stat(infile, &st);
	if (ret == -1) {
		perror("stat");
		exit(EXIT_FAILURE);
	}
	
	buf = malloc(st.st_size);
	if (!buf) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	
	ret = fread(buf, 1, st.st_size, fp_i);
	if (ret != st.st_size) {
		fprintf(stderr, "Error reading input file: %s\n", infile);
		exit(EXIT_FAILURE);
	}
		
	
	/* Open output file */
	fp_o = fopen(outfile, "w+");
	if (!fp_o) {
		fprintf(stderr, "Error opening output file: %s\n", outfile);
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	
	
	print_header();
	
	/* Dump content of nvs */
	for (i = 0; i < ARRAY_SIZE(nvs_entry_table); i++) {
		entry = nvs_entry_table[i];
		printf("%-20s\t\t[%04X]: ", entry.name, entry.offset);
		for (j = 0; j < entry.size; j++)
			printf("%2.2X ", buf[entry.offset + j]);
		printf("\n");
	}

	exit(EXIT_SUCCESS);
}
