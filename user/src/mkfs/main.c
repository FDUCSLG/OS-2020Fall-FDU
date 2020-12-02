#include <stdio.h>
#include <assert.h>

int
main(int argc, char *argv[])
{
    printf("mkfs: hello, world %d\n", argc);
    assert(argc >= 2);

    for (int i = 0; i < argc; i++)
        printf("arg[%d] = \"%s\"\n", i, argv[i]);

    size_t chunk = 512;
    char buf[chunk];
    FILE *fout = fopen(argv[1], "w");
    for (int i = 2; i < argc; i++) {
        FILE *fin = fopen(argv[i], "r");
        for (size_t n; (n = fread(buf, 1, chunk, fin)); ) {
            assert(n <= chunk);
            fwrite(buf, n, 1, fout);
        }
        fclose(fin);
    }
    fclose(fout);
    return 0;
}
