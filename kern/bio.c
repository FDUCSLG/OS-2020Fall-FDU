/* Buffer cache.
 *
 * The buffer cache is a linked list of buf structures holding
 * cached copies of disk block contents.  Caching disk blocks
 * in memory reduces the number of disk reads and also provides
 * a synchronization point for disk blocks used by multiple processes.
 *
 * Interface:
 * * To get a buffer for a particular disk block, call bread.
 * * After changing buffer data, call bwrite to write it to disk.
 * * When done with the buffer, call brelse.
 * * Do not use the buffer after calling brelse.
 * * Only one process at a time can use a buffer,
 *     so do not keep them longer than necessary.
 *
 * The implementation uses two state flags internally:
 * * B_VALID: the buffer data has been read from the disk.
 * * B_DIRTY: the buffer data has been modified
 *     and needs to be written to disk.
 */

#include "spinlock.h"
#include "sleeplock.h"
#include "buf.h"
#include "console.h"
#include "sd.h"
#include "fs.h"

struct {
    struct spinlock lock;
    struct buf buf[NBUF];

    // Linked list of all buffers, through prev/next.
    // head.next is most recently used.
    struct buf head;
} bcache;

/* Initialize the cache list and locks. */
void
binit()
{
    /* TODO: Your code here. */
}

/*
 * Look through buffer cache for block on device dev.
 * If not found, allocate a buffer.
 * In either case, return locked buffer.
 */
static struct buf *
bget(uint32_t dev, uint32_t blockno)
{
    /* TODO: Your code here. */

}

/* Return a locked buf with the contents of the indicated block. */
struct buf *
bread(uint32_t dev, uint32_t blockno)
{
    /* TODO: Your code here. */
}

/* Write b's contents to disk. Must be locked. */
void
bwrite(struct buf *b)
{
    /* TODO: Your code here. */
}

/*
 * Release a locked buffer.
 * Move to the head of the MRU list.
 */
void
brelse(struct buf *b)
{
    /* TODO: Your code here. */
}

