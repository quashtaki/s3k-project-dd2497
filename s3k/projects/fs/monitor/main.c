#include "altc/altio.h"
#include "altc/string.h"
#include "s3k/s3k.h"
#include "disk.h"
#include "../conf.h"

#define APP0_PID 0
#define APP1_PID 1
#define MONITOR_PID 2

// See plat_conf.h
// #define BOOT_PMP 0
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
#define APP_ADDRESS_SECOND 0x80050000
#define BUFFER_SIZE 512
#define NUM 8

struct quarantine {
	int queue_count;
	struct patient *first;
	struct patient *last;
};

struct patient {
	s3k_cap_t *cap;
	s3k_cidx_t cap_index;
	s3k_pid_t pid;
	struct patient *next;
};

uint64_t pow_own(int base, int p) {
  uint64_t temp = base;

  for (int i = 1; i < p; i++) {
    temp *= base;
  }

  return temp;
}

uint64_t charhex_to_hex(char* c_str, int unsign, int c) {
  int count = c;
  uint64_t temp = 0;
	size_t temp_st = 0;

  // for (int i = 2; i < c; i++) {
  //   count++;
  // }
	alt_printf("C COUNT %x, %x\n", c, count);

  char naive[22] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', 'a', 'b', 'c', 'd', 'e', 'f'};
  int naive_val[22] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 10, 11, 12, 13, 14, 15};
  int index;
  int count_two = count - 1;

  for (int i = 2; i < c; i++) {
    int ascii_val = *(c_str + i);

    if (ascii_val > 47 && ascii_val < 58) {
      index = ascii_val - 48;
    }

    if (ascii_val > 64 && ascii_val < 71) {
      index = ascii_val - 65;
    }

    if (ascii_val > 96 && ascii_val < 103) {
      index = ascii_val - 97;
    }

		if (unsign) {
			temp_st += naive_val[index] * pow_own(16, count_two--);
		} else {
			temp += naive_val[index] * pow_own(16, count_two--);
		}
    
  }

  return unsign ? temp_st : temp;
}

#define TAG_BLOCK_TO_ADDR(tag, block) ( \
					(((uint64_t) tag) << S3K_MAX_BLOCK_SIZE) + \
					(((uint64_t) block) << S3K_MIN_BLOCK_SIZE) \
					)

void print_memory_range(s3k_cap_t *cap) {
	uint64_t begin = TAG_BLOCK_TO_ADDR(cap->mem.tag, cap->mem.bgn);
	uint64_t end = TAG_BLOCK_TO_ADDR(cap->mem.tag, cap->mem.end);
	alt_printf("MONITOR: CAP in range begin %X end %X\n", begin, end);
}

void print_pmp_range(s3k_cap_t *cap) {
	uint64_t begin;
	uint64_t size;
	s3k_napot_decode(cap->pmp.addr, &begin, &size);
	uint64_t end = begin + size;
	alt_printf("MONITOR: CAP in range begin %X end %X\n", begin, end);
}

bool isin_memory(s3k_cap_t *cap, uint64_t addr) {
	uint64_t begin = TAG_BLOCK_TO_ADDR(cap->mem.tag, cap->mem.bgn);
	uint64_t end = TAG_BLOCK_TO_ADDR(cap->mem.tag, cap->mem.end);
	alt_printf("MONITOR: MEMORY check address %X begin %X end %X\n", addr, begin, end);
	return (addr >= begin && addr < end);
}

bool isin_pmp(s3k_cap_t *cap, uint64_t addr) {
	uint64_t begin;
	uint64_t size;
	s3k_napot_decode(cap->pmp.addr, &begin, &size);
	uint64_t end = begin + size;
	alt_printf("MONITOR: PMP check address %X begin: %X end %X\n", addr, begin, end);
	return (addr >= begin && addr < end);
}

bool check_memory(s3k_cap_t *cap, uint64_t addr) {
	if (cap->type == S3K_CAPTY_MEMORY) {
		alt_puts("MONITOR: MEMORY CAP");
		return isin_memory(cap, addr);
	} else if (cap->type == S3K_CAPTY_PMP) {
		alt_puts("MONITOR: PMP CAP");
		return isin_pmp(cap, addr);
	}
}

