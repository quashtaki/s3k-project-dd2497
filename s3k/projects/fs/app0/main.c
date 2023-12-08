#include "altc/altio.h"
#include "s3k/s3k.h"

#include "ff.h"

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

#define APP_ADDRESS 0x80020000
#define MONITOR_ADDRESS 0x80040000

#define SHARED_MEM 0x80050000
#define SHARED_MEM_LEN 0x10000

void setup_monitor(uint64_t tmp)
{
	uint64_t uart_addr = s3k_napot_encode(UART0_BASE_ADDR, 0x8);
	uint64_t mon_addr = s3k_napot_encode(MONITOR_ADDRESS, 0x10000);

	// Derive a PMP capability for mon main memory
	s3k_cap_derive(RAM_MEM, tmp, s3k_mk_pmp(mon_addr, S3K_MEM_RWX));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, MONITOR_PID, 0);
	s3k_mon_pmp_load(MONITOR, MONITOR_PID, 0, 0);

	// Derive a PMP capability for uart
	s3k_cap_derive(UART_MEM, tmp, s3k_mk_pmp(uart_addr, S3K_MEM_RW));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, MONITOR_PID, 1);
	s3k_mon_pmp_load(MONITOR, MONITOR_PID, 1, 1);
}

void start_monitor(uint64_t tmp) {
	// derive a time slice capability
	s3k_cap_derive(HART0_TIME, tmp,
		       s3k_mk_time(S3K_MIN_HART, 0, 8)); // 0-5
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, MONITOR_PID, 3);

	// Write start PC of app1 to PC
	s3k_mon_reg_write(MONITOR, MONITOR_PID, S3K_REG_PC, MONITOR_ADDRESS);

	// Start app1
	s3k_mon_resume(MONITOR, MONITOR_PID);
}

void setup_app1(uint64_t tmp)
{
	uint64_t uart_addr = s3k_napot_encode(UART0_BASE_ADDR, 0x8);
	uint64_t app1_addr = s3k_napot_encode(APP_ADDRESS, 0x10000);

	// Derive a PMP capability for app1 main memory
	s3k_cap_derive(RAM_MEM, tmp, s3k_mk_pmp(app1_addr, S3K_MEM_RWX));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, APP1_PID, 0);
	s3k_mon_pmp_load(MONITOR, APP1_PID, 0, 0);

	// Derive a PMP capability for uart
	s3k_cap_derive(UART_MEM, tmp, s3k_mk_pmp(uart_addr, S3K_MEM_RW));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, APP1_PID, 1);
	s3k_mon_pmp_load(MONITOR, APP1_PID, 1, 1);
}

void start_app1(uint64_t tmp) {
	// derive a time slice capability
	s3k_cap_derive(HART0_TIME, tmp,
		       s3k_mk_time(S3K_MIN_HART, 8, 16));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, APP1_PID, 2);

	// Write start PC of app1 to PC
	s3k_mon_reg_write(MONITOR, APP1_PID, S3K_REG_PC, APP_ADDRESS);

	// Start app1
	s3k_mon_resume(MONITOR, APP1_PID);
}

void setup_socket(uint64_t socket, uint64_t tmp)
{
	s3k_cap_derive(CHANNEL, socket,
		       s3k_mk_socket(0, S3K_IPC_NOYIELD,
				     S3K_IPC_SDATA | S3K_IPC_CDATA, 0));
	s3k_cap_derive(socket, tmp,
		       s3k_mk_socket(0, S3K_IPC_NOYIELD,
				     S3K_IPC_SDATA | S3K_IPC_CDATA, 1));
	s3k_mon_cap_move(MONITOR, APP0_PID, socket, MONITOR_PID, 4); // give monitor server
}


void setup_shared(uint64_t tmp)
{
	uint64_t shared_address = s3k_napot_encode(SHARED_MEM, SHARED_MEM_LEN);
	// derive a shared memory from ram🍴
	s3k_err_t err = s3k_cap_derive(RAM_MEM, tmp, s3k_mk_memory(0x80050000, 0x80060000, S3K_MEM_RW));
	alt_printf("APP0: shared mem derivation result %X\n", err);
	// create two pmps for shared memory
	s3k_cap_derive(tmp, 20, s3k_mk_pmp(shared_address, S3K_MEM_RW)); // TODO: this to only read 
	s3k_cap_derive(tmp, 21, s3k_mk_pmp(shared_address, S3K_MEM_RW));
	// delete mem cap
	s3k_cap_delete(tmp);
	// move pmp 1 to app0 and pmp 2 to monitor
	s3k_mon_cap_move(MONITOR, APP0_PID, 21, MONITOR_PID, 2);
	// load pmp 1 for app0 and pmp 2 for monitor
	s3k_pmp_load(20, 2);
	s3k_mon_pmp_load(MONITOR, MONITOR_PID, 2, 2);

	s3k_sync_mem();
}

int main(void)
{	
	s3k_cap_delete(HART1_TIME);
	s3k_cap_delete(HART2_TIME);
	s3k_cap_delete(HART3_TIME);
	uint64_t uart_addr = s3k_napot_encode(UART0_BASE_ADDR, 0x2000);
	while (s3k_cap_derive(2, 16, s3k_mk_pmp(uart_addr, S3K_MEM_RW)))
		;
	while (s3k_pmp_load(16, 1))
		;
	s3k_sync();

	setup_monitor(11);
	setup_app1(12);
	setup_socket(13, 14); // Socket is on 13 - and moved to 4
	setup_shared(15); // doesnt work

	s3k_sync();

	// monitor for app0, app1, monitor
	s3k_cap_derive(MONITOR, 17, s3k_mk_monitor(0, 2));
	s3k_mon_cap_move(MONITOR, APP0_PID, 17, MONITOR_PID, MONITOR);

	alt_puts("APP0: gave mon cap to monitor");

	// Order of starting these matters 💀
	start_monitor(11);
	start_app1(12);

	s3k_mon_cap_move(MONITOR, APP0_PID, 12, APP1_PID, 3); // move out capability to app1
	// this removes our ability to edit its memory


	alt_puts("APP0: hello from app0");
	
	FATFS FatFs;		/* FatFs work area needed for each volume */
	f_mount(&FatFs, "", 0);		/* Give a work area to the default drive */
	alt_puts("APP0: File system mounted");

	UINT bw;
	FRESULT fr;
	FIL Fil;			/* File object needed for each open file */


	char buffer[1024];
	fr = f_open(&Fil, "app2.bin", FA_READ);
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

	// alt_puts("APP0: hello from app0");
	s3k_mon_suspend(MONITOR, APP1_PID);
	s3k_mon_reg_write(MONITOR, APP1_PID, S3K_REG_PC, APP_ADDRESS);
	s3k_mon_resume(MONITOR, APP1_PID);
}
