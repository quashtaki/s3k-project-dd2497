#include "altc/altio.h"
#include "s3k/s3k.h"
#include "../conf.h"
#define APP_ADDRESS 0x80040000

int main(void)
{
	alt_puts("APP1: hello from app1");

	volatile int *msg_status = (int *)0x80097925;
	*msg_status = 1;
	s3k_msg_t msg;
	s3k_reply_t reply;
	s3k_err_t err;
	msg.data[0] = s3k_get_pid();
	msg.data[1] = 94;
	msg.data[2] = 94;
	msg.data[3] = 7;

	alt_puts("APP1: Trying to revoke own capability");

	err = s3k_cap_revoke(7); // not allowed to revoke
	
	if (err = S3K_SUCCESS) {
		alt_puts("APP1: Successfully revoked capability");
	} else {
		alt_puts("APP1: Failed to revoke capability");
	}
	
	if (USE_SENDRECV) {
		if (USE_SHARED_MEMORY) {
			s3k_reg_write(S3K_REG_SERVTIME, 4500);
			do {
				reply = s3k_sock_sendrecv(14, &msg);
			} while (*msg_status);
		} else {
			s3k_reg_write(S3K_REG_SERVTIME, 4500);
			do {
				reply = s3k_sock_sendrecv(14, &msg);
			} while (reply.err != 0);
		}
	} else {
		s3k_reg_write(S3K_REG_SERVTIME, 4500);
		do {
			err = s3k_sock_send(14, &msg);
		} while (err != 0);
	}
	
	alt_puts("APP1: bye from from app1");
}