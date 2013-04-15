#include <stdio.h>
#include <string.h>

#define BUF_SIZE 1000


/* convert ascii to hex */
int atohx(unsigned char * dst, const unsigned char * src)
{
	char *ret = dst;
	unsigned char lsb, msb;


	
	for(; *src; src += 2) {
		
		msb = tolower(*src);
		lsb = tolower(*(src + 1));
		msb -= isdigit(msb) ? 0x30 : 0x57;
		lsb -= isdigit(lsb) ? 0x30 : 0x57;
		if((msb < 0x0 || msb > 0xf) || (lsb < 0x0 || lsb > 0xf))
			return -1;
			
		*dst++ = (unsigned char)(lsb | (msb << 4));
	}

	return 0;
}


int main(void) {
	char s[BUF_SIZE];
	unsigned char o[BUF_SIZE];
	int len;
	
	fgets(s, BUF_SIZE, stdin);
	len = strlen(s);
	atohx(o, s);
	
	fwrite(o, 1, (len -1)/2, stdout);
}
