#include "../app0/types.h"
#include "../app0/buf.h"
#include "../app0/virtio.h"
#include <string.h>
#include "altc/altio.h"
#include "s3k/s3k.h"
#include "setup_queue.h"

#define PGSIZE 4096
#define PGSHIFT 12  // bits of offset within a page
// virtio mmio interface
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

#define SHARED_MEM 0x80050000
#define SHARED_MEM_LEN 0x10000

#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

struct disk *disk = (struct disk *) SHARED_MEM;

void test(void) {
    alt_puts("test\n");
    alt_printf("%X", disk);
    
}

// find a free descriptor, mark it non-free, return its index.
static int
alloc_desc()
{
  for(int i = 0; i < NUM; i++){
    if(disk->free[i]){
      disk->free[i] = 0;
      return i;
    }
  }
  return -1;
}

// mark a descriptor as free.
static void
free_desc(int i)
{
  if(i >= NUM) {
    alt_puts("free_desc 1");
    return;
  }
  if(disk->free[i]) {
    alt_puts("free_desc 2");
    return;
  }
  disk->desc[i].addr = 0;
  disk->desc[i].len = 0;
  disk->desc[i].flags = 0;
  disk->desc[i].next = 0;
  disk->free[i] = 1;
  /* wakeup(&disk->free[0]); */
}

// free a chain of descriptors.
static void
free_chain(int i)
{
  while(1){
    int flag = disk->desc[i].flags;
    int nxt = disk->desc[i].next;
    free_desc(i);
    if(flag & VRING_DESC_F_NEXT)
      i = nxt;
    else
      break;
  }
}

// allocate three descriptors (they need not be contiguous).
// disk transfers always use three descriptors.
static int
alloc3_desc(int *idx)
{
  for(int i = 0; i < 3; i++){
    idx[i] = alloc_desc();
    if(idx[i] < 0){
      for(int j = 0; j < i; j++)
        free_desc(idx[j]);
      return -1;
    }
  }
  return 0;
}

void
initialize(void)
{
  alt_puts("DISK AT:");
  alt_printf("%X\n", disk);
  uint32 status = 0;
  /* initlock(&disk->vdisk_lock, "virtio_disk"); */
  if(*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
     *R(VIRTIO_MMIO_VERSION) != 1 ||
     *R(VIRTIO_MMIO_DEVICE_ID) != 2 ||
     *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551){
    alt_puts("could not find virtio disk");
  }

  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R(VIRTIO_MMIO_STATUS) = status;

  status |= VIRTIO_CONFIG_S_DRIVER;
  *R(VIRTIO_MMIO_STATUS) = status;
  // negotiate features
  uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
  features &= ~(1 << VIRTIO_BLK_F_RO);
  features &= ~(1 << VIRTIO_BLK_F_SCSI);
  features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
  features &= ~(1 << VIRTIO_BLK_F_MQ);
  features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
  features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
  features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
  *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;
  // tell device that feature negotiation is complete.
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS) = status;
  // tell device we're completely ready.
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  *R(VIRTIO_MMIO_GUEST_PAGE_SIZE) = PGSIZE;

  // initialize queue 0.
  *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
  uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if(max == 0) {
    alt_puts("virtio disk has no queue 0");
    return;
  }
  if(max < NUM) {
    alt_puts("virtio disk max queue too short");
    return;
  }
  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;
  memset(disk->pages, 0, sizeof(disk->pages));
  *R(VIRTIO_MMIO_QUEUE_PFN) = ((uint64)disk->pages) >> PGSHIFT;
  // desc = pages -- num * virtq_desc
  // avail = pages + 0x40 -- 2 * uint16, then num * uint16
  // used = pages + 4096 -- 2 * uint16, then num * vRingUsedElem

  disk->desc = (struct virtq_desc *) disk->pages;
  disk->avail = (struct virtq_avail *)(disk->pages + NUM*sizeof(struct virtq_desc));
  disk->used = (struct virtq_used *) (disk->pages + PGSIZE);

  // all NUM descriptors start out unused.
  for(int i = 0; i < NUM; i++)
    disk->free[i] = 1;

  // plic.c and trap.c arrange for interrupts from VIRTIO0_IRQ.
  disk->initialised = 1;
  alt_puts("Disk Initialization completed\n");
}