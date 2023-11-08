#include "altc/altio.h"
#include "s3k/s3k.h"

#define APP0_PID 0
#define APP1_PID 1

// See plat_conf.h
#define BOOT_PMP 0
#define RAM_MEM 1
#define UART_MEM 2
#define TIME_MEM 3
#define HART0_TIME 4
#define HART1_TIME 5
#define HART2_TIME 6
#define HART3_TIME 7
#define MONITOR 8
#define CHANNEL 9

void *memcpy(void *dest, const void *src, size_t n)
{
	for (int i = 0; i < n; ++i) {
		((char *)dest)[i] = ((char *)src)[i];
	}
	return dest;
}

void setup_uart(uint64_t uart_idx)
{
	uint64_t uart_addr = s3k_napot_encode(0x10000000, 0x8);
	// Derive a PMP capability for accessing UART
	s3k_cap_derive(UART_MEM, uart_idx, s3k_mk_pmp(uart_addr, S3K_MEM_RW));
	// Load the derive PMP capability to PMP configuration
	s3k_pmp_load(uart_idx, 1);
	// Synchronize PMP unit (hardware) with PMP configuration
	s3k_sync_mem();
}

void setup_buffer_parent_cap(uint64_t parent)
{
	s3k_cap_derive(RAM_MEM, parent,
				   s3k_mk_memory(0x80030000, 0x80040000, S3K_MEM_RW));
}
void setup_buffer_access(uint64_t parent, uint64_t pmp)
{
	s3k_cap_derive(parent, pmp,
				   s3k_mk_pmp( s3k_napot_encode(0x80030000, 0x10000), S3K_MEM_RW));
	s3k_pmp_load(pmp, 2);
	s3k_sync_mem();
}

void setup_app1(uint64_t parent, uint64_t tmp)
{
	uint64_t uart_addr = s3k_napot_encode(0x10000000, 0x8);
	uint64_t app1_addr = s3k_napot_encode(0x80020000, 0x10000);

	// Derive a PMP capability for app1 main memory
	s3k_cap_derive(RAM_MEM, parent, s3k_mk_memory(0x80020000, 0x80030000, S3K_MEM_RWX));
	s3k_cap_derive(parent, tmp, s3k_mk_pmp(app1_addr, S3K_MEM_RWX));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, APP1_PID, 0);
	s3k_mon_pmp_load(MONITOR, APP1_PID, 0, 0);

	// Derive a PMP capability for uart
	s3k_cap_derive(UART_MEM, tmp, s3k_mk_pmp(uart_addr, S3K_MEM_RW));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, APP1_PID, 1);
	s3k_mon_pmp_load(MONITOR, APP1_PID, 1, 1);

	// derive a time slice capability
	s3k_cap_derive(HART0_TIME, tmp, s3k_mk_time(S3K_MIN_HART, 0,
	S3K_SLOT_CNT / 2));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, APP1_PID, 2);

	// Write start PC of app1 to PC
	s3k_mon_reg_write(MONITOR, APP1_PID, S3K_REG_PC, 0x80020000);
}

void setup_socket(uint64_t socket, uint64_t tmp)
{
	s3k_cap_derive(CHANNEL, socket, s3k_mk_socket(0, S3K_IPC_YIELD, S3K_IPC_SCAP| S3K_IPC_SDATA | S3K_IPC_CDATA, 0));
	s3k_cap_derive(socket, tmp, s3k_mk_socket(0, S3K_IPC_YIELD, S3K_IPC_SCAP| S3K_IPC_SDATA | S3K_IPC_CDATA, 1));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, APP1_PID, 3);
}

int main(void)
{
	s3k_cap_delete(HART1_TIME);
	s3k_cap_delete(HART2_TIME);
	s3k_cap_delete(HART3_TIME);
	// Setup UART access
	setup_uart(10);

	// Setup app1 capabilities and PC
	setup_app1(13, 11);

	// Setup socket capabilities.
	setup_socket(11, 12);

	// Setup buffer for message passing
	setup_buffer_parent_cap(14);

	// Resume app1
	s3k_mon_resume(MONITOR, APP1_PID);

	alt_printf("App 0 started\n");

	s3k_msg_t msg;
	s3k_reply_t reply;
	memcpy(msg.data, "pong", 5);
	s3k_reg_write(S3K_REG_SERVTIME, 0);
	while (1) {
		do {
			s3k_cap_revoke(14);
			setup_buffer_access(14, 15);
			memcpy((void*)0x80030000, "pong", 5);
			msg.send_cap = 1;
			msg.cap_idx = 15;
			reply = s3k_sock_sendrecv(11, &msg);
		} while (reply.err);
		alt_printf("Time %X\n", s3k_get_time());
		alt_puts((char *)reply.data);
	}
}
