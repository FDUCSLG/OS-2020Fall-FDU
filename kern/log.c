#include "types.h"
#include "console.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "string.h"

/* Simple logging that allows concurrent FS system calls.
 *
 * A log transaction contains the updates of multiple FS system
 * calls. The logging system only commits when there are
 * no FS system calls active. Thus there is never
 * any reasoning required about whether a commit might
 * write an uncommitted system call's updates to disk.
 *
 * A system call should call begin_op()/end_op() to mark
 * its start and end. Usually begin_op() just increments
 * the count of in-progress FS system calls and returns.
 * But if it thinks the log is close to running out, it
 * sleeps until the last outstanding end_op() commits.
 *
 * The log is a physical re-do log containing disk blocks.
 * The on-disk log format:
 *   header block, containing block #s for block A, B, C, ...
 *   block A
 *   block B
 *   block C
 *   ...
 * Log appends are synchronous.
 */

/*
 * Contents of the header block, used for both the on-disk header block
 * and to keep track in memory of logged block# before commit.
 */
struct logheader {
    int n;
    int block[LOGSIZE];
};

struct log {
    struct spinlock lock;
    int start;
    int size;
    int outstanding;    // How many FS sys calls are executing.
    int committing;     // In commit(), please wait.
    int dev;
    struct logheader lh;
};
struct log log;

static void recover_from_log();
static void commit();

void
initlog(int dev)
{
    /* TODO: Your code here. */
}

/* Copy committed blocks from log to their home location. */
static void
install_trans()
{
    /* TODO: Your code here. */
}

/* Read the log header from disk into the in-memory log header. */
static void
read_head()
{
    /* TODO: Your code here. */
}

/*
 * Write in-memory log header to disk.
 * This is the true point at which the
 * current transaction commits.
 */
static void
write_head()
{
    struct buf *buf = bread(log.dev, log.start);
    struct logheader *hb = (struct logheader *) (buf->data);
    int i;
    hb->n = log.lh.n;
    for (i = 0; i < log.lh.n; i++) {
        hb->block[i] = log.lh.block[i];
    }
    bwrite(buf);
    brelse(buf);
}

static void
recover_from_log()
{
    /* TODO: Your code here. */
}

/* Called at the start of each FS system call. */
void
begin_op()
{
    /* TODO: Your code here. */
}

/*
 * Called at the end of each FS system call.
 * Commits if this was the last outstanding operation.
 */
void
end_op()
{
    /* TODO: Your code here. */
}

/* Copy modified blocks from cache to log. */
static void
write_log()
{
    /* TODO: Your code here. */
}

static void
commit()
{
    /* TODO: Your code here. */
}

/* Caller has modified b->data and is done with the buffer.
 * Record the block number and pin in the cache with B_DIRTY.
 * commit()/write_log() will do the disk write.
 *
 * log_write() replaces bwrite(); a typical use is:
 *   bp = bread(...)
 *   modify bp->data[]
 *   log_write(bp)
 *   brelse(bp)
 */
void
log_write(struct buf *b)
{
    /* TODO: Your code here. */
}

