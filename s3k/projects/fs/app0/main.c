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


char trap_stack[1024];
void trap_handler(void) __attribute__((interrupt("machine")));

void trap_handler(void)
{
	// We enter here on illegal instructions, for example writing to
	// protected area (UART).

	// On an exception we do
	// - tf.epc = tf.pc (save program counter)
	// - tf.pc = tf.tpc (load trap handler address)
	// - tf.esp = tf.sp (save stack pointer)
	// - tf.sp = tf.tsp (load trap stack pointer)
	// - tf.ecause = mcause (see RISC-V privileged spec)
	// - tf.eval = mval (see RISC-V privileged spec)
	// tf is the trap frame, all registers of our process
	uint64_t epc = s3k_reg_read(S3K_REG_EPC);
	uint64_t esp = s3k_reg_read(S3K_REG_ESP);
	uint64_t ecause = s3k_reg_read(S3K_REG_ECAUSE);
	uint64_t eval = s3k_reg_read(S3K_REG_EVAL);

	alt_printf(
	    "error info:\n- epc: 0x%x\n- esp: 0x%x\n- ecause: 0x%x\n- eval: 0x%x\n",
	    epc, esp, ecause, eval);
	alt_printf("restoring pc and sp\n\n");
}

void setup_trap(void)
{
	// Sets the trap handler
	s3k_reg_write(S3K_REG_TPC, (uint64_t)trap_handler);
	// Set the trap stack
	s3k_reg_write(S3K_REG_TSP, (uint64_t)trap_stack + 1024);
}


int main(void)
{	

	//setup_trap();
	//s3k_mon_cap_move(MONITOR, APP0_PID, 11, APP1_PID, 3); // move out capability to app1
	
	FATFS FatFs;		/* FatFs work area needed for each volume */
	f_mount(&FatFs, "", 0);		/* Give a work area to the default drive */
	alt_puts("APP0: File system mounted");

	UINT bw;
	FRESULT fr;
	FIL Fil;			/* File object needed for each open file */
	
	char buffer[1024];
	
	fr = f_open(&Fil, "app2.bin", FA_READ);
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
