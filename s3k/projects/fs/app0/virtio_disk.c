//
// driver for qemu's virtio disk device.
// uses qemu's mmio interface to virtio.
// qemu presents a "legacy" virtio interface.
//
// qemu ... -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
//

#include "types.h"
/* #include "defs.h" */
/* #include "param.h" */
/* #include "memlayout.h" */
/* #include "spinlock.h" */
/* #include "sleeplock.h" */
/* #include "fs.h" */
#include "buf.h"
#include "virtio.h"
#include <string.h>
#include "altc/altio.h"
#include "virtio_disk.h"
#include "s3k/s3k.h"
#include "../monitor/setup_queue.h"

#define PGSIZE 4096
#define PGSHIFT 12  // bits of offset within a page
// virtio mmio interface
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

#define TOGGLE_ADDRESS 0x80030000 - 1
#define DISK_ADDRESS 0x80050000

// set up pages global in monitor to shift where its read from

int virtio_disk_status() {
  struct disk *disk = (struct disk *) DISK_ADDRESS;
  return disk->initialised;
}

void
virtio_disk_rw(struct buf *b, int write)
{
  alt_puts("VIRTIO_DISK: Checking with monitor...");
  s3k_msg_t msg;
  //set data
  msg.data[0] = (uint64_t) b;
  msg.data[1] = (uint64_t) write;

  volatile char *toggle = (char*) TOGGLE_ADDRESS;
  s3k_reply_t reply;
  s3k_err_t err;
  s3k_reg_write(S3K_REG_SERVTIME, 4500);
  *toggle = 1;
  do {
			err = s3k_sock_send(4, &msg);
      //alt_printf("VIRTIO_DISK: reply.err: %X\n", err);
		} while (err != 0);
  // instead of a waiting bit we should be able to look at the data structure like in the other one
  while(*toggle == 1) {}
  alt_puts("VIRTIO_DISK: virtio_disk_rw done");
}