void add_cap_to_quarantine(struct quarantine *q, s3k_cap_t *cap, s3k_pid_t pid, s3k_cidx_t cap_index) {
	alt_puts("MONITOR: Adding item to quarantine for future removal");
	struct patient p;
	p.cap = cap;
	p.cap_index = cap_index;
	p.pid = pid;
	p.next = q->first;
	check_memory(p.cap, 0x80010000);

	if (q->first == NULL) {
		p.next = &p;
		q->first = &p;
		q->last = &p;
		q->queue_count += 1;
		alt_printf("%x\n", p.next);
		return;
	}

	// check for dupes
	struct patient *temp;
	temp = q->first;

	do {
		if (cap == temp->cap) {
			alt_puts("MONITOR: Capability is already in quarantine for future processing");
			return;
		}

		temp = temp->next;
	} while (temp != q->last);

	q->last->next = &p;
	q->last = &p;
	q->queue_count += 1;
}

void iterate_queue_revoke_caps(struct quarantine *q, struct disk *virt_queues) {
	struct patient *first = q->first;
	struct patient *prev;
	struct patient *temp;
	int last;

	for (int i = 0; i < NUM; i++) {
		if (i > 0 && last == virt_queues->avail->ring[i]) {
			break;
		}

		int index = virt_queues->avail->ring[i];
		uint64_t desc_addrs[3];

		desc_addrs[0] = virt_queues->desc[index].addr;
		index = virt_queues->desc[index].next;
		desc_addrs[1] = virt_queues->desc[index].addr;
		index = virt_queues->desc[index].next;
		desc_addrs[2] = virt_queues->desc[index].addr;

		temp = first;
		alt_printf("MONITOR: Iterating quarantine holding %x items\n", q->queue_count);
		int j = 0;
		do {
			alt_printf("MONITOR: Item # %x\n", ++j);
			s3k_cap_t *cap = temp->cap;
			s3k_cidx_t cap_index = temp->cap_index;
			s3k_pid_t pid = temp->pid;

			// three descriptors for rw operations
			bool in_range = false;
			for (int k = 0; k < 3; k++) {
				in_range = check_memory(cap, desc_addrs[k]);

				if (in_range) {
					alt_printf("MONITOR: Descriptor #%x is in range\n", k+1);

					// 1 descriptor already in range - could break cap may not be removed
				} else {
					alt_printf("MONITOR: Descriptor #%x is NOT in range\n", k+1);
				}
			}

			if (!in_range) {
				alt_puts("MONITOR: CAP NOT IN RANGE - may be removed");
				s3k_mon_suspend(MONITOR, pid);
				s3k_mon_cap_move(MONITOR, pid, cap_index, MONITOR_PID, 30);
				s3k_err_t err = s3k_cap_revoke(30);
				s3k_mon_resume(MONITOR, pid);
				// shut down driver...

				if (j == 1) {
					if (q->queue_count == 1) {
						q->first = NULL;
						q->last = NULL;
						break;
					} else {
						q->first = temp->next;
					}
				} else if (j == q->queue_count) {
					prev->next = q->first;
					q->last = prev;
				} else {
					prev->next = temp->next;
				}

				q->queue_count -= 1;
			} else {
				alt_puts("MONITOR: CAP IN RANGE - may not be removed");
			}

			prev = temp;
			temp = temp->next;
		} while (temp != first);

		last = virt_queues->avail->ring[i];
	}
}

bool quarantine_has_queue(struct quarantine *q) {
	return q->first && q->last;
}

void setup_app1(uint64_t tmp)
{
	uint64_t uart_addr = s3k_napot_encode(UART0_BASE_ADDR, 0x8);
	uint64_t app1_addr = s3k_napot_encode(APP_ADDRESS, 0x10000);

	// Derive a PMP capability for app1 main memory
	s3k_cap_derive(RAM_MEM, tmp, s3k_mk_pmp(app1_addr, S3K_MEM_RWX));
	s3k_mon_cap_move(MONITOR, MONITOR_PID, tmp, APP1_PID, 0);
	s3k_mon_pmp_load(MONITOR, APP1_PID, 0, 0);

	// Derive a PMP capability for uart
	s3k_cap_derive(UART_MEM, tmp, s3k_mk_pmp(uart_addr, S3K_MEM_RW));
	s3k_mon_cap_move(MONITOR, MONITOR_PID, tmp, APP1_PID, 1);
	s3k_mon_pmp_load(MONITOR, APP1_PID, 1, 1);
}

void start_app1(uint64_t tmp) {
	// derive a time slice capability
	// s3k_cap_derive(HART0_TIME, tmp,
	// 	       s3k_mk_time(S3K_MIN_HART, 0, 8)); // S3K_MIN_HART
	// s3k_mon_cap_move(MONITOR, MONITOR_PID, tmp, APP1_PID, 2);

	// Write start PC of app1 to PC
	s3k_mon_reg_write(MONITOR, APP1_PID, S3K_REG_PC, APP_ADDRESS);

	// Start app1
	s3k_mon_resume(MONITOR, APP1_PID);
}

