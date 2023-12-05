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
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, MONITOR_PID, 2);

	// print S3K_SLOT_CNT in hex
	alt_printf("HART: %X\n", S3K_MIN_HART);

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


	alt_puts("starting monitor (Not the real one)");
	setup_monitor(11);
	setup_app1(12);

	// Order of starting these matters 💀
	start_monitor(11);
	start_app1(12);



	

	// s3k_mon_cap_move(MONITOR, APP0_PID, 11, APP1_PID, 3); // move out capability to app1
	// // this removes our ability to edit its memory

	// alt_puts("hello from app0");
	
	// FATFS FatFs;		/* FatFs work area needed for each volume */
	// f_mount(&FatFs, "", 0);		/* Give a work area to the default drive */
	// alt_puts("File system mounted");

	// UINT bw;
	// FRESULT fr;
	// FIL Fil;			/* File object needed for each open file */


	// char buffer[1024];
	// fr = f_open(&Fil, "app2.bin", FA_READ);
	// if (fr == FR_OK) {
	// 	alt_puts("File opened\n");
	// 	f_read(&Fil, buffer, 1023, &bw);	/*Read data from the file */
	// 	fr = f_close(&Fil);							/* Close the file */
	// 	if (fr == FR_OK) {
	// 		buffer[bw] = '\0';
	// 		alt_puts(buffer);
	// 	}
	// } else{
	// 	alt_puts("File not opened\n");
	// }

	// // alt_puts("hello from app0");
	// s3k_mon_suspend(MONITOR, APP1_PID);
	// s3k_mon_reg_write(MONITOR, APP1_PID, S3K_REG_PC, APP_ADDRESS);
	// s3k_mon_resume(MONITOR, APP1_PID);
}
