#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[]){
    int frames[100] = {0};
    int pid[100] = {0};
    dump_physmem(frames, pid, 100);
    for(int i = 0; i < 100;i++){
        printf(1, "frames = %d, pid = %d\n",frames[i], pid[i]);
    }
    exit();
}