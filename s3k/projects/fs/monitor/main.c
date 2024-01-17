#include "altc/altio.h"
#include "altc/string.h"
#include "s3k/s3k.h"
#include "setup_queue.h"

#define MONITOR_PID 0
#define APP0_PID 1
#define APP1_PID 2


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

#define APP0_ADDRESS 0x80020000
#define APP1_ADDRESS 0x80030000

#define SHARED_MEM 0x80050000

#define TOGGLE_ADDRESS 0x80030000 - 1
#define DISK_ADDRESS 0x80050000

#define SHARED_MEM_LEN 0x10000

void setup_app0(uint64_t tmp)
{	s3k_err_t err;

	uint64_t uart_addr = s3k_napot_encode(UART0_BASE_ADDR, 0x8);
	uint64_t app0_addr = s3k_napot_encode(APP0_ADDRESS, 0x10000);

	// Derive a PMP capability for app0 main memory
	s3k_cap_derive(RAM_MEM, tmp, s3k_mk_pmp(app0_addr, S3K_MEM_RWX));
	s3k_mon_cap_move(MONITOR, MONITOR_PID, tmp, APP0_PID, 0);
	s3k_mon_pmp_load(MONITOR, APP0_PID, 0, 0);
	

	// Derive a PMP capability for uart
	s3k_cap_derive(UART_MEM, tmp, s3k_mk_pmp(uart_addr, S3K_MEM_RW));
	s3k_mon_cap_move(MONITOR, MONITOR_PID, tmp, APP0_PID, 1);
	s3k_mon_pmp_load(MONITOR, APP0_PID, 1, 1);
	
}

void start_app0(uint64_t tmp) {
	s3k_err_t err;
	// derive a time slice capability
	s3k_cap_derive(HART0_TIME, tmp,
		       s3k_mk_time(S3K_MIN_HART, 0, 8));
	s3k_mon_cap_move(MONITOR, MONITOR_PID, tmp, APP0_PID, 2);	
	// Write start PC of app1 to PC
	s3k_mon_reg_write(MONITOR, APP0_PID, S3K_REG_PC, APP0_ADDRESS);	
	// Start app0
	s3k_mon_resume(MONITOR, APP0_PID);
	
}

void setup_app1(uint64_t tmp)
{
	uint64_t uart_addr = s3k_napot_encode(UART0_BASE_ADDR, 0x8);
	uint64_t app1_addr = s3k_napot_encode(APP1_ADDRESS, 0x10000);

	// Derive a PMP capability for app1 main memory
	s3k_cap_derive(RAM_MEM, tmp, s3k_mk_pmp(app1_addr, S3K_MEM_RWX));
	s3k_mon_cap_move(MONITOR, MONITOR_PID, tmp, APP1_PID, 0);
	s3k_mon_pmp_load(MONITOR, APP1_PID, 0, 0);

	// Derive a PMP capability for uart
	s3k_cap_derive(UART_MEM, tmp, s3k_mk_pmp(uart_addr, S3K_MEM_RW));
	s3k_mon_cap_move(MONITOR, MONITOR_PID, tmp, APP1_PID, 1);
	s3k_mon_pmp_load(MONITOR, APP1_PID, 1, 1);
}

void start_app1(uint64_t tmp) 
{
	// derive a time slice capability
	s3k_cap_derive(HART0_TIME, tmp,
		       s3k_mk_time(S3K_MIN_HART, 8, 16));
	s3k_mon_cap_move(MONITOR, MONITOR_PID, tmp, APP1_PID, 2);

	// Write start PC of app1 to PC
	s3k_mon_reg_write(MONITOR, APP1_PID, S3K_REG_PC, APP1_ADDRESS);

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
	s3k_mon_cap_move(MONITOR, MONITOR_PID, tmp, APP0_PID, 4); // give app0 client
}

