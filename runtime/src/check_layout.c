/* Print sizeof/offsetof struct mCore fields to diagnose layout mismatch */
#include <mgba/flags.h>
#include <mgba/core/core.h>
#include <stdio.h>
#include <stddef.h>

int main(void) {
    printf("sizeof(struct mCore) = %zu\n", sizeof(struct mCore));
    printf("offsetof(mCore, cpu)       = %zu\n", offsetof(struct mCore, cpu));
    printf("offsetof(mCore, board)     = %zu\n", offsetof(struct mCore, board));
    printf("offsetof(mCore, timing)    = %zu\n", offsetof(struct mCore, timing));
    printf("offsetof(mCore, debugger)  = %zu\n", offsetof(struct mCore, debugger));
    printf("offsetof(mCore, config)    = %zu\n", offsetof(struct mCore, config));
    printf("offsetof(mCore, init)      = %zu\n", offsetof(struct mCore, init));
    printf("offsetof(mCore, deinit)    = %zu\n", offsetof(struct mCore, deinit));
    printf("offsetof(mCore, setVideoBuffer) = %zu\n", offsetof(struct mCore, setVideoBuffer));
    printf("offsetof(mCore, runFrame)  = %zu\n", offsetof(struct mCore, runFrame));
    printf("offsetof(mCore, setKeys)   = %zu\n", offsetof(struct mCore, setKeys));
    printf("offsetof(mCore, reset)     = %zu\n", offsetof(struct mCore, reset));
    return 0;
}
