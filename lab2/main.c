#include "stdio.h"
#include "string.h"

void convert_num(int dec_num, const int base, char (*buffer)[])
{
    memset(*buffer, 0, 7);
   	int i = 7;

   	while (dec_num > 0) {
   		int x = dec_num % base;
   		dec_num /= base;

   		char z;
   		if(x < 10) {
       		z = '0' + x;
     	}
     	else {
       		z = 'A' - 10 + x;
       	}
     	
     	(*buffer)[i] = z;
     	i--;
   }
}

int main()
{
	char buffer[7] = { '0' };
	convert_num(15, 2, &buffer);
	printf("bin_num = %s\n", buffer);
}