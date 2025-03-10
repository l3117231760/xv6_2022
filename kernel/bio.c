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
#define NBUCKET 7
// struct {
//   struct spinlock lock;
//   struct buf buf[NBUF];

//   // Linked list of all buffers, through prev/next.
//   // Sorted by how recently the buffer was used.
//   // head.next is most recent, head.prev is least.
//   struct buf head;
// } bcache;
struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
} bcache[NBUCKET];

// void
// binit(void)
// {
//   struct buf *b;

//   initlock(&bcache.lock, "bcache");

//   // Create linked list of buffers
//   bcache.head.prev = &bcache.head;
//   bcache.head.next = &bcache.head;
//   for(b = bcache.buf; b < bcache.buf+NBUF; b++){
//     b->next = bcache.head.next;
//     b->prev = &bcache.head;
//     initsleeplock(&b->lock, "buffer");
//     bcache.head.next->prev = b;
//     bcache.head.next = b;
//   }
// }

void
binit(void)
{
  struct buf *b;
  for(int i = 0;i<NBUCKET;i++)
  {
    char name[64];
    snprintf(name,sizeof name,"bcache_%d",i);
    initlock(&bcache->lock,name);
    for(b = bcache[i].buf; b < bcache[i].buf+NBUF; b++)
    {
      initsleeplock(&b->lock, "buffer");
      b->refcnt = 0;
      b->nticks = 0;
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
// static struct buf*
// bget(uint dev, uint blockno)
// {
//   struct buf *b;

//   acquire(&bcache.lock);

//   // Is the block already cached?
//   for(b = bcache.head.next; b != &bcache.head; b = b->next){
//     if(b->dev == dev && b->blockno == blockno){
//       b->refcnt++;
//       release(&bcache.lock);
//       acquiresleep(&b->lock);
//       return b;
//     }
//   }

//   // Not cached.
//   // Recycle the least recently used (LRU) unused buffer.
//   for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
//     if(b->refcnt == 0) {
//       b->dev = dev;
//       b->blockno = blockno;
//       b->valid = 0;
//       b->refcnt = 1;
//       release(&bcache.lock);
//       acquiresleep(&b->lock);
//       return b;
//     }
//   }
//   panic("bget: no buffers");
// }
// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int bucket_idx = blockno % NBUCKET;
  acquire(&bcache[bucket_idx].lock);

  int min_idx = -1;
  uint min_ticks = __UINT32_MAX__;

  for(int i = 0; i < NBUF; i++){
    b = bcache[bucket_idx].buf + i;
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache[bucket_idx].lock);
      acquiresleep(&b->lock);
      return b;
    }
    else {
      // printf("refcnt: %d, ntick: %d\n", b->refcnt, b->nticks);
      if (b->refcnt == 0 && b->nticks < min_ticks) {
        min_idx = i;
        min_ticks = b->nticks;
      }
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // 如果不存在对应的cache且找到可用buffer
  if (min_idx != -1) {
    b = bcache[bucket_idx].buf + min_idx;
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    release(&bcache[bucket_idx].lock);
    acquiresleep(&b->lock);
    return b;
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
// void
// brelse(struct buf *b)
// {
//   if(!holdingsleep(&b->lock))
//     panic("brelse");

//   releasesleep(&b->lock);

//   acquire(&bcache.lock);
//   b->refcnt--;
//   if (b->refcnt == 0) {
//     // no one is waiting for it.
//     b->next->prev = b->prev;
//     b->prev->next = b->next;
//     b->next = bcache.head.next;
//     b->prev = &bcache.head;
//     bcache.head.next->prev = b;
//     bcache.head.next = b;
//   }
  
//   release(&bcache.lock);
// }
// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bucket_idx = b->blockno % NBUCKET;
  acquire(&bcache[bucket_idx].lock);
  b->refcnt--;
  // 如果引用为0，释放掉该buffer
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // 将该buffer移到头节点
    b->nticks = ticks;
  }
  
  release(&bcache[bucket_idx].lock);
}

// void
// bpin(struct buf *b) {
//   acquire(&bcache.lock);
//   b->refcnt++;
//   release(&bcache.lock);
// }

// void
// bunpin(struct buf *b) {
//   acquire(&bcache.lock);
//   b->refcnt--;
//   release(&bcache.lock);
// }
void
bpin(struct buf *b) {
  int bucket_idx = b->blockno % NBUCKET;
  acquire(&bcache[bucket_idx].lock);
  b->refcnt++;
  release(&bcache[bucket_idx].lock);
}

void
bunpin(struct buf *b) {
  int bucket_idx = b->blockno % NBUCKET;
  acquire(&bcache[bucket_idx].lock);
  b->refcnt--;
  release(&bcache[bucket_idx].lock);
}



