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

void setup_uart(uint64_t uart_idx)
{
	uint64_t uart_addr = s3k_napot_encode(0x10000000, 0x8);
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
	uint64_t uart_addr = s3k_napot_encode(0x10000000, 0x8);
	uint64_t app1_addr =    s3k_napot_encode(0x80020000, 0x10000);
	uint64_t app1_sp_addr = s3k_napot_encode(0x80040000, 0x10000);

	// Derive a PMP capability for app1 main memory
	alt_printf("1 %X\n",
		s3k_cap_derive(RAM_MEM, tmp, s3k_mk_pmp(app1_addr, S3K_MEM_RWX))
			   );
	alt_printf("2 %X\n",
			   s3k_mon_cap_move(MONITOR, APP0_PID, tmp, APP1_PID, 0)
			   );
	alt_printf("3 %X\n",
			   s3k_mon_pmp_load(MONITOR, APP1_PID, 0, 0)
			   );

	// Derive a PMP capability for uart
	alt_printf("4 %X\n",
			   s3k_cap_derive(UART_MEM, tmp, s3k_mk_pmp(uart_addr, S3K_MEM_RW))
			   );

	alt_printf("5 %X\n",
			   s3k_mon_cap_move(MONITOR, APP0_PID, tmp, APP1_PID, 1)
			   );
	alt_printf("6 %X\n",
			   s3k_mon_pmp_load(MONITOR, APP1_PID, 1, 1)
			   );

	alt_printf("7 %X\n",
		s3k_cap_derive(RAM_MEM, tmp, s3k_mk_pmp(app1_sp_addr, S3K_MEM_RW))
			   );
	alt_printf("8 %X\n",
			   s3k_mon_cap_move(MONITOR, APP0_PID, tmp, APP1_PID, 2)
			   );
	alt_printf("9 %X\n",
			   s3k_mon_pmp_load(MONITOR, APP1_PID, 2, 2)
			   );



	// Write start PC of app1 to PC
	alt_printf("9 %X\n",
			   s3k_mon_reg_write(MONITOR, APP1_PID, S3K_REG_PC, 0x80020000)
			   );

	alt_printf("10a: %X\n",
			   s3k_mon_cap_move(MONITOR, APP0_PID, HART0_TIME, APP1_PID, 3)
			   );

	// Start app1
	alt_printf("10 %X\n",
			   s3k_mon_resume(MONITOR, APP1_PID)
			   );


	s3k_cap_delete(HART1_TIME);
	s3k_cap_delete(HART2_TIME);
	s3k_cap_delete(HART3_TIME);


	s3k_sync();
}

int main(void)
{
	// Setup UART access
	setup_uart(10);

	// Setup app1 capabilities and PC
	setup_app1(11);

	// Write hello world.
	alt_puts("hello, world from app0");

	/* alt_printf("11: %X\n", */
	/* 		   s3k_mon_cap_move(MONITOR, APP0_PID, HART0_TIME, APP1_PID, 3) */
	/* 		   ); */


	// BYE!
	alt_puts("bye from app0");
}
