#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

int main(int argc, char const *argv[]) {
    if (argc == 1) {
        printf("wzip: file1 [file2 ...]\n");
        exit(1);
    }

    char *buffer = NULL;
    char curr = '\0';
    int count = 0;
    size_t n = 0;
    ssize_t nread;

    for (size_t i = 1; i < argc; i++) {
        FILE *fp = fopen(argv[i], "r");

        if (fp == NULL) {
            printf("wzip: cannot open file\n");
            exit(1);
        }

        if (curr == '\0') {
            nread = getline(&buffer, &n, fp);
            if (nread == -1) continue;
            curr = buffer[0];
            count = 1;

            for (size_t i = 1; i < nread; i++) {
                if (buffer[i] != curr) {
                    fwrite(&count, sizeof(int), 1, stdout);
                    fwrite(&curr, sizeof(char), 1, stdout);
                    curr = buffer[i];
                    count = 1;
                } else {
                    count++;
                }
            }
        }

        while ((nread = getline(&buffer, &n, fp)) != -1) {

            for (size_t i = 0; i < nread; i++) {
                if (buffer[i] != curr) {
                    fwrite(&count, sizeof(int), 1, stdout);
                    fwrite(&curr, sizeof(char), 1, stdout);
                    curr = buffer[i];
                    count = 1;
                } else {
                    count++;
                }
            }
        }
        fclose(fp);
    }
    fwrite(&count, sizeof(int), 1, stdout);
    fwrite(&curr, sizeof(char), 1, stdout);
    return 0;
}