void setup_shared(uint64_t tmp)
{
	uint64_t shared_address = s3k_napot_encode(SHARED_MEM, SHARED_MEM_LEN);
	// derive a shared memory from ram🍴
	s3k_err_t err = s3k_cap_derive(RAM_MEM, tmp, s3k_mk_memory(0x80050000, 0x80060000, S3K_MEM_RW));
	alt_printf("MONITOR: shared mem derivation result %X\n", err);
	// create two pmps for shared memory
	s3k_cap_derive(tmp, 20, s3k_mk_pmp(shared_address, S3K_MEM_RW)); // TODO: this to only read 
	s3k_cap_derive(tmp, 21, s3k_mk_pmp(shared_address, S3K_MEM_R)); // We make shared memory read only for app0
	// delete mem cap
	s3k_cap_delete(tmp);
	// move pmp 1 to app0
	s3k_mon_cap_move(MONITOR, MONITOR_PID, 21, APP0_PID, 3);
	// load pmp 2 for monitor
	s3k_pmp_load(20, 3);
	// load pmp 1 for app0
	s3k_mon_pmp_load(MONITOR, APP0_PID, 3, 2);

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

bool inside_memory(s3k_cap_t *cap, uint64_t addr) {
	if ((*cap).type != S3K_CAPTY_MEMORY)
		return false;
	uint64_t bgn = TAG_BLOCK_TO_ADDR((*cap).mem.tag, (*cap).mem.bgn);
	uint64_t end = TAG_BLOCK_TO_ADDR((*cap).mem.tag, (*cap).mem.end);
	alt_printf("MONITOR: inside_memory: addr:%X bgn:%X end:%X\n", addr, bgn, end);
	return (addr >= bgn && addr < end);
}

// checks if the address is inside the capability
bool inside_cap(s3k_cap_t *cap, uint64_t addr) {
	s3k_capty_t type = (*cap).type;
	uint64_t bgn;
	uint64_t end;
	int priv;
	if (type == S3K_CAPTY_PMP) {
		uint64_t size;
		s3k_napot_decode((*cap).pmp.addr, &bgn, &size);
		end = bgn + size;
		priv = (*cap).pmp.rwx;
	} else if (type == S3K_CAPTY_MEMORY) {
		bgn = TAG_BLOCK_TO_ADDR((*cap).mem.tag, (*cap).mem.bgn);
		end = TAG_BLOCK_TO_ADDR((*cap).mem.tag, (*cap).mem.end);
		priv = (*cap).mem.rwx;
	} else {
		return false;
	}
	if (priv != S3K_MEM_W && priv != S3K_MEM_RW && priv != S3K_MEM_RWX ) {
		return false;
	}
	if (addr >= bgn && addr < end) {
		alt_printf("MONITOR: inside_cap: addr:%X bgn:%X end:%X\n", addr, bgn, end);
	}
	return (addr >= bgn && addr < end);
}

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

	s3k_cap_delete(HART1_TIME);
	s3k_cap_delete(HART2_TIME);
	s3k_cap_delete(HART3_TIME);
	uint64_t uart_addr = s3k_napot_encode(UART0_BASE_ADDR, 0x2000);
	uint64_t app0_addr = s3k_napot_encode(APP0_ADDRESS, 0x10000);
	while (s3k_cap_derive(2, 16, s3k_mk_pmp(uart_addr, S3K_MEM_RW)))
		;
	while (s3k_pmp_load(16, 1))
		;
	// i guess we set up app0 mem for ourselves here?
	while (s3k_cap_derive(RAM_MEM, 17, s3k_mk_pmp(app0_addr, S3K_MEM_RW)))
		;
	while (s3k_pmp_load(17, 2))
		;

	s3k_sync();
	alt_puts("Monitor starts");

	//setup_trap();

	setup_app0(11);
	setup_app1(12);	
	
	setup_socket(13, 14); // Socket is on 13 - and moved to 4
	setup_shared(15);

	alt_puts("MONITOR: setup done");
	initialize();

	// Order of starting these matters 💀
	
	start_app0(11);
	start_app1(12);

	s3k_reply_t reply;
	int i = 0;
	alt_puts("MONITOR: started");

	volatile char *toggle = (char*) TOGGLE_ADDRESS;
	// this loop is to start app2 after the attack
	while (i < 3) {
		alt_puts("MONITOR: awaiting request");
		do {
			reply = s3k_sock_recv(13,0);
			if (reply.err == S3K_ERR_TIMEOUT)
				alt_puts("MONITOR: timeout");
		} while (reply.err);
		alt_puts("MONITOR: received");
		s3k_mon_suspend(MONITOR, APP0_PID);
		alt_puts("MONITOR: suspended");
		struct buf *b = (struct buf *)(uintptr_t)reply.data[0];
		uint64 destination = (uint64) reply.data[1];
		int write = (int) reply.data[2]; // we cast to int

		alt_puts("MONITOR: checking memory access");

		//Checking the capability of app0
		bool result = false;
		for (int i = 0; i < 32; i++) {
			s3k_cap_t cap;
			//Derive the capability of app0
			s3k_err_t err = s3k_mon_cap_read(MONITOR, APP0_PID, i, &cap);
			//checking that the capability of app0 has the permission to access the addres 
			result = inside_cap(&cap, destination); // get the pointer
			if (result) {
				break;
			}
		}
		//result = true;
		if (!result) {
			alt_puts("MONITOR: denied access to memory");
			// we don't resume app0 as its trying to access memory it doesn't have access to
			i++;
			continue;
		}

		alt_puts("MONITOR: allowed access to memory");
		read_write(b, write, destination);
		s3k_mon_resume(MONITOR, APP0_PID);

		*toggle = 0;
		alt_puts("MONITOR: done");
	
		i++;
	}

	alt_puts("MONITOR: testing app1");

	s3k_mon_suspend(MONITOR, APP1_PID);
	s3k_mon_reg_write(MONITOR, APP1_PID, S3K_REG_PC, 0x80030000);
	s3k_mon_resume(MONITOR, APP1_PID);
}

