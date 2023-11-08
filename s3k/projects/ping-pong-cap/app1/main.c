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
	s3k_msg_t msg;
	s3k_reply_t reply;
	memcpy(msg.data, "ping", 5);
	alt_printf("App 1 started\n");
	while (1) {
		do {
			s3k_cap_delete(4);
			msg.cap_idx = 4;
			reply = s3k_sock_sendrecv(3, &msg);
			if (reply.err == S3K_ERR_TIMEOUT)
				alt_puts("timeout");
		} while (reply.err);
		alt_puts((char *)reply.data);
		/* alt_printf("> cap %X\n", reply.cap); */
		if (reply.cap.type != 0) {
			s3k_pmp_load(4, 4);
			s3k_sync_mem();
			alt_puts((char *)0x80030000);
		}
	}
}
