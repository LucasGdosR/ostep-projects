#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

int main(int argc, char* argv[]) {
	if (argc == 1) {
		printf("wgrep: searchterm [file ...]\n");
		exit(1);
	}

	char *word = argv[1], *buffer = NULL;
	int boundary = 0;
	size_t length = strlen(word), n = 0;
	ssize_t nread;
	bool printed = false;

	if (argc == 2) {
		while ((nread = getline(&buffer, &n, stdin)) != -1) {
			boundary = nread - length;
            for (int i = 0; i <= boundary; i++) {
                if (buffer[i] == word[0]) {
                    bool match = true;
                    for (size_t j = 1; j < length; j++)	{
                        if (buffer[i + j] != word[j]) match = false;
                    }
                    if (match) {
                        printf("%s", buffer);
                        printed = true;
                    }
                }
                if (printed) {
					printed = false;
					break;
            	}
			}
		}
		free(buffer);
	}

	else {
		for (size_t i = 2; i < argc; i++) {
			
            FILE* fp = fopen(argv[i], "r");
			if (fp == NULL)	{
				printf("wgrep: cannot open file\n");
				exit(1);
			}

			while ((nread = getline(&buffer, &n, fp)) != -1) {
				boundary = nread - length;
				for (int i = 0; i <= boundary; i++) {

                    if (buffer[i] == word[0]) {
						bool match = true;
						for (size_t j = 1; j < length; j++)	{
							if (buffer[i + j] != word[j]) match = false;
						}
						if (match) {
							printf("%s", buffer);
                            printed = true;
						}
					}
                    if (printed) {
						printed = false;
					 	break;
					}	
				}
			}
			fclose(fp);
		}
		free(buffer);
	}
	return 0;
}