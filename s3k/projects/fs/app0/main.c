#include "altc/altio.h"
#include "s3k/s3k.h"

#include "ff.h"

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

void setup_uart(uint64_t uart_idx)
{
	uint64_t uart_addr = s3k_napot_encode(UART0_BASE_ADDR, 0x2000);
	// Derive a PMP capability for accessing UART
	s3k_cap_derive(UART_MEM, uart_idx, s3k_mk_pmp(uart_addr, S3K_MEM_RW));
	// Load the derive PMP capability to PMP configuration
	s3k_pmp_load(uart_idx, 1);
	// Synchronize PMP unit (hardware) with PMP configuration
	// false => not full synchronization.
	s3k_sync_mem();
}

void setup_app1(uint64_t tmp)
{
	uint64_t uart_addr = s3k_napot_encode(UART0_BASE_ADDR, 0x8);
	uint64_t app1_addr = s3k_napot_encode(0x80020000, 0x10000);

	// Derive a PMP capability for app1 main memory
	s3k_cap_derive(RAM_MEM, tmp, s3k_mk_pmp(app1_addr, S3K_MEM_RWX));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, APP1_PID, 0);
	s3k_mon_pmp_load(MONITOR, APP1_PID, 0, 0);

	// Derive a PMP capability for uart
	s3k_cap_derive(UART_MEM, tmp, s3k_mk_pmp(uart_addr, S3K_MEM_RW));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, APP1_PID, 1);
	s3k_mon_pmp_load(MONITOR, APP1_PID, 1, 1);

}

void start_app1() {
	// derive a time slice capability
	s3k_mon_cap_move(MONITOR, APP0_PID, HART1_TIME, APP1_PID, 2);

	// Write start PC of app1 to PC
	s3k_mon_reg_write(MONITOR, APP1_PID, S3K_REG_PC, 0x80020000);

	// Start app1
	s3k_mon_resume(MONITOR, APP1_PID);

}

void setup_socket(uint64_t socket, uint64_t tmp)
{
	s3k_cap_derive(CHANNEL, socket,
		       s3k_mk_socket(0, S3K_IPC_YIELD,
				     S3K_IPC_SDATA | S3K_IPC_CDATA, 0));
	s3k_cap_derive(socket, tmp,
		       s3k_mk_socket(0, S3K_IPC_YIELD,
				     S3K_IPC_SDATA | S3K_IPC_CDATA, 1));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, APP1_PID, 3);
}

int main(void)
{

	
	s3k_cap_delete(HART1_TIME);
	s3k_cap_delete(HART2_TIME);
	s3k_cap_delete(HART3_TIME);
	// This is the same stuff as setup_uart
	setup_uart(10);

	// Modify the FileSystem example to boot process A
	setup_app1(11);
	setup_socket(12, 13);
	alt_puts("Starting fs");
	FATFS FatFs;		/* FatFs work area needed for each volume */
	f_mount(&FatFs, "", 0);		/* Give a work area to the default drive */
	alt_puts("File system mounted");

	UINT bw;
	FRESULT fr;
	FIL Fil;			/* File object needed for each open file */

	// fr = f_open(&Fil, "newfile.txt", FA_WRITE | FA_CREATE_ALWAYS);	/* Create a file */
	// if (fr == FR_OK) {
	// 	f_write(&Fil, "It works!\r\n", 11, &bw);	/* Write data to the file */
	// 	fr = f_close(&Fil);							/* Close the file */
	// 	if (fr == FR_OK && bw == 11) {
	// 		alt_puts("File saved\n");
	// 	}
	// } else{
	// 	alt_puts("File not opened\n");
	// }

	// Copy the binary of B to the disk image

	char buffer[1024];
	alt_puts("made a buffer");
	// Allow A to send a message to the filesystem with the path to executable B
	s3k_cap_t cap;
	while (s3k_cap_read(1, &cap))
		;
	alt_puts("starting app1");
	// Resume app1
	start_app1();
	
	alt_puts("app1 done");
	s3k_reply_t reply;
	s3k_reg_write(S3K_REG_SERVTIME, 4500);
	reply = s3k_sock_recv(12, 0);
	if (reply.err)
		alt_puts("timeout");
	



	fr = f_open(&Fil, (char*)reply.data, FA_READ);
	alt_puts("opened a file");
	if (fr == FR_OK) {
		f_read(&Fil, buffer, 1023, &bw);	/*Read data from the file */
		alt_puts("Read data from the file\n");
		fr = f_close(&Fil);							/* Close the file */
		if (fr == FR_OK) {
			buffer[bw] = '\0';
			alt_printf(buffer);
		}
	} else{
		alt_puts("File not opened\n");
	}

	alt_puts("Done");
}
