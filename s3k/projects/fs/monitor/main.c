#include "altc/altio.h"
#include "altc/string.h"
#include "s3k/s3k.h"

#define APP0_PID 0

#define MONITOR 8

int main(void)
{
	alt_puts("MONITOR: Monitor started");

	s3k_msg_t msg;
	s3k_reply_t reply;
	msg.data[0] = 1; // true or false
	
	while (1) {
		alt_puts("MONITOR: waiting for req");

		do {		
			reply = s3k_sock_recv(4,0);

			if (reply.err == S3K_ERR_TIMEOUT)
				alt_puts("MONITOR: timeout");
		} while (reply.err);
		alt_puts("MONITOR: received");
		// reply data is hex
		alt_printf("MONITOR: data: %X\n", reply.data);
		
		// we want to check if address is within app0 capabilities
		s3k_mon_suspend(MONITOR, APP0_PID);
		alt_puts("MONITOR: CHECKING CAPS");
		for (int i = 0; i < 15; i++) {
			s3k_cap_t cap;
			s3k_err_t err = s3k_mon_cap_read(MONITOR, APP0_PID, i, &cap);
			alt_printf("MONITOR: cap: %X\n", cap.type);
			alt_printf("MONITOR: err: %X\n", err);
		}

		s3k_mon_resume(MONITOR, APP0_PID);
		// check if address is ok in cap - TODO

		while (s3k_sock_send(4, &msg));
		alt_puts("MONITOR: sent");
	}
}

