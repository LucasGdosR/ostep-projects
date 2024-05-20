#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#define BUFFER_SIZE (5)

int main(int argc, char const *argv[]) {
    if (argc == 1) {
        printf("wunzip: file1 [file2 ...]\n");
        exit(1);
    }

    char buffer[BUFFER_SIZE];

    for (size_t i = 1; i < argc; i++) {
        FILE *fp = fopen(argv[i], "r");

        if (fp == NULL) {
            printf("wunzip: cannot open file\n");
            exit(1);
        }

        while (fread(&buffer, BUFFER_SIZE, 1, fp) != 0) {
            int count = *(int *)buffer;
            char ch = buffer[4];
            for (int j = 0; j < count; j++) {
                printf("%c", ch);
            }
        }
        fclose(fp);
    }
    return 0;
}
