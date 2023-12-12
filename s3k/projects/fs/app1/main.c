#include "altc/altio.h"
#include "s3k/s3k.h"

int main(void)
{
	alt_puts("APP1: hello from app1");
	alt_printf("APP1: pid %X\n", s3k_get_pid());
	
	alt_puts("APP1: bye from from app1");
}