//// Copyright 2019 Haoyu Li
//
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
//
//int main(int argc, char *argv[]) {
//    if (argc <= 1 || argc >= 4) {
//        printf("my-look: invalid number of arguments\n");
//        exit(1);
//    }
//
//    FILE *fp = fopen(argc == 2 ? "/usr/share/dict/words" : argv[2], "r");
//    if (fp == NULL) {
//        printf("my-look: cannot open file\n");
//        exit(1);
//    }
//
//    char line[128];
//    while (fgets(line, sizeof(line), fp)) {
//        if (strncasecmp(argv[1], line, strlen(argv[1])) == 0) {
//            printf("%s", line);
//        }
//    }
//
//    fclose(fp);
//    return 0;
//}
