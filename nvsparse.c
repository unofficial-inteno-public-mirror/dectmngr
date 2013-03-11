#include <stdio.h>
#include <stdlib.h>


#define ARRAY_SIZE(a) sizeof(a) / sizeof((a)[0])

typedef struct nvs_entry {
	const char *name;
	int offset;
	int size;
} nvs_entry;


static void print_header(void)
{
	printf("Name\t\t\tOffset\tValue\n");
}


static const nvs_entry nvs_entry_table[] = {
	{
		.name = "FixedEmc",
		.offset = 9,
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
		

int main(int argc, char *argv[])
{
	nvs_entry entry;
	int i;

	print_header();

	for (i = 0; i < ARRAY_SIZE(nvs_entry_table); i++) {
		entry = nvs_entry_table[i];
		printf("%-20s\t[%04d]: \n", entry.name, entry.offset);
	}

	exit(EXIT_SUCCESS);
}
