#include "altc/altio.h"
#include "altc/string.h"
#include "s3k/s3k.h"
#include "test.h"
#include "../app0/types.h"
#include "../app0/buf.h"
#include "../app0/virtio.h"

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
#define SHARED_MEM_LEN 0x10000

#define PGSIZE 4096
#define PGSHIFT 12  // bits of offset within a page
// virtio mmio interface
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1
void test_call(void){
    alt_puts("I am called!!");
}

static struct disk {

  char pages[2*PGSIZE];

  struct virtq_desc *desc;

  struct virtq_avail *avail;

  struct virtq_used *used;

  // our own book-keeping.
  char free[8];  // is a descriptor free?
  uint16 used_idx; // we've looked this far in used[2..NUM].

  struct {
    struct buf *b;
    char status;
  } info[8];

  struct virtio_blk_req ops[8];

  int initialised;
} __attribute__ ((aligned (PGSIZE))) disk;

#define SHARED_MEM 0x80050000
#define SHARED_MEM_LEN 0x10000
#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

void queue_build(struct buf *b, int write)
{
  alt_puts("I am called and now build the queue");
  uint64 sector = b->blockno * (BSIZE / 512);

  // format the three descriptors.
  // qemu's virtio-blk.c reads them.
  int idx[3];
  struct virtio_blk_req *buf0 = &disk.ops[idx[0]];

  uint64 output = (uint64) b->data;
  if (sector == 49) {
    output = (uint64) 0x0000000080030000;
  }
    
  // DECIDE IF ADD TO QUEUE OR NOT

  //alt_puts("VIRTIO_DISK: Checked with monitor");
  
  //This should not happen here 
  //First still allow and move few operations for a while
  disk.desc[idx[1]].addr = output; // 0x00000000800200000;  // b->data; 
  disk.desc[idx[1]].len = BSIZE;
  if(write)
    disk.desc[idx[1]].flags = 0; // device reads b->data
  else
    disk.desc[idx[1]].flags = 2; // device writes b->data
  disk.desc[idx[1]].flags |= 1;
  disk.desc[idx[1]].next = idx[2];

  disk.info[idx[0]].status = 0xff; // device writes 0 on success
  disk.desc[idx[2]].addr = (uint64) &disk.info[idx[0]].status;
  disk.desc[idx[2]].len = 1;
  disk.desc[idx[2]].flags = 2; // device writes the status
  disk.desc[idx[2]].next = 0;

  // record struct buf for virtio_disk_intr().
  b->disk = 1;
  disk.info[idx[0]].b = b;

  // tell the device the first index in our chain of descriptors.
  disk.avail->ring[disk.avail->idx % 8] = idx[0];

  __sync_synchronize();

  // tell the device another avail ring entry is available.
  disk.avail->idx += 1; // not % NUM ...

  __sync_synchronize();

  //*R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // value is queue number


}

