// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

struct bucket {
    struct spinlock lock;
    struct buf buf[NBUF];
};


struct {
  struct spinlock lock;
  struct bucket bucket[NBUCKET];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  /* struct buf head; */
} bcache;

void
binit(void)
{
  initlock(&bcache.lock, "bcache");

  for(int b=0; b<NBUCKET; b++) {
      initlock(&bcache.bucket[b].lock, "bcache");
      for(int i=0; i<NBUF; i++) {
          initsleeplock(&bcache.bucket[b].buf[i].lock, "buffer");
          /* printf("init %d %d %d\n", b, i, bcache.bucket[b].buf[i].refcnt); */
      }
  }
/*
  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  */
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint buck = blockno % NBUCKET;
  /* printf("get %d\n", blockno); */

  acquire(&bcache.bucket[buck].lock);

  // Is the block already cached?
  for(int i=0; i<NBUF; i++){
      b = &bcache.bucket[buck].buf[i];
      if(b->dev == dev && b->blockno == blockno){
          b->refcnt++;
          release(&bcache.bucket[buck].lock);
          acquiresleep(&b->lock);
          return b;
      }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(int i=0; i<NBUF; i++){
      b = &bcache.bucket[buck].buf[i];
      if(b->refcnt == 0) {
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;
          release(&bcache.bucket[buck].lock);
          acquiresleep(&b->lock);
          return b;
      }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  uint buck = b->blockno % NBUCKET;
  releasesleep(&b->lock);

  acquire(&bcache.bucket[buck].lock);
  b->refcnt--;
  /* printf("release %d %d\n", b->blockno, b->refcnt); */
  /*
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  */
  
  release(&bcache.bucket[buck].lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  /* uint buck = b->blockno % NBUCKET; */
  /* acquire(&bcache.bucket[buck].lock); */
  b->refcnt++;
  /* release(&bcache.bucket[buck].lock); */
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


