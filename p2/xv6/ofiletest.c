#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

//ofiletest N <list of file nums to close and delete>. 
//This program takes an argument, N, which is the number 
//of (empty) files to open (creating if necessary) , 
//followed by a list of files to close and delete.   
//After this work has been done, it should print out two 
//values: the value returned by getofilecnt(getpid()) and 
//the value returned by getofilenext(getpid()).  The N 
//files that are opened should be named ofile0...ofileN-1.
int main(int argc, char *argv[])
{
    //printf(1, "argc = %d\n", argc);
    if(argc < 2){
        printf(2, "usage: ofiletest fd...\n");
        exit();
    }

    char str[100];
    strcpy(str, "ofile");
    for (int i = 0 ; i < atoi(argv[1]);i++){
        if(i < 10){
            str[5] = '0' + i;
            str[6] = '\0';
        }else{
            str[5] = '0' + i / 10;
            str[6] = '0' + i;
            str[7] = '\0';
        }
        open(str, O_CREATE);
    }
    for(int i = 2 ; i < argc; i++){
        close(atoi(argv[i]) + 3);
        //printf(1, strcpy(&str[5], argv[i]));
        strcpy(&str[5], argv[i]);
        unlink(str);
    }
    
    printf(1, "%d", getofilecnt(getpid()));
    printf(1, " ");
    printf(1, "%d\n", getofilenext(getpid()));
    exit();
}