#ifndef INC_BUF_H
#define INC_BUF_H

#include <stdint.h>
#include "sleeplock.h"
#include "fs.h"

#define B_VALID 0x2     /* Buffer has been read from disk. */
#define B_DIRTY 0x4     /* Buffer needs to be written to disk. */

struct buf {
    int flags;
    uint32_t dev;
    uint32_t blockno;
    uint32_t refcnt;
    uint8_t data[BSIZE];
    struct sleeplock lock;

    /* TODO: Your code here. */

};

void        binit();
void        bwrite(struct buf *b);
void        brelse(struct buf *b);
struct buf *bread(uint32_t dev, uint32_t blockno);

#endif
