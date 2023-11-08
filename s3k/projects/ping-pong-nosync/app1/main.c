#include "altc/altio.h"
#include "altc/string.h"
#include "s3k/s3k.h"

#include <stdint.h>

int main(void)
{
	alt_puts("starting app1");
	s3k_msg_t msg;
	s3k_reply_t reply;
	memcpy(msg.data, "ping", 5);
	while (1) {
		do {
			reply = s3k_sock_sendrecv(3, &msg);
			if (reply.err == S3K_ERR_TIMEOUT)
				alt_puts("1> timeout");
		} while (reply.err);
		uint64_t time = s3k_get_time();
		while (s3k_get_time() - time < 10000000) {
		}
		alt_puts((char *)reply.data);
	}
}