void setup_socket(uint64_t socket, uint64_t tmp, uint64_t tmp1)
{
	s3k_cap_derive(CHANNEL, socket,
		       s3k_mk_socket(0, S3K_IPC_NOYIELD,
				     S3K_IPC_SDATA | S3K_IPC_CDATA , 0));
	s3k_cap_derive(socket, tmp,
		       s3k_mk_socket(0, S3K_IPC_NOYIELD,
				     S3K_IPC_SDATA | S3K_IPC_CDATA , 1));
	s3k_cap_derive(socket, tmp1,
		       s3k_mk_socket(0, S3K_IPC_NOYIELD,
				     S3K_IPC_SDATA | S3K_IPC_CDATA , 1));
	s3k_mon_cap_move(MONITOR, MONITOR_PID, tmp, APP0_PID, 14); // give app0 client
	s3k_mon_cap_move(MONITOR, MONITOR_PID, tmp1, APP1_PID, 14); // give app1 client
}

int main(void)
{
	// s3k_cap_delete(HART1_TIME);
	// s3k_cap_delete(HART2_TIME);
	// s3k_cap_delete(HART3_TIME);
	
	alt_puts("MONITOR: Monitor started");

	// quarantine (queue) for caps that may not be deleted at this time given entries in virtqueues
	struct quarantine quarantine_queue;
	quarantine_queue.queue_count = 0;
	quarantine_queue.first = NULL;
	quarantine_queue.last = NULL;

	s3k_msg_t msg;
	s3k_reply_t reply;
	s3k_err_t err;
	char* address_data;
	address_data = (char*)0x80099900;
	s3k_addr_t base_addr;
	size_t range;
	struct disk *disk_ptr = (struct disk*)0x80098000;

	// give app0 driver range sliced from parent cap
	s3k_mon_suspend(MONITOR, 0);
	uint64_t driver_addr = s3k_napot_encode(DRIVER_ADDRESS, 0x10000);
	s3k_cap_derive(RAM_MEM, 21, s3k_mk_pmp(driver_addr, S3K_MEM_RWX));
	s3k_cap_derive(21, 22, s3k_mk_pmp(driver_addr, S3K_MEM_RWX));
	s3k_mon_cap_move(MONITOR, MONITOR_PID, 22, APP0_PID, 0); // cap 5
	s3k_mon_pmp_load(MONITOR, APP0_PID, 0, 3);
	s3k_mon_resume(MONITOR, 0);

	// give app1 mem range it is supposed to be allowed to remove
	uint64_t app1_addr_second = s3k_napot_encode(APP_ADDRESS_SECOND, 0x10000);
	s3k_cap_derive(RAM_MEM, 23, s3k_mk_pmp(app1_addr_second, S3K_MEM_RWX));
	s3k_mon_cap_move(MONITOR, MONITOR_PID, 23, APP1_PID, 5); // cap 5
	s3k_mon_pmp_load(MONITOR, APP1_PID, 5, 2);

	// setup_app1(12);
	start_app1(12);
	// setup_socket(14, 15, 16); // socket on 14, move respective clients to 14 too

	while (1) {
		alt_puts("MONITOR: waiting for req");

		do {
			reply = s3k_sock_recv(14,0);
			if (reply.err == S3K_ERR_TIMEOUT)
				alt_puts("MONITOR: timeout");
		} while (reply.err);
		alt_puts("MONITOR: received");

		alt_printf("MONITOR: MSG DATA %X %X %X %X\n", reply.data[0], reply.data[1], reply.data[2], reply.data[3]);

		// this block just for debugging purposes
		if (reply.data[1] == 84 && reply.data[2] == 84) {
			// check all caps
			alt_puts("MONITOR: CHECK ALL CAPS");
			s3k_pid_t pid = reply.data[0];
			s3k_cidx_t cap_index = reply.data[3];

			// check memory - suspend process meanwhile to access cap
			uint64_t pc;
			// s3k_mon_reg_read(MONITOR_PID, pid, S3K_REG_PC, &pc);
			s3k_err_t err = s3k_mon_suspend(MONITOR, pid);

			for (int i = 0; i < 32; i++) {
				s3k_cap_t cap;
				err = s3k_mon_cap_read(MONITOR, pid, i, &cap);
				alt_printf("PID: %x | CAP index: %x | CAP type %x\n", pid, i, cap.type);

				if (cap.type == S3K_CAPTY_MEMORY) {
					print_memory_range(&cap);
				} else if (cap.type == S3K_CAPTY_PMP) {
					print_pmp_range(&cap);
				}
				// check_memory(&cap, 0x80010000);
			}
			
			// alt_printf("PC: %x\n", pc);
			// s3k_mon_reg_write(MONITOR, pid, S3K_REG_PC, pc+1);
			err = s3k_mon_resume(MONITOR, pid);

			// msg.data[0] = 44;
			// msg.data[1] = 44;
			// msg.data[2] = 44;
			// msg.data[3] = 44;
			// s3k_reg_write(S3K_REG_SERVTIME, 4500);
			// do {
			// 	err = s3k_sock_send(14, &msg);
			// } while (err != 0);
		}

		if (reply.data[1] == 94 && reply.data[2] == 94) {
			alt_puts("MONITOR: PROCESSING REQUEST TO REVOKE CAP");
			s3k_pid_t pid = reply.data[0];
			s3k_cidx_t cap_index = reply.data[3];

			alt_printf("MONITOR descriptor 0 addr: %x | len/flags/next %x - %x - %x\n", disk_ptr->desc[0].addr, disk_ptr->desc[0].len, disk_ptr->desc[0].flags, disk_ptr->desc[0].next);
			alt_printf("MONITOR descriptor 1 addr: %x | len/flags/next %x - %x - %x\n", disk_ptr->desc[1].addr, disk_ptr->desc[1].len, disk_ptr->desc[1].flags, disk_ptr->desc[1].next);
			alt_printf("MONITOR descriptor 2 addr: %x | len/flags/next %x - %x - %x\n", disk_ptr->desc[2].addr, disk_ptr->desc[2].len, disk_ptr->desc[2].flags, disk_ptr->desc[2].next);
			// alt_printf("MONITOR avail %x\n", disk_ptr->avail->ring[0]);

			// addresses 0 indicates no entries in queue
			bool virt_empty = false;
			uint64_t addrs[] = {disk_ptr->desc[0].addr, disk_ptr->desc[1].addr, disk_ptr->desc[2].addr};
			if (addrs[0] == 0 && addrs[1] == 0 && addrs[2] == 0) {
				alt_puts("MONITOR: VIRTQUEUE EMPTY (driver side) - cap removal not restricted");
				virt_empty = true;
			}

			s3k_err_t err = s3k_mon_suspend(MONITOR, pid);
			s3k_cap_t cap;

			err = s3k_mon_cap_read(MONITOR, pid, cap_index, &cap);
			alt_printf("MONITOR: Revoke request of PID: %x | CAP index: %x | CAP type %x\n", pid, cap_index, cap.type);

			if (cap.type == S3K_CAPTY_MEMORY) {
				print_memory_range(&cap);
			} else if (cap.type == S3K_CAPTY_PMP) {
				print_pmp_range(&cap);
			}

			bool status = false;
			for (int i = 0; i < 3; i++) {
				status = check_memory(&cap, addrs[i]);

				if (status) {
					break;
				}
			}
			if (status && !virt_empty) {
				alt_puts("MONITOR: INSIDE MEMORY - CANNOT REVOKE");
				add_cap_to_quarantine(&quarantine_queue, &cap, pid, cap_index);
			} else {
				alt_puts("MONITOR: NOT IN MEMORY OR VIRT QUEUE EMPTY - CAN REVOKE");
				alt_puts("MONITOR: Moving cap temporarily to slot 30 to revoke");
				// revoke cap - currently let monitor move cap and revoke it instead of kernel communicate and revoke/return error
				s3k_mon_cap_move(MONITOR, pid, cap_index, MONITOR_PID, 30);
				err = s3k_cap_revoke(30);
				// shut down driver if in driver range
			}
			// alt_printf("PC: %x\n", pc);
			// s3k_mon_reg_write(MONITOR, pid, S3K_REG_PC, pc);
			err = s3k_mon_resume(MONITOR, pid);
			
			if (USE_SENDRECV) {
				msg.data[0] = 77;
				msg.data[1] = 77;
				msg.data[2] = 77;
				msg.data[3] = 77;
				s3k_reg_write(S3K_REG_SERVTIME, 4500);
				do {
					err = s3k_sock_send(14, &msg);
				} while (err != 0);
			}			
		}

		if (quarantine_has_queue(&quarantine_queue)) {
			alt_puts("MONITOR: Quarantine has queue - iterating over attempting to revoke caps");

			iterate_queue_revoke_caps(&quarantine_queue, disk_ptr);
			alt_puts("MONITOR: Done iterating queue");
		} else {
			alt_puts("MONITOR: Quarantine has NO queue");
		}

		// alt_puts("MONITOR: sent");
	}
}