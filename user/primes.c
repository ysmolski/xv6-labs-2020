#include "../kernel/types.h"
#include "../user/user.h"

void 
spawn(int read_fd) {
    /* printf("start %d\n", read_fd); */
    int p[2];
    pipe(p);

    int prime = 0;
    for (;;) {
        int num;
        int n = read(read_fd, &num, sizeof num);
        if (n == 0)
            break;
        if (n < 4) {
            /* printf("%d [%d]\n", read_fd, n); */
            continue;
        }
        if (prime == 0) {
            // only if we find prime then we fork a process
            prime = num;
            printf("prime %d\n", prime);
            if (fork() == 0) {
                close(p[1]);
                spawn(p[0]);
            } else {
                close(p[0]);
                // carry on sending to child like nothing happened
            }
        } else {
            if (num % prime == 0)
                continue;
            write(p[1], &num, 4);
        }
    }
    close(p[1]);
    int status;
    wait(&status);
    /* printf("%d is done\n", pid); */
}

int
main(int argc, char *argv[]) {

    int p[2];
    pipe(p);

    if (fork() == 0) {
        close(p[1]);
        spawn(p[0]);
    } else {
        close(p[0]);
        for (int i=2; i<=35; i++) {
            write(p[1], &i, sizeof i);
        }
        close(p[1]);
        int status;
        wait(&status);
        /* printf("%d is done\n", pid); */
    }
    exit(0);
}

