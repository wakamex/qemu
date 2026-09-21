/*
 * Exercise repeated translations through the public libtcg API.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libtcg/libtcg.h"

/* Public x86-64 inputs: a self-loop, and arithmetic followed by a self-loop. */
static const unsigned char loop[] = { 0xeb, 0xfe };
static const unsigned char arithmetic[] = {
    0xb8, 0x2a, 0x00, 0x00, 0x00, /* mov eax, 42 */
    0x83, 0xc0, 0x01,             /* add eax, 1 */
    0xeb, 0xf6,                   /* jmp to the first instruction */
};

/* Also makes the failing call easy to identify in a debugger. */
volatile unsigned long libtcg_test_attempt;

static void check_block(LibTcgTranslationBlock block, size_t size)
{
    if (!block.list || !block.instruction_count || block.size_in_bytes != size) {
        fprintf(stderr, "Invalid translation at call %lu\n", libtcg_test_attempt);
        abort();
    }
}

int main(void)
{
    LibTcgInterface api = libtcg_load();
    LibTcgContext *context = api.context_create();
    LibTcgTranslationBlock retained;
    char (*before)[1024];
    char after[1024];

    if (!context) {
        return EXIT_FAILURE;
    }

    libtcg_test_attempt = 1;
    retained = api.translate_block(context, loop, sizeof(loop), 0x1000, 0);
    check_block(retained, sizeof(loop));
    before = calloc(retained.instruction_count, sizeof(*before));
    if (!before) {
        return EXIT_FAILURE;
    }
    for (size_t i = 0; i < retained.instruction_count; i++) {
        api.dump_instruction_to_buffer(&retained.list[i], before[i],
                                       sizeof(*before));
    }

    /* Exceeds the capacity of the default 128 MiB TCG code region. */
    for (libtcg_test_attempt = 2; libtcg_test_attempt <= 1000000;
         libtcg_test_attempt++) {
        const unsigned char *bytes = libtcg_test_attempt % 2 ? loop : arithmetic;
        size_t size = libtcg_test_attempt % 2 ? sizeof(loop) : sizeof(arithmetic);
        LibTcgTranslationBlock block = api.translate_block(context, bytes, size,
                                                           0x1000, 0);
        check_block(block, size);
        api.translation_block_destroy(context, block);
    }

    /* Results remain caller-owned while subsequent translations run. */
    for (size_t i = 0; i < retained.instruction_count; i++) {
        api.dump_instruction_to_buffer(&retained.list[i], after, sizeof(after));
        if (strcmp(before[i], after)) {
            return EXIT_FAILURE;
        }
    }
    free(before);
    api.translation_block_destroy(context, retained);
    api.context_destroy(context);
    puts("1000000 translations passed");
    return EXIT_SUCCESS;
}
