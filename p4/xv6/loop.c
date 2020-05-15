// Job loop created for testing userRR.

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[]) {
    /*for (int i = 0;i<3;i++){
        sleep(10);
        printf(1, "%d\n", getpid());
    }*/

    while (1) {
        sleep(10);
        printf(1, "%d\n", getpid());
    }
    exit();
}
