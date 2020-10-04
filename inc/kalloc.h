#ifndef KERN_KALLOC_H
#define KERN_KALLOC_H

void boot_alloc_init(void);
void alloc_init(void);
char *kalloc(void);
void kfree(char*);
void free_range(void *, void *);

// Check function
void check_free_list(void);

#endif /* !KERN_KALLOC_H */