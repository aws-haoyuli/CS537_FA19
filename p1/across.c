//// Copyright 2019 Haoyu Li
//
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
//
//int main(int argc, char *argv[]) {
//    if (argc != 4 && argc != 5) {
//        printf("across: invalid number of arguments\n");
//        exit(1);
//    }
//
//    for (int i = 0; i < strlen(argv[1]); i++) {
//        if (argv[1][i] < 97 || argv[1][i] > 122) {
//            return 0;
//        }
//    }
//
//    FILE *fp = fopen(argc == 4 ? "/usr/share/dict/words" : argv[4], "r");
//    if (fp == NULL) {
//        printf("across: cannot open file\n");
//        exit(1);
//    }
//
//    int pos = atoi(argv[2]);
//    int target_len = atoi(argv[3]);
//    if (pos + strlen(argv[1]) > target_len) {
//        printf("across: invalid position\n");
//        exit(1);
//    }
//
//    char line[128];
//    while (fgets(line, sizeof(line), fp)) {
//        line[strlen(line) - 1] = '\0';
//        if (strlen(line) == target_len) {
//            int flag = 0;
//            for (int i = 0; i < strlen(line); i++) {
//                if (line[i] < 97 || line[i] > 122) {
//                    flag = 1;
//                    break;
//                }
//            }
//            if (flag == 1) {
//                continue;
//            }
//            for (int i = 0; i < strlen(argv[1]); i++) {
//                if (line[pos + i] != argv[1][i]) {
//                    flag = 1;
//                    break;
//                }
//            }
//            if (flag == 0) {
//                line[strlen(line)] = '\n';
//                printf("%s", line);
//            }
//        }
//    }
//    fclose(fp);
//    return 0;
//}
