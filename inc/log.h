#ifndef INC_LOG_H
#define INC_LOG_H

struct buf;

void initlog(int dev);
void log_write(struct buf *);
void begin_op();
void end_op();

#endif
