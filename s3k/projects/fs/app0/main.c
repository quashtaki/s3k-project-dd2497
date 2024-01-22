#include "altc/altio.h"
#include "s3k/s3k.h"

#include "ff.h"
#include "../conf.h"

#define APP0_PID 0
#define APP1_PID 1
#define MONITOR_PID 2

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

#define DRIVER_ADDRESS 0x80010000
#define APP_ADDRESS 0x80020000
#define MONITOR_ADDRESS 0x80090000

void setup_monitor(uint64_t tmp)
{
	uint64_t uart_addr = s3k_napot_encode(UART0_BASE_ADDR, 0x8);
	uint64_t mon_addr = s3k_napot_encode(MONITOR_ADDRESS, 0x10000);

	// Derive a PMP capability for mon main memory
	s3k_cap_derive(RAM_MEM, tmp, s3k_mk_pmp(mon_addr, S3K_MEM_RWX));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, MONITOR_PID, 0);
	s3k_mon_pmp_load(MONITOR, MONITOR_PID, 0, 0);
	// s3k_mon_cap_move(MONITOR, MONITOR_PID, 0, MONITOR_PID, 10);

	uint64_t driver_addr = s3k_napot_encode(DRIVER_ADDRESS, 0x10000);
	s3k_cap_derive(RAM_MEM, tmp, s3k_mk_pmp(driver_addr, S3K_MEM_RWX));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, MONITOR_PID, 2);
	s3k_mon_pmp_load(MONITOR, MONITOR_PID, 2, 2);

	// Derive a PMP capability for uart
	s3k_cap_derive(UART_MEM, tmp, s3k_mk_pmp(uart_addr, S3K_MEM_RW));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, MONITOR_PID, 1);
	s3k_mon_pmp_load(MONITOR, MONITOR_PID, 1, 1);
	s3k_mon_cap_move(MONITOR, MONITOR_PID, 1, MONITOR_PID, 11);
}

void start_monitor(uint64_t tmp) {
	// derive a time slice capability
	s3k_cap_derive(HART0_TIME, tmp,
		       s3k_mk_time(S3K_MIN_HART, 0, 8));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, MONITOR_PID, 2);
	s3k_mon_cap_move(MONITOR, MONITOR_PID, 2, MONITOR_PID, 12);

	// Write start PC of app1 to PC
	s3k_mon_reg_write(MONITOR, MONITOR_PID, S3K_REG_PC, MONITOR_ADDRESS);

	// Start app1
	s3k_mon_resume(MONITOR, MONITOR_PID);
}

void setup_app1(uint64_t tmp)
{
	uint64_t uart_addr = s3k_napot_encode(UART0_BASE_ADDR, 0x8);
	uint64_t app1_addr = s3k_napot_encode(APP_ADDRESS, 0x10000);

	// Derive a PMP capability for app1 main memory - guessing can do in our monitor but hard to tell as cant launch app1 given subsequent derive of HART (may fail silently)
	s3k_cap_derive(RAM_MEM, tmp, s3k_mk_pmp(app1_addr, S3K_MEM_RWX));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, APP1_PID, 0);
	s3k_mon_pmp_load(MONITOR, APP1_PID, 0, 0);

	// Derive a PMP capability for uart - guessing can do in our monitor but hard to tell as cant launch app1 given subsequent derive of HART (may fail silently)
	s3k_cap_derive(UART_MEM, tmp, s3k_mk_pmp(uart_addr, S3K_MEM_RW));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, APP1_PID, 1);
	s3k_mon_pmp_load(MONITOR, APP1_PID, 1, 1);
}

void start_app1(uint64_t tmp) {
	// derive a time slice capability - cant seem to do in our monitor regardless of what HART is moved there (not sure why)??
	s3k_cap_derive(HART0_TIME, tmp,
		       s3k_mk_time(S3K_MIN_HART, 0, 8));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, APP1_PID, 2);

	// Write start PC of app1 to PC - can do in our monitor
	// s3k_mon_reg_write(MONITOR, APP1_PID, S3K_REG_PC, APP_ADDRESS);

	// // Start app1
	// s3k_mon_resume(MONITOR, APP1_PID);
}

void setup_socket(uint64_t socket, uint64_t tmp, uint64_t tmp1)
{
	s3k_cap_derive(CHANNEL, socket,
		       s3k_mk_socket(0, S3K_IPC_NOYIELD, // S3K_IPC_YIELD
				     S3K_IPC_SDATA  | S3K_IPC_CDATA , 0)); // S3K_IPC_SDATA  | S3K_IPC_CDATA
	s3k_cap_derive(socket, tmp,
		       s3k_mk_socket(0, S3K_IPC_NOYIELD,
				     S3K_IPC_SDATA  | S3K_IPC_CDATA  , 1)); // S3K_IPC_SDATA | S3K_IPC_SCAP | S3K_IPC_CDATA | S3K_IPC_CCAP
	s3k_cap_derive(socket, tmp1,
		       s3k_mk_socket(0, S3K_IPC_NOYIELD,
				     S3K_IPC_SDATA  | S3K_IPC_CDATA , 1));
	s3k_mon_cap_move(MONITOR, APP0_PID, socket, MONITOR_PID, 14); // give monitor server
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp1, APP1_PID, 14); // give app1 client
}

