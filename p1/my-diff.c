// Copyright 2019 Haoyu Li

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("my-diff: invalid number of arguments\n");
        exit(1);
    }

    FILE *fp1 = fopen(argv[1], "r");
    FILE *fp2 = fopen(argv[2], "r");

    if (fp1 == NULL || fp2 == NULL) {
        printf("my-diff: cannot open file\n");
        exit(1);
    }


    char *line1 = NULL, *line2 = NULL;
    size_t len = 0;
    int lineLen1 = 0, lineLen2 = 0;
    int lenNum = 1, prev = -1;
    lineLen1 = getline(&line1, &len, fp1);
    lineLen2 = getline(&line2, &len, fp2);
    while (lineLen1 != -1 || lineLen2 != -1) {
        if (lineLen1 != lineLen2 || strcmp(line1, line2) != 0) {
            if (prev + 1 < lenNum) {
                printf("%d\n", lenNum);
            }
            if (lineLen1 != -1) {
                printf("< %s", line1);
            }
            if (lineLen2 != -1) {
                printf("> %s", line2);
            }
            prev = lenNum;
        }
        lenNum++;
        lineLen1 = getline(&line1, &len, fp1);
        lineLen2 = getline(&line2, &len, fp2);
    }

    fclose(fp1);
    fclose(fp2);

    return 0;
}
