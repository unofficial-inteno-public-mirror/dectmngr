#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>


#define ARRAY_SIZE(a) sizeof(a) / sizeof((a)[0])


typedef struct nvs_entry {
	const char *name;
	int offset;
	int size;
} nvs_entry;


static void print_header(void)
{
	printf("%-25s\t\tOffset\tValue\n", "Name");
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
	{
		.name = "BcmDefaultFrequency",
		.offset = 6,
		.size = 2,
	},
	{
		.name = "DiversityMode",
		.offset = 32,
		.size = 1,
	},
	{
		.name = "DualSlotReleaseHysteresis",
		.offset = 33,
		.size = 1,
	},
	{
		.name = "DualSlotReleaseOnDifference",
		.offset = 34,
		.size = 1,
	},
	{
		.name = "PowerLowRssiLimit",
		.offset = 35,
		.size = 1,
	},
	{
		.name = "PowerHighRssiLimit",
		.offset = 36,
		.size = 1,
	},
	{
		.name = "PowerCrcLimit",
		.offset = 37,
		.size = 1,
	},
	{
		.name = "PowerCrcHighCnt",
		.offset = 41,
		.size = 1,
	},
	{
		.name = "bFreqBandOffset",
		.offset = 42,
		.size = 1,
	},
	{
		.name = "bRssiLevel_UpperThreshold",
		.offset = 43,
		.size = 1,
	},
	{
		.name = "bRssiScanType",
		.offset = 44,
		.size = 1,
	},
	{
		.name = "AdaptivePowerCtrl",
		.offset = 57,
		.size = 1,
	},
	{
		.name = "EepromUsedRfCarriers",
		.offset = 274,
		.size = 2,
	},
	{
		.name = "UsedRfCarriers5g8",
		.offset = 294,
		.size = 6,
	},
	{
		.name = "RepeaterControl",
		.offset = 276,
		.size = 1,
	},
	{
		.name = "Default_BCM_CTRL_REG",
		.offset = 292,
		.size = 1,
	},
	{
		.name = "Default_BCM_CTRL_REG",
		.offset = 293,
		.size = 1,
	},
	{
		.name = "BcmConfigErr",
		.offset = 277,
		.size = 1,
	},
	{
		.name = "BcmConfigSlideMask",
		.offset = 278,
		.size = 1,
	},
	{
		.name = "BcmConfigVol",
		.offset = 279,
		.size = 1,
	},
	{
		.name = "BcmConfigDac",
		.offset = 280,
		.size = 1,
	},
	{
		.name = "BcmConfigWin",
		.offset = 281,
		.size = 1,
	},
	{
		.name = "BcmConfigEnc",
		.offset = 282,
		.size = 1,
	},
	{
		.name = "BandGap",
		.offset = 129,
		.size = 1,
	},
	{
		.name = "AdvanceTimingEnable",
		.offset = 273,
		.size = 1,
	},
	{
		.name = "bProtocolFeatures",
		.offset = 58,
		.size = 1,
	},
	{
		.name = "bNoEmission_PreferredCarrier",
		.offset = 60,
		.size = 1,
	},
	{
		.name = "ExtendedRfCarriers",
		.offset = 648,
		.size = 3,
	},
	{
		.name = "bRssiLevel_LowerThreshold",
		.offset = 97,
		.size = 1,
	},
	{
		.name = "ExtendedFpCap2",
		.offset = 98,
		.size = 1,
	},
	{
		.name = "RfBandgap",
		.offset = 307,
		.size = 1,
	},
	{
		.name = "RfPower4x1[0]",
		.offset = 61,
		.size = 6,
	},
	{
		.name = "RfPower4x1[1]",
		.offset = 67,
		.size = 6,
	},
	{
		.name = "RfPower4x1[2]",
		.offset = 73,
		.size = 6,
	},
	{
		.name = "RfPower4x1[3]",
		.offset = 79,
		.size = 6,
	},
	{
		.name = "RfPower4x1[4]",
		.offset = 85,
		.size = 6,
	},
	{
		.name = "RfPower4x1[5]",
		.offset = 91,
		.size = 6,
	},
	{
		.name = "RfLMX4x68Length",
		.offset = 8,
		.size = 1,
	},
	{
		.name = "RfLMX4x68Data_patch[0]",
		.offset = 176,
		.size = 4,
	},
	{
		.name = "RfLMX4x68Data_patch[1]",
		.offset = 180,
		.size = 4,
	},
	{
		.name = "RfLMX4x68Data_patch[2]",
		.offset = 184,
		.size = 4,
	},
	{
		.name = "RfLMX4x68Data_patch[3]",
		.offset = 188,
		.size = 4,
	},
	{
		.name = "RfLMX4x68Data_patch[4]",
		.offset = 192,
		.size = 4,
	},
	{
		.name = "RfLMX4x68Data_patch[5]",
		.offset = 196,
		.size = 4,
	},
	{
		.name = "RfLMX4x68Data_patch[6]",
		.offset = 200,
		.size = 4,
	},
	{
		.name = "RfLMX4x68Data_patch[7]",
		.offset = 204,
		.size = 4,
	},
	{
		.name = "RfLMX4x68Data_patch[8]",
		.offset = 208,
		.size = 4,
	},
	{
		.name = "RfLMX4x68Data_patch[9]",
		.offset = 212,
		.size = 4,
	},
	{
		.name = "Ac",
		.offset = 267,
		.size = 4,
	},
	{
		.name = "DeleteMmeiArr",
		.offset = 478,
		.size = 16,
	},
	{
		.name = "DeleteIpui1",
		.offset = 494,
		.size = 5,
	},
	{
		.name = "DeleteIpui2",
		.offset = 499,
		.size = 5,
	},
	{
		.name = "Ipui1",
		.offset = 504,
		.size = 5,
	},
	{
		.name = "Ipui2",
		.offset = 509,
		.size = 5,
	},
	{
		.name = "Ipui3",
		.offset = 514,
		.size = 5,
	},
	{
		.name = "Ipui4",
		.offset = 519,
		.size = 5,
	},
	{
		.name = "Ipui5",
		.offset = 524,
		.size = 5,
	},
	{
		.name = "Ipu6",
		.offset = 529,
		.size = 5,
	},
	{
		.name = "Uak1",
		.offset = 534,
		.size = 16,
	},
	{
		.name = "Uak2",
		.offset = 550,
		.size = 16,
	},
	{
		.name = "Uak3",
		.offset = 566,
		.size = 16,
	},
	{
		.name = "Uak4",
		.offset = 582,
		.size = 16,
	},
	{
		.name = "Uak5",
		.offset = 598,
		.size = 16,
	},
	{
		.name = "Uak6",
		.offset = 614,
		.size = 16,
	},
	{
		.name = "PpCapability",
		.offset = 654,
		.size = 16,
	},
	{
		.name = "InitialTestMode",
		.offset = 128,
		.size = 1,
	},
	{
		.name = "Tbr_22",
		.offset = 272,
		.size = 1,
	},
	{
		.name = "ClSelect",
		.offset = 271,
		.size = 1,
	},
	{
		.name = "bNoEmissionEnable",
		.offset = 59,
		.size = 1,
	},
	{
		.name = "AutoTest",
		.offset = 266,
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
		printf("%-25s\t\t[%04X]: ", entry.name, entry.offset);
		for (j = 0; j < entry.size; j++)
			printf("%2.2X ", buf[entry.offset + j]);
		printf("\n");
	}

	exit(EXIT_SUCCESS);
}
