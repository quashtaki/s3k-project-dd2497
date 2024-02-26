// just leave these two true (shared memory to cirvumvent potential ipc issues after immediate suspend/resume)
#define USE_SENDRECV true
#define USE_SHARED_MEMORY true

// blocks execution of file operations temporarily to illustrate quarantine
#define ENABLE_VIRTIO_QUEUE true

// 1, 2, 3 - 1 = P Parent cap, 2 = P Child cap, 3 = P Independent cap
#define MEM_MODE 1

// true = PMP CAP, false = MEM CAP
#define PMP_CAP true