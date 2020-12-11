#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"
#include "squeue.h"

void sighandler(int sig) {
    if (sig == SIGABRT || sig == SIGSEGV || sig == SIGINT) {
        char dir[64] = "/dev/shm";
        remove(strcat(dir, SHM_QUEUE));
    }
}

bool request_eql(struct request r1, struct request r2) {
    bool cmd = strcmp(r1.cmd, r2.cmd) == 0;
    bool pipe = strcmp(r1.pipe, r2.pipe) == 0;
    return cmd && pipe;
}

void test_sq_empty(void) {
    printf("Testing sq_empty...\n");
    SQueue q = sq_empty();
    assert(q != NULL);
    sq_dispose(&q);
}

void test_sq_dispose(void) {
    printf("Testing sq_dispose...\n");
    SQueue q = sq_empty();
    sq_dispose(&q);
    assert(q == NULL);
}

void test_sq_enqueue(void) {
    printf("Testing sq_enqueue...\n");
    SQueue q = sq_empty();
    struct request dummy = { "foo", "bar" };
        struct request r;
    int ret;

    for (int i = 0; i < SQ_LENGTH_MAX; i++) {
        sq_enqueue(q, &dummy);
    }

    assert(sq_length(q) == SQ_LENGTH_MAX);

    switch (fork()) {
    case -1:
        perror("fork");
        exit(EXIT_FAILURE);

    case 0:
        sleep(1);
        sq_dequeue(q, &r);
        exit(EXIT_SUCCESS);
    
    default:
        ret = sq_enqueue(q, &dummy);
    }

    assert(ret == 0);
    assert(sq_length(q) == SQ_LENGTH_MAX);
    sq_dispose(&q);
}

void test_sq_dequeue(void) {
    printf("Testing sq_dequeue...\n");
    SQueue q = sq_empty();
    struct request dummy = { "foo", "bar" };
    struct request r;
    int ret;

    switch (fork()) {
    case -1:
        perror("fork()");
        exit(EXIT_FAILURE);

    case 0:
        sleep(1);
        sq_enqueue(q, &dummy);
        exit(EXIT_SUCCESS);

    default:
        ret = sq_dequeue(q, &r);
        break;
    }

    assert(ret == 0);
    assert(request_eql(dummy, r));
    assert(sq_length(q) == 0);
    sq_dispose(&q);
}

void test_sq_sync(void) {
    struct request dummy = { "foo", "bar" };
    struct request r;

    SQueue q = sq_empty();

    switch (fork()) {
    case -1:
        perror("fork");
        exit(EXIT_FAILURE);

    case 0:
        for (int i = 0; i < SQ_LENGTH_MAX + 1; i++) {
            sq_enqueue(q, &dummy);
        }
        exit(EXIT_SUCCESS);

    default:
        sq_dequeue(q, &r);
        sq_dequeue(q, &r);
    }

    wait(NULL);
    assert(sq_length(q) == SQ_LENGTH_MAX);
    sq_dispose(&q);
}

int main(void) {
    struct sigaction action;
    action.sa_handler = sighandler;
    action.sa_flags = 0;
    if (sigfillset(&action.sa_mask) == -1) {
        perror("sigfillset");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGABRT, &action, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGSEGV, &action, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGINT, &action, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    test_sq_empty();
    test_sq_dispose();
    test_sq_enqueue();
    test_sq_dequeue();

    printf("All tests passed :)\n");

    return EXIT_SUCCESS;
}


