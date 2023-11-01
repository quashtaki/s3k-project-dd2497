#include "altio/altio.h"
#include "s3k/s3k.h"

int main(void)
{
	uint64_t uart_addr = s3k_napot_encode(0x10000000, 0x8);
	while (s3k_cap_derive(2, 16, s3k_mk_pmp(uart_addr, S3K_MEM_RW)))
		;
	while (s3k_pmp_load(16, 1))
		;
	s3k_sync();
	alt_puts("hello, world");

}
