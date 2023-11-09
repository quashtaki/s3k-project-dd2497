#include "altc/altio.h"
#include "s3k/s3k.h"

#include <stdint.h>

void *memcpy(void *dest, const void *src, size_t n)
{
	for (int i = 0; i < n; ++i) {
		((char *)dest)[i] = ((char *)src)[i];
	}
	return dest;
}

int main(void)
{
	alt_printf("hello from app1");
	s3k_msg_t msg;
	memcpy(msg.data, "app2.bin", 9);
	s3k_err_t err = s3k_sock_send(3, &msg);
	if (err == S3K_ERR_TIMEOUT)
		alt_puts("timeout");
}