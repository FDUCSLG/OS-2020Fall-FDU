#ifndef INC_BUF_H
#define INC_BUF_H

#include <stdint.h>

#define BSIZE   512

#define B_VALID 0x2     /* Buffer has been read from disk. */
#define B_DIRTY 0x4     /* Buffer needs to be written to disk. */

struct buf {
    int flags;
    uint32_t blockno;
    uint8_t data[BSIZE];

    /* TODO: Your code here. */

};

#endif
