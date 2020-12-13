#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "squeue.h"

struct dummy {
    int a;
    const char *str;
};

bool dummy_cmp(const struct dummy *a, const struct dummy *b) {
    return (a->a == b->a) && (strcmp(a->str, b->str) == 0);
}

int dummy_display(const struct dummy *d) {
    printf("{ a: %d, str: %s }\n", d->a, d->str);
    return 0;
}

void sighandler(int sig) {
    if (sig == SIGABRT || sig == SIGSEGV || sig == SIGINT) {
        char dir[64] = "/dev/shm";
        remove(strcat(dir, SHM_QUEUE));
    }
}

void test_sq_empty(void) {
    printf("Testing sq_empty...\n");
    SQueue q = sq_empty(sizeof(struct dummy));
    assert(q != NULL);
    sq_dispose(&q);
}

void test_sq_dispose(void) {
    printf("Testing sq_dispose...\n");
    SQueue q = sq_empty(sizeof(struct dummy));
    sq_dispose(&q);
    assert(q == NULL);
}

void test_sq_enqueue(void) {
    printf("Testing sq_enqueue...\n");
    SQueue q = sq_empty(sizeof(struct dummy));
    struct dummy d = { 10, "foo" };
    assert(sq_enqueue(q, &d) == 0);
    assert(sq_length(q) == 1);
    sq_dispose(&q);
}

void test_sq_dequeue(void) {
    printf("Testing sq_dequeue...\n");
    SQueue q = sq_empty(sizeof(struct dummy));
    struct dummy d = { 10, "foo" };
    sq_enqueue(q, &d);
    struct dummy r;
    assert(sq_dequeue(q, &r) == 0);
    assert(sq_length(q) == 0);
    assert(dummy_cmp(&d, &r));
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


    SQueue q = sq_empty(sizeof(struct dummy));

    switch (fork()) {
    case -1:
        perror("fork");
        exit(EXIT_FAILURE);

    case 0:
        for (int i = 0; i < SQ_LENGTH_MAX + 5; i++) {
            struct dummy d = { i, "foo" };
            sq_enqueue(q, &d);
        }
        exit(EXIT_SUCCESS);

    default:
        for (int i = 0; i < 5; i++) {
            struct dummy d;
            sq_dequeue(q, &d);
        }
    }

    assert(sq_apply(q, (int (*)(void *)) dummy_display) == 0);
    assert(sq_length(q) == SQ_LENGTH_MAX);

    sq_dispose(&q);

    printf("All tests passed :)\n");

    return EXIT_SUCCESS;
}
