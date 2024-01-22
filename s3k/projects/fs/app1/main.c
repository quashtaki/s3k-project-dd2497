#include "altc/altio.h"
#include "s3k/s3k.h"
#include "../conf.h"
#define APP_ADDRESS 0x80020000

int main(void)
{
	alt_puts("APP1: hello from app1");

	s3k_msg_t msg;
	s3k_reply_t reply;
	s3k_err_t err;
	msg.data[0] = s3k_get_pid();
	// msg.data[1] = 64;
	// msg.data[2] = 64;
	msg.data[1] = 94;
	msg.data[2] = 94;
	msg.data[3] = 5;
	
	if (REVOKE_UNRELATED_MEM) {
		if (USE_SENDRECV) {
			s3k_reg_write(S3K_REG_SERVTIME, 4500);
			do {
				reply = s3k_sock_sendrecv(14, &msg);
			} while (reply.err != 0);
		} else {
			s3k_reg_write(S3K_REG_SERVTIME, 4500);
			do {
				err = s3k_sock_send(14, &msg);
			} while (err != 0);
		}
	}
	
	// s3k_cap_revoke(5); // allowed to revoke - nope resume nope nope

	alt_puts("APP1: bye from from app1");
}