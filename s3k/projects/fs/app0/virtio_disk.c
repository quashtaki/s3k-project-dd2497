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

#define SHARED_MEM 0x80050000
#define SHARED_MEM_LEN 0x10000

// the address of virtio mmio register r.
//#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

void virtio_disk_intr(void);

// set up pages global in monitor to shift where its read from

int virtio_disk_status() {
  alt_puts("Getting status from disk - but probably failing bc no access to that mem region");
  struct disk *disk = (struct disk *) SHARED_MEM;
  return disk->initialised;
}

void
virtio_disk_rw(struct buf *b, int write)
{
  alt_puts("VIRTIO_DISK: got call from diskio");
}

// void
// virtio_disk_rw(struct buf *b, int write)
// {

  

//   uint64 sector = b->blockno * (BSIZE / 512);
//   alt_puts("VIRTIO_DISK: inside virtio_disk_rw");


//   /* acquire(&disk.vdisk_lock); */

//   // the spec's Section 5.2 says that legacy block operations use
//   // three descriptors: one for type/reserved/sector, one for the
//   // data, one for a 1-byte status result.

//   // allocate the three descriptors.
//   int idx[3];
//   while(1){
//     if(alloc3_desc(idx) == 0) {
//       break;
//     }
//     /* sleep(&disk.free[0], &disk.vdisk_lock); */
//   }

//   // format the three descriptors.
//   // qemu's virtio-blk.c reads them.

//   struct virtio_blk_req *buf0 = &disk.ops[idx[0]];

//   if(write)
//     buf0->type = VIRTIO_BLK_T_OUT; // write the disk
//   else
//     buf0->type = VIRTIO_BLK_T_IN; // read the disk
//   buf0->reserved = 0;
//   buf0->sector = sector; // Är sector för filsystemet? Eller för disk?
//   alt_printf("VIRTIO_DISK: virtio_disk_rw sector %x\n", sector);

//   disk.desc[idx[0]].addr = (uint64) buf0; // vad är detta?
//   disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
//   disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
//   disk.desc[idx[0]].next = idx[1];
  
//   // instead of setting data param in b we write straight to memory
//   // it reads 3 times but its only the last one on sector 49 that is the the data
//   uint64 output = (uint64) b->data;
//   if (sector == 49) {
//     output = (uint64) 0x0000000080030000;
//   }

//   //alt_printf("VIRTIO_DISK: virtio_disk_rw output %x\n", output);


//   // HERE WE CHECK WITH MONITOR!

//   volatile char *shared_status = (char*) SHARED_MEM; // this is important bc it optimizes otherwise
// 	char *shared_result = (char*) SHARED_MEM + 1;

//   alt_puts("VIRTIO_DISK: Checking with monitor...");
//   s3k_msg_t msg;
//   memcpy(msg.data, &output, sizeof(output));


//   s3k_reply_t reply;
//   s3k_reg_write(S3K_REG_SERVTIME, 4500);


//   *shared_status = 0; // this one could be write and read, bc we want to set it to 0 before comms to not have risk for issues
//   s3k_err_t err;


//    do {
// 			err = s3k_sock_send(4, &msg);
//       alt_printf("VIRTIO_DISK: reply.err CHANGED: %X\n", err);
// 		} while (err != 0 && *shared_status == 0);
//   alt_puts("VIRTIO_DISK: Sent to monitor");
//   while (*shared_status == 0) {}
//   alt_puts("VIRTIO_DISK: Monitor replied");
//   int result = *shared_result; // this one should only be read ofc

//   if (result == 0) {
//     alt_puts("VIRTIO_DISK: Monitor denied access to memory");
//     return;
//   } else {
//     alt_puts("VIRTIO_DISK: Monitor allowed access to memory");
//   }
    
//   // DECIDE IF ADD TO QUEUE OR NOT

//   alt_puts("VIRTIO_DISK: Checked with monitor");
  
//   disk.desc[idx[1]].addr = output;
//   disk.desc[idx[1]].len = BSIZE;
//   if(write)
//     disk.desc[idx[1]].flags = 0; // device reads b->data
//   else
//     disk.desc[idx[1]].flags = VRING_DESC_F_WRITE; // device writes b->data
//   disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
//   disk.desc[idx[1]].next = idx[2];

//   disk.info[idx[0]].status = 0xff; // device writes 0 on success
//   disk.desc[idx[2]].addr = (uint64) &disk.info[idx[0]].status;
//   disk.desc[idx[2]].len = 1;
//   disk.desc[idx[2]].flags = VRING_DESC_F_WRITE; // device writes the status
//   disk.desc[idx[2]].next = 0;

//   // record struct buf for virtio_disk_intr().
//   b->disk = 1;
//   disk.info[idx[0]].b = b;

//   // tell the device the first index in our chain of descriptors.
//   disk.avail->ring[disk.avail->idx % NUM] = idx[0];

//   __sync_synchronize();

//   // tell the device another avail ring entry is available.
//   disk.avail->idx += 1; // not % NUM ...

//   __sync_synchronize();

//   *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // value is queue number

//   // Wait for virtio_disk_intr() to say request has finished.
//   while(b->disk == 1) {
//     /* sleep(b, &disk.vdisk_lock); */
//     virtio_disk_intr();
//   }


//   disk.info[idx[0]].b = 0;
//   free_chain(idx[0]);

//   alt_puts("VIRTIO_DISK: virtio_disk_rw done");

//   /* release(&disk.vdisk_lock); */
// }

// void
// virtio_disk_intr(void)
// {
//   /* acquire(&disk.vdisk_lock); */

//   // the device won't raise another interrupt until we tell it
//   // we've seen this interrupt, which the following line does.
//   // this may race with the device writing new entries to
//   // the "used" ring, in which case we may process the new
//   // completion entries in this interrupt, and have nothing to do
//   // in the next interrupt, which is harmless.
//   *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

//   __sync_synchronize();

//   // the device increments disk.used->idx when it
//   // adds an entry to the used ring.

//   struct disk *diskPtr = (struct disk *) DISK_ADDRESS;

//   while(disk.used_idx != disk.used->idx){
//     __sync_synchronize();
//     int id = disk.used->ring[disk.used_idx % NUM].id;

//     if(disk.info[id].status != 0) {
//       alt_puts("virtio_disk_intr status");
//       return;
//     }

//     struct buf *b = disk.info[id].b;
//     b->disk = 0;   // disk is done with buf
//     /* wakeup(b); */

//     disk.used_idx += 1;
//   }

//   /* release(&disk.vdisk_lock); */
// }
