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

#define DRIVER_ADDRESS 0x80010000
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
		       s3k_mk_time(S3K_MIN_HART, 8, 16));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, MONITOR_PID, 3);

	// Write start PC of app1 to PC
	s3k_mon_reg_write(MONITOR, MONITOR_PID, S3K_REG_PC, MONITOR_ADDRESS);

	// Start app1
	s3k_mon_resume(MONITOR, MONITOR_PID);
}

void setup_app1(uint64_t tmp)
{
	uint64_t uart_addr = s3k_napot_encode(UART0_BASE_ADDR, 0x8);
	uint64_t driver_addr = s3k_napot_encode(DRIVER_ADDRESS, 0x10000);
	uint64_t app1_addr = s3k_napot_encode(APP_ADDRESS, 0x10000);

	// Derive a PMP capability for driver main memory
	// s3k_cap_derive(RAM_MEM, tmp + -2, s3k_mk_pmp(driver_addr, S3K_MEM_RWX));
	// s3k_cap_derive(RAM_MEM, tmp -1, s3k_mk_pmp(driver_addr, S3K_MEM_RWX));
	// // s3k_mon_cap_move(MONITOR, APP0_PID, tmp  -2, APP1_PID, 1);
	// // s3k_mon_pmp_load(MONITOR, APP1_PID, 1, 1);
	// s3k_mon_pmp_load(MONITOR, APP0_PID, 0, 0);

	// Derive a PMP capability for app1 main memory
	s3k_cap_derive(RAM_MEM, tmp, s3k_mk_pmp(app1_addr, S3K_MEM_RWX));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, APP1_PID, 0);
	s3k_mon_pmp_load(MONITOR, APP1_PID, 0, 0);
	// s3k_mon_cap_move(MONITOR, APP0_PID, 15, APP1_PID, 2);
	// s3k_mon_pmp_load(MONITOR, APP1_PID, 15, 2);

	// Derive a PMP capability for uart
	s3k_cap_derive(UART_MEM, tmp, s3k_mk_pmp(uart_addr, S3K_MEM_RW));
	s3k_mon_cap_move(MONITOR, APP0_PID, tmp, APP1_PID, 1);
	s3k_mon_pmp_load(MONITOR, APP1_PID, 1, 1);
}

void start_app1(uint64_t tmp) {
	// derive a time slice capability
	s3k_cap_derive(HART0_TIME, tmp,
		       s3k_mk_time(S3K_MIN_HART, 0, 8));
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
				     S3K_IPC_SDATA | S3K_IPC_CDATA , 0));
	s3k_cap_derive(socket, tmp,
		       s3k_mk_socket(0, S3K_IPC_NOYIELD,
				     S3K_IPC_SDATA | S3K_IPC_CDATA , 1));
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
#define TEMP_ADDR 0x80060000
int main(void)
{	
	s3k_cap_delete(HART1_TIME);
	s3k_cap_delete(HART2_TIME);
	s3k_cap_delete(HART3_TIME);
	uint64_t uart_addr = s3k_napot_encode(UART0_BASE_ADDR, 0x2000);
	uint64_t temp_addr = s3k_napot_encode(TEMP_ADDR, 0x10000);
	while (s3k_cap_derive(2, 16, s3k_mk_pmp(uart_addr, S3K_MEM_RW)))
		;
	while (s3k_pmp_load(16, 1))
		;
	
	s3k_sync();

	while (s3k_cap_derive(RAM_MEM, 15, s3k_mk_pmp(temp_addr, S3K_MEM_RWX)))
		;
	while (s3k_pmp_load(15, 2))
		;
	s3k_sync();

	setup_app1(11);	
	setup_monitor(12);
	
	setup_socket(13, 14); // Socket is on 13 - and moved to 4
	// setup_shared(15);

		// monitor for app0, app1, monitor
	
	alt_puts("APP0: gave mon cap to monitor");

	// Order of starting these matters 💀
	
	start_app1(11);

	s3k_cap_derive(MONITOR, 17, s3k_mk_monitor(0, 2));
	s3k_mon_cap_move(MONITOR, APP0_PID, 17, MONITOR_PID, MONITOR);


	// s3k_cap_t cap;
	// s3k_err_t err = s3k_cap_read(MONITOR, &cap);
	// alt_printf("APP0: monitor cap read result %X\n", err);
	// s3k_print_cap(&cap);

	start_monitor(12);

	//s3k_mon_cap_move(MONITOR, APP0_PID, 12, APP1_PID, 3); // this does nothing
	// we dont have cap to write to app1 memory, so its chillll

	// char *app_address = (char *)APP_ADDRESS;
	// *app_address = 0;

	// is this legal? NO

	alt_puts("APP0: hello from app0");

	char* test;
	test = 0x80060000;
	*test = 'H';
	*(test+1) = 'e';
	*(test+2) = 'l';
	*(test+3) = 'l';
	*(test+4) = 'o';
	*(test+5) = ' ';
	*(test+6) = 'D';
	*(test+7) = 'r';
	*(test+8) = 'i';
	*(test+9) = 'v';
	*(test+10) = 'e';
	*(test+11) = 'r';
	
	FATFS FatFs;		/* FatFs work area needed for each volume */
	f_mount(&FatFs, "", 0);		/* Give a work area to the default drive */
	alt_puts("APP0: File system mounted");

	UINT bw;
	FRESULT fr;
	FIL Fil;			/* File object needed for each open file */


	char buffer[1024];
	fr = f_open(&Fil, "test1.txt", FA_READ);
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

	alt_puts("APP0: BEFORE REVOKE");
	s3k_cap_revoke(15);
	alt_puts("APP0: AFTER REVOKE");

	fr = f_open(&Fil, "test2.txt", FA_READ);
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

	// err = s3k_cap_read(MONITOR, &cap);
	// alt_printf("APP0: monitor cap read result %X\n", err);
	// s3k_print_cap(&cap);
	
	// alt_puts("APP0: files read and attack executed");
	// s3k_err_t ee1 = s3k_mon_suspend(MONITOR, APP1_PID);
	// alt_printf("APP0: suspend result %X\n", ee1);
	// s3k_err_t ee2 = s3k_mon_reg_write(MONITOR, APP1_PID, S3K_REG_PC, APP_ADDRESS);
	// alt_printf("APP0: write result %X\n", ee2);
	// s3k_err_t ee3 = s3k_mon_resume(MONITOR, APP1_PID);
	// alt_printf("APP0: resume result %X\n", ee3);
	alt_puts("APP0: done");
}
