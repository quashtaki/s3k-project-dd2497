#include "altc/altio.h"
#include "altc/string.h"
#include "s3k/s3k.h"

#define APP0_PID 0
#define APP1_PID 1

#define MONITOR 8

#define DRIVER_ADDRESS 0x80010000
#define APP_ADDRESS 0x80020000
#define TEMP_ADDR1 0x80070000

#define SHARED_MEM 0x80050000
#define SHARED_MEM_LEN 0x10000

int DO_REVOKE = 0;

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

bool inside_memory(s3k_cap_t *cap, uint64_t addr) {
	if ((*cap).type != S3K_CAPTY_MEMORY)
		return false;
	uint64_t bgn = TAG_BLOCK_TO_ADDR((*cap).mem.tag, (*cap).mem.bgn);
	uint64_t end = TAG_BLOCK_TO_ADDR((*cap).mem.tag, (*cap).mem.end);
	alt_printf("MONITOR: inside_memory: addr:%X bgn:%X end:%X\n", addr, bgn, end);
	return (addr >= bgn && addr < end);
}

bool inside_pmp(s3k_cap_t *cap, uint64_t addr) {
	if ((*cap).type != S3K_CAPTY_PMP)
		return false;
	uint64_t bgn;
	uint64_t size;
	s3k_napot_decode((*cap).pmp.addr, &bgn, &size);
	uint64_t end = bgn + size;
	int priv = (*cap).pmp.rwx;
	// if not high enough privileges
	if (priv != S3K_MEM_W && priv != S3K_MEM_RW && priv != S3K_MEM_RWX ) {
		return false;
	}
	return (addr >= bgn && addr < end);
}

int main(void)
{
	alt_puts("MONITOR: Monitor started");

	s3k_msg_t msg;
	s3k_reply_t reply;
	s3k_err_t err;
	msg.data[0] = 1; // true or false

	while (1) {
		alt_puts("MONITOR: waiting for req");

		do {		
			reply = s3k_sock_recv(4,0);
			if (reply.err == S3K_ERR_TIMEOUT)
				alt_puts("MONITOR: timeout");
		} while (reply.err);
		alt_puts("MONITOR: received");

		alt_printf("MONITOR: data: %X\n", *reply.data);

		// from App1 to check if it's time for revocation (concurrency, allow driver to read for illustration purposes)
		if (reply.data[0] == 4 && reply.data[1] == 4 && reply.data[2] == 4 && reply.data[3] == 4) {
			alt_puts("MONITOR: FROM APP1 TO MONNI");
			s3k_msg_t msg_app1;
			s3k_err_t err_app1;
			msg_app1.data[0] = DO_REVOKE;
			msg_app1.data[1] = DO_REVOKE;
			msg_app1.data[2] = DO_REVOKE;
			msg_app1.data[3] = DO_REVOKE;
			do {
				err_app1 = s3k_sock_send(4, &msg_app1);
			} while (err_app1 != 0);
		}

		// check with monitor if app may resume (for illustration purposes)
		if (reply.data[0] == 6 && reply.data[1] == 6 && reply.data[2] == 6 && reply.data[3] == 6) {
			alt_puts("MONITOR: Checking allow App0 to resume");

			char* hold_status = (char*) TEMP_ADDR1+0x9000;
			*hold_status = 0;
		}

		// this particular data message is used to give app1 driver memory and trigger its revocation of said memory
		if (reply.data[0] == 7 && reply.data[1] == 7 && reply.data[2] == 7 && reply.data[3] == 7) {
			alt_puts("MONITOR: APP0 TRIGGER MOVING OF CAPABILITY");
			DO_REVOKE = 1;

			s3k_err_t ee10 = s3k_mon_suspend(MONITOR, APP0_PID);
			s3k_err_t ee1 = s3k_mon_suspend(MONITOR, APP1_PID);
			s3k_err_t ee2 = s3k_mon_reg_write(MONITOR, APP1_PID, S3K_REG_PC, APP_ADDRESS);
			s3k_err_t ee4 = s3k_mon_cap_move(MONITOR, APP0_PID, 15, APP1_PID, 15);
			s3k_err_t ee30 = s3k_mon_resume(MONITOR, APP0_PID);
			s3k_err_t ee3 = s3k_mon_resume(MONITOR, APP1_PID);

			if (!ee10 && !ee1 && !ee2 && !ee4 && !ee30 && !ee3) {
				alt_puts("MONITOR: Driver memory moved to APP1");
			}
		}

		if (reply.data[0] == 9 && reply.data[1] == 9 && reply.data[2] == 9 && reply.data[3] == 9) {
			alt_puts("MONITOR: Checking if memory belongs to driver");
			// todo check if memory belongs to driver
			int drivers_memory = 0;
			if (1) { // if belongs
				alt_puts("MONITOR: Memory belongs to driver");
				drivers_memory = 1;
			} else {
				alt_puts("MONITOR: Memory does NOT belong to driver");
			}

			s3k_reg_write(S3K_REG_SERVTIME, 4500);
			do {
				msg.data[0] = drivers_memory;
				msg.data[1] = drivers_memory;
				msg.data[2] = drivers_memory;
				msg.data[3] = drivers_memory;
				DO_WAIT = 0;
				err = s3k_sock_send(4, &msg);
			} while (err != 0);
			}

		alt_puts("MONITOR: sent");
	}
}

