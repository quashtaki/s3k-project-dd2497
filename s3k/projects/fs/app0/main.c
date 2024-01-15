#include "altc/altio.h"
#include "s3k/s3k.h"

#include "ff.h"

#define MONITOR_PID 0
#define APP0_PID 1
#define APP1_PID 2


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

//#define APP1_ADDRESS 0x80020000
//#define MONITOR_ADDRESS 0x80040000

#define SHARED_MEM 0x80050000
#define SHARED_MEM_LEN 0x10000


int main(void)
{	

	alt_puts("APP0: hello from app0");
	//s3k_mon_cap_move(MONITOR, APP0_PID, 11, APP1_PID, 3); // move out capability to app1
	
	FATFS FatFs;		/* FatFs work area needed for each volume */
	f_mount(&FatFs, "", 0);		/* Give a work area to the default drive */
	alt_puts("APP0: File system mounted");

	UINT bw;
	FRESULT fr;
	FIL Fil;			/* File object needed for each open file */
	
	char buffer[1024];
	
	fr = f_open(&Fil, "app2.bin", FA_READ);
	alt_puts("APP0: File opened");
	alt_printf("APP0: fr rn?: %X\n", fr);
	if (fr == FR_OK) {
		f_read(&Fil, buffer, 1023, &bw);	/*Read data from the file */	
		fr = f_close(&Fil);							/* Close the file */
		if (fr == FR_OK) {
			buffer[bw] = '\0';
			alt_puts(buffer);
		}
	} else{
		alt_puts("APP0: File not opened\n");
	}
}
