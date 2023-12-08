#include "altc/altio.h"
#include "altc/string.h"
#include "s3k/s3k.h"

#define APP0_PID 0

#define MONITOR 8

#define SHARED_MEM 0x80050000
#define SHARED_MEM_LEN 0x10000

#define TAG_BLOCK_TO_ADDR(tag, block) ( \
					(((uint64_t) tag) << S3K_MAX_BLOCK_SIZE) + \
					(((uint64_t) block) << S3K_MIN_BLOCK_SIZE) \
					)

void s3k_print_cap(s3k_cap_t *cap) {
	if (!cap)
		alt_printf("Capability is NULL\n");
	switch ((*cap).type) {
	case S3K_CAPTY_NONE:
		alt_printf("No Capability\n");
		break;
	case S3K_CAPTY_TIME:
		alt_printf("Time hart:%X bgn:%X mrk:%X end:%Z\n",
				   (*cap).time.hart, (*cap).time.bgn, (*cap).time.mrk, (*cap).time.end);
		break;
	case S3K_CAPTY_MEMORY:
		alt_printf("Memory rwx:%X lock:%X bgn:%X mrk:%X end:%X\n",
				   (*cap).mem.rwx, (*cap).mem.lck,
				   TAG_BLOCK_TO_ADDR((*cap).mem.tag, (*cap).mem.bgn),
				   TAG_BLOCK_TO_ADDR((*cap).mem.tag, (*cap).mem.mrk),
				   TAG_BLOCK_TO_ADDR((*cap).mem.tag, (*cap).mem.end)
				   );
		break;
	case S3K_CAPTY_PMP:
		alt_printf("PMP rwx:%X used:%X index:%X address:%Z\n",
				   (*cap).pmp.rwx, (*cap).pmp.used, (*cap).pmp.slot, (*cap).pmp.addr);
		break;
	case S3K_CAPTY_MONITOR:
		alt_printf("Monitor  bgn:%X mrk:%X end:%X\n",
				    (*cap).mon.bgn, (*cap).mon.mrk, (*cap).mon.end);
		break;
	case S3K_CAPTY_CHANNEL:
		alt_printf("Channel  bgn:%X mrk:%X end:%X\n",
				    (*cap).chan.bgn, (*cap).chan.mrk, (*cap).chan.end);
		break;
	case S3K_CAPTY_SOCKET:
		alt_printf("Socket  mode:%X perm:%X channel:%X tag:%X\n",
				    (*cap).sock.mode, (*cap).sock.perm, (*cap).sock.chan, (*cap).sock.tag);
		break;
	}
}


int main(void)
{
	alt_puts("MONITOR: Monitor started");

	s3k_msg_t msg;
	s3k_reply_t reply;
	msg.data[0] = 1; // true or false

	char *shared_status = (char*) SHARED_MEM;
	char *shared_result = (char*) SHARED_MEM + 1;

	while (1) {
		alt_puts("MONITOR: waiting for req");

		do {		
			reply = s3k_sock_recv(4,0);

			if (reply.err == S3K_ERR_TIMEOUT)
				alt_puts("MONITOR: timeout");
		} while (reply.err);
		alt_puts("MONITOR: received");
		// suspend instantly
		*shared_status = 0;
		s3k_mon_suspend(MONITOR, APP0_PID);
		// reply data is hex
		alt_printf("MONITOR: data: %X\n", reply.data);
		
		// we want to check if address is within app0 capabilities
	

		alt_puts("MONITOR: CHECKING CAPS");
		for (int i = 0; i < 15; i++) {
			s3k_cap_t cap;
			s3k_err_t err = s3k_mon_cap_read(MONITOR, APP0_PID, i, &cap);
			alt_printf("MONITOR: cap: %X\n", cap.type);
			alt_printf("MONITOR: err: %X\n", err);
		}

		s3k_mon_resume(MONITOR, APP0_PID);
		alt_puts("MONITOR: resuming");
		*shared_result = 1;
		*shared_status = 1;
		alt_puts("MONITOR: sent");
	}
}
