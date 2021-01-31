/* File descriptors */

#include "types.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "console.h"
#include "log.h"

struct devsw devsw[NDEV];
struct {
    struct spinlock lock;
    struct file file[NFILE];
} ftable;

/* Optional since BSS is zero-initialized. */
void
fileinit()
{
    /* TODO: Your code here. */
}

/* Allocate a file structure. */
struct file *
filealloc()
{
    /* TODO: Your code here. */
}

/* Increment ref count for file f. */
struct file *
filedup(struct file *f)
{
    /* TODO: Your code here. */
}

/* Close file f. (Decrement ref count, close when reaches 0.) */
void
fileclose(struct file *f)
{
    /* TODO: Your code here. */
}

/* Get metadata about file f. */
int
filestat(struct file *f, struct stat *st)
{
    /* TODO: Your code here. */
}

/* Read from file f. */
ssize_t
fileread(struct file *f, char *addr, ssize_t n)
{
    /* TODO: Your code here. */
}

/* Write to file f. */
ssize_t
filewrite(struct file *f, char *addr, ssize_t n)
{
    /* TODO: Your code here. */
}

