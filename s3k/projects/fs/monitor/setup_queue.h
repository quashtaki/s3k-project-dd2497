#include "../app0/types.h"
#include "../app0/buf.h"
#include "../app0/virtio.h"
#include "altc/altio.h"
#include "s3k/s3k.h"
#ifndef SETUP_QUEUE_H
#define SETUP_QUEUE_H

void initialize(void);
void read_write(struct buf *b, int write);

#define PGSIZE 4096
#define PGSHIFT 12 

struct disk {
  char pages[2*PGSIZE];
  struct virtq_desc *desc;
  struct virtq_avail *avail;
  struct virtq_used *used;
  char free[NUM];
  uint16 used_idx;
  struct {
    struct buf *b;
    char status;
  } info[NUM];
  struct virtio_blk_req ops[NUM];
  int initialised;
} __attribute__ ((aligned (PGSIZE)));

#endif // SETUP_QUEUE_H