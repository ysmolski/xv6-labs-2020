#include "../kernel/types.h"
#include "../user/user.h"

int
main(int argc, char *argv[]) {
    int par[2]; // parent to child
    int chi[2]; // child to parent
    pipe(par);
    pipe(chi);
    int pid = fork();
    if (pid == 0) {
        // child
        close(par[1]); // parent will write here
        close(chi[0]); // parent will read this
        char b[1];
        read(par[0], b, 1);
        printf("%d: received ping\n", getpid());
        write(chi[1], b, 1);
        close(chi[1]);
    } else {
        // parent
        close(par[0]);
        close(chi[1]);
        char b[1];
        write(par[1], b, 1);
        close(par[1]);
        read(chi[0], b, 1);
        printf("%d: received pong\n", getpid());
    }


    exit(0);
}
