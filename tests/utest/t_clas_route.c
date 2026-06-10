/*------------------------------------------------------------------------------
 * unit test : clas_route_l6frame() L6D demux contract (#197)
 *
 * A multi-channel L6 receiver (u-blox D9C, 2ch; future receivers with more)
 * interleaves L6D frames from several QZS PRNs in one UBX/SBF stream. Each CLAS
 * channel decodes one PRN's subframe sequence statefully, so the demux must:
 *   - lock each channel to a single active source PRN and DROP frames from any
 *     other PRN (mixing two PRNs into one channel corrupts subframe assembly,
 *     which manifested as "L6 Lost" with corrections flowing but not decoding);
 *   - re-lock a channel to a new PRN/pattern only after the active source has
 *     been silent past CLAS_L6_RELOCK_TIMEOUT (QZS satellite handover, #197);
 *   - key channels by CLAS Transmit Pattern ID in dual mode (l6mrg!=0) so the
 *     two augmentation patterns map to separate channels for merging, while
 *     single mode (l6mrg=0) collapses to ch0.
 *
 * The routing decision is a pure function of (prn, ptn, l6mrg, now) and the
 * per-channel lock state, so it is fully testable without a raw L6 stream.
 * Explicit checks (not assert) so it is robust under -DNDEBUG.
 *-----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>

#include "mrtklib/mrtk_clas.h"

static int fails = 0;

#define CHECK(cond, msg)                 \
    do {                                 \
        if (!(cond)) {                   \
            printf("FAIL: %s\n", (msg)); \
            fails++;                     \
        } else {                         \
            printf("ok  : %s\n", (msg)); \
        }                                \
    } while (0)

/* GPST time t seconds past an arbitrary epoch */
static gtime_t at(int t) {
    gtime_t g;
    g.time = (time_t)(1000000000 + t);
    g.sec = 0.0;
    return g;
}

/* fresh context with the demux lock state cleared (banks not needed here) */
static clas_ctx_t* fresh(void) {
    clas_ctx_t* ctx = (clas_ctx_t*)calloc(1, sizeof(clas_ctx_t));
    int i;
    for (i = 0; i < CLAS_CH_NUM; i++) {
        ctx->l6delivery[i] = -1;
        ctx->l6pattern[i] = -1;
        ctx->l6lock_time[i].time = 0;
        ctx->l6lock_time[i].sec = 0.0;
    }
    return ctx;
}

/* single-channel CLAS (l6mrg=0): one coherent PRN on ch0, handover re-lock */
static void test_single(void) {
    clas_ctx_t* ctx = fresh();

    CHECK(clas_route_l6frame(ctx, 194, 2, 0, at(0)) == 0, "single: first PRN locks ch0");
    CHECK(clas_route_l6frame(ctx, 194, 2, 0, at(1)) == 0, "single: active PRN stays on ch0");
    CHECK(clas_route_l6frame(ctx, 199, 1, 0, at(1)) == -1, "single: interleaved 2nd PRN dropped");
    CHECK(clas_route_l6frame(ctx, 194, 2, 0, at(5)) == 0, "single: active PRN refreshes lock");
    CHECK(clas_route_l6frame(ctx, 199, 1, 0, at(8)) == -1, "single: 2nd PRN still dropped within timeout");
    CHECK(clas_route_l6frame(ctx, 199, 1, 0, at(16)) == 0, "single: re-lock to new PRN after timeout (handover)");
    CHECK(clas_route_l6frame(ctx, 199, 1, 0, at(17)) == 0, "single: new active PRN stays on ch0");
    CHECK(clas_route_l6frame(ctx, 194, 2, 0, at(17)) == -1, "single: old PRN now dropped");

    free(ctx);
}

/* dual-channel CLAS (l6mrg!=0): one channel per transmit pattern */
static void test_dual(void) {
    clas_ctx_t* ctx = fresh();

    CHECK(clas_route_l6frame(ctx, 194, 2, 1, at(0)) == 0, "dual: pattern 2 → ch0");
    CHECK(clas_route_l6frame(ctx, 199, 1, 1, at(0)) == 1, "dual: pattern 1 → ch1");
    CHECK(clas_route_l6frame(ctx, 194, 2, 1, at(0)) == 0, "dual: pattern 2 PRN stays ch0");
    CHECK(clas_route_l6frame(ctx, 195, 1, 1, at(1)) == -1, "dual: 2nd same-pattern PRN dropped");
    CHECK(clas_route_l6frame(ctx, 195, 1, 1, at(12)) == 1, "dual: same-pattern handover re-locks ch1");
    CHECK(clas_route_l6frame(ctx, 195, 1, 1, at(13)) == 1, "dual: new active PRN stays ch1");
    CHECK(clas_route_l6frame(ctx, 199, 1, 1, at(13)) == -1, "dual: replaced PRN now dropped");
    CHECK(clas_route_l6frame(ctx, 194, 2, 1, at(13)) == 0, "dual: active PRN short-circuits regardless of pattern arg");

    free(ctx);
}

int main(void) {
    test_single();
    test_dual();

    printf("\n%s: %d check(s) failed\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
