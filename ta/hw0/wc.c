#include <stdio.h>
#include <ctype.h>
#include <string.h>

typedef struct Count {
    unsigned int words;
    unsigned int lines;
    unsigned int characters;
} Count;

void wc(FILE *ofile, FILE *infile, char *inname) {
    Count stats;

    char buf[1000];

    while(fgets(buf, 1000, infile) != NULL) {
        stats.lines++;
        unsigned int i;
        char isWord;
        for(i = 0, isWord = 0; i < 1000 && buf[i] != '\n'; i++) {
            if(!isblank(buf[i]) || !isspace(buf[i])) {
                stats.characters++;
                if(isWord == 0) {
                    isWord = 1;
                    stats.words++;
                }
            } else {
                isWord = 0;
            }
        }
    }

    char output[1024];
    sprintf(output, "%d %d %d %s\n", stats.lines, stats.words, stats.characters, inname);
    fputs(output, ofile);
}

int main (int __unused argc, char __unused  *argv[]) {
    char *fileIn = argv[1];
    char *fileOut = argv[2];
    wc(fopen(fileOut, "w"), fopen(fileIn, "r"), fileIn);
    return 0;
}