// seems reasonable to just move most - could derive big region and delgate from that instead (at least for mem)
void move_caps_to_mon() {
	alt_puts("APP0: move most caps to monitor");
	// s3k_mon_cap_move(MONITOR, APP0_PID, BOOT_PMP, MONITOR_PID, BOOT_PMP); // nope?
	s3k_mon_cap_move(MONITOR, APP0_PID, RAM_MEM, MONITOR_PID, RAM_MEM); // yes - let monitor derive
	s3k_mon_cap_move(MONITOR, APP0_PID, UART_MEM, MONITOR_PID, UART_MEM); // yes - let monitor derive
	s3k_mon_cap_move(MONITOR, APP0_PID, TIME_MEM, MONITOR_PID, TIME_MEM); // don't need but why not?
	s3k_mon_cap_move(MONITOR, APP0_PID, HART0_TIME, MONITOR_PID, HART0_TIME);
	s3k_mon_cap_move(MONITOR, APP0_PID, HART1_TIME, MONITOR_PID, HART1_TIME); // why not?
	s3k_mon_cap_move(MONITOR, APP0_PID, HART2_TIME, MONITOR_PID, HART2_TIME); // why not?
	// keep HART3 here
	s3k_mon_cap_move(MONITOR, APP0_PID, CHANNEL, MONITOR_PID, CHANNEL); // yes - let monitor derive
}

int main(void)
{	
	// s3k_cap_delete(HART1_TIME);
	// s3k_cap_delete(HART2_TIME);
	// s3k_cap_delete(HART3_TIME);
	uint64_t uart_addr = s3k_napot_encode(UART0_BASE_ADDR, 0x2000);
	uint64_t mon_addr = s3k_napot_encode(MONITOR_ADDRESS, 0x10000);
	while (s3k_cap_derive(RAM_MEM, 28, s3k_mk_pmp(mon_addr, S3K_MEM_RWX)))
	;
	while (s3k_pmp_load(28, 2))
		;
	while (s3k_cap_derive(2, 16, s3k_mk_pmp(uart_addr, S3K_MEM_RW)))
		;
	while (s3k_pmp_load(16, 1))
		;
	s3k_sync();

	setup_app1(11);	
	setup_monitor(12);
	setup_socket(13, 14, 19); // Socket is on 13 - and moved to 4
	// setup_socket_app1(18, 19); // Socket on 18 - moved to 4

	start_app1(11);
	move_caps_to_mon();

	// monitor for app0, app1, monitor
	alt_puts("APP0: moved monitor cap to monitor");
	s3k_cap_derive(MONITOR, 17, s3k_mk_monitor(0, 2));
	s3k_err_t err = s3k_mon_cap_move(MONITOR, APP0_PID, 17, MONITOR_PID, MONITOR);

	FATFS FatFs;		/* FatFs work area needed for each volume */
	f_mount(&FatFs, "", 0);		/* Give a work area to the default drive */
	alt_puts("APP0: File system mounted");

	UINT bw;
	FRESULT fr;
	FIL Fil;			/* File object needed for each open file */

	char buffer[1024];
	fr = f_open(&Fil, "test.txt", FA_READ);
	if (fr == FR_OK) {
		// alt_puts("APP0: File opened\n");
		f_read(&Fil, buffer, 1023, &bw);	/*Read data from the file */
		fr = f_close(&Fil);							/* Close the file */
		// if (fr == FR_OK) {
		// 	buffer[bw] = '\0';
		// 	alt_puts(buffer);
		// }
	}
	start_monitor(12);
	// s3k_sync();
	alt_puts("APP0: hello from app0");
		
	// FATFS FatFs;		/* FatFs work area needed for each volume */
	// f_mount(&FatFs, "", 0);		/* Give a work area to the default drive */
	// alt_puts("APP0: File system mounted");

	// UINT bw;
	// FRESULT fr;
	// FIL Fil;			/* File object needed for each open file */

	// char buffer[1024];
	fr = f_open(&Fil, "test.txt", FA_READ);
	if (fr == FR_OK) {
		alt_puts("APP0: File opened\n");
		f_read(&Fil, buffer, 1023, &bw);	/*Read data from the file */
		fr = f_close(&Fil);							/* Close the file */
		if (fr == FR_OK) {
			buffer[bw] = '\0';
			alt_puts(buffer);
		}
	} else{
		alt_puts("APP0: File not opened\n");
	}

	s3k_msg_t msg;
  msg.data[0] = s3k_get_pid();
  msg.data[1] = 94;
  msg.data[2] = 94;
  msg.data[3] = 0;
  s3k_reply_t reply;

	if (REVOKE_DRIVER_MEM_EMPTY_QUEUE) {
		alt_puts("REVOKE EMPTY QUEUE");
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

	alt_puts("APP0: done");
}