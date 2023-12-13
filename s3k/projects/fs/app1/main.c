#include "altc/altio.h"
#include "s3k/s3k.h"

int main(void)
{
	alt_puts("APP1: hello from app1");
	alt_printf("APP1: pid %X\n", s3k_get_pid());

	s3k_msg_t msg;
	msg.data[0] = 4;
	msg.data[1] = 4;
	msg.data[2] = 4;
	msg.data[3] = 4;
  s3k_reply_t reply;
  s3k_reg_write(S3K_REG_SERVTIME, 4500);

  s3k_err_t err;
	do {
		reply = s3k_sock_sendrecv(4, &msg);
	} while (reply.err != 0);
	alt_puts("APP1 recipient");
	if (reply.data[0] == 0 && reply.data[1] == 0 && reply.data[2] == 0 && reply.data[3] == 0) { 
		alt_puts("APP1: NO REVOKE YO");
	}
	if (reply.data[0] == 1 && reply.data[1] == 1 && reply.data[2] == 1 && reply.data[3] == 1) {
		alt_puts("APP1: REVOKE LETS SHUT DRIVER DOWN");

		// todo in revoke trigger error until shut down

		// do { ... } while (s3k_cap_revoke(15));
		s3k_cap_revoke(15);
	}
	alt_puts((char *)reply.data);


	alt_puts("APP1: bye from from app1");
}