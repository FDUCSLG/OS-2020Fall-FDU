#include <stdint.h>
#include "syscall.h"
#include "console.h"
#include "string.h"

int
sys_exec()
{
    char *path, *argv[MAXARG];
    int i;
    uint64_t uargv, uarg;

    if(argstr(0, &path) < 0 || argint(1, &uargv) < 0){
        return -1;
    }
    memset(argv, 0, sizeof(argv));
    for(i = 0;; i++){
        if(i >= NELEM(argv)) {
            return -1;
        }
        if(fetchint(uargv + 8 * i, (int64_t*)&uarg) < 0) {
            return -1;
        }
        if(uarg == 0){
            argv[i] = 0;
            break;
        }
        if(fetchstr(uarg, &argv[i]) < 0) {
            return -1;
        }
    }
    cprintf("sys_exec: executing %s with parameters: ", path);
    for (int j = 0; j < i; j++) {
        cprintf("%s ", argv[j]);
    }
    cprintf("\n");
    return 0;
}