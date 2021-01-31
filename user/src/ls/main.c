
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "../../../inc/fs.h"

char *
fmtname(char *path)
{
    static char buf[DIRSIZ+1];
    char *p;
    // Find first character after last slash.
    for (p = path+strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    // Return blank-padded name.
    if (strlen(p) >= DIRSIZ)
        return p;
    memmove(buf, p, strlen(p));
    memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
    return buf;
}

void
ls(char *path)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, O_RDONLY)) < 0) {
        fprintf(stderr, "ls: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(stderr, "ls: cannot stat %s\n", path);
        close(fd);
        return;
    }

    if (S_ISREG(st.st_mode)) {
        printf("%s %x %ld %ld\n", fmtname(path), st.st_mode, st.st_ino, st.st_size);
    } else if (S_ISDIR(st.st_mode)) {
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
            fprintf(stderr, "ls: path too long\n");
        } else {
            strcpy(buf, path);
            p = buf+strlen(buf);
            *p++ = '/';
            while (read(fd, &de, sizeof(de)) == sizeof(de)) {
                if (de.inum == 0)
                    continue;
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;
                if (stat(buf, &st) < 0) {
                    fprintf(stderr, "ls: cannot stat %s\n", buf);
                    continue;
                }
                printf("%s %x %ld %ld\n", fmtname(buf), st.st_mode, st.st_ino, st.st_size);
            }
        }
    }
    close(fd);
}

int
main(int argc, char *argv[])
{
    if (argc < 2)
        ls(".");
    else for (int i = 1; i < argc; i++)
        ls(argv[i]);
    return 0;
}
