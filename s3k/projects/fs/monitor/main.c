#include "altc/altio.h"
#include "s3k/s3k.h"

int main(void)
{
	alt_puts("MONITOR: Monitor started");

	s3k_msg_t msg;
	s3k_reply_t reply;
	msg.data[0] = 1; // true or false
	
	while (1) {
		alt_puts("MONITOR: waiting for req");
		do {
			reply = s3k_sock_recv(3,0);
			if (reply.err == S3K_ERR_TIMEOUT)
				alt_puts("MONITOR: timeout");
		} while (reply.err);
		alt_puts("MONITOR: received");
		// reply data is hex
		alt_printf("MONITOR: data: %X\n", reply.data);

		// check if address is ok in cap

		s3k_err_t err = s3k_sock_send(3, &msg);
		if (err == S3K_ERR_TIMEOUT)
			alt_puts("MONITOR: timeout");
		alt_puts("MONITOR: sent");
	}
	

// 	s3k_msg_t msg;
//  msg.data[0] = 0; // true or false
}