/*
 * bots.c -- twins of dominion_engine/bots.py, and the entry points dominion_engine/fast.py
 * calls through ctypes.
 */
#include "engine.h"

enum { BOT_BIG_MONEY, BOT_RANDOM };

typedef struct { int kind; uint64_t rng; } bot;

static int bot_decide(void *ctx, const de_game *g, int kind, int player, int subject,
                      const int *opts, int n, int may_pass) {
    bot *b = ctx;
    (void)g; (void)player; (void)subject;
    if (b->kind == BOT_RANDOM) {
        int k = (int)(de_rng_next(&b->rng) % (uint64_t)(n + (may_pass ? 1 : 0)));
        return k == n ? -1 : opts[k];
    }
    if (kind == DE_BUY) {
        static const int want[3] = {DE_PROVINCE, DE_GOLD, DE_SILVER};
        for (int w = 0; w < 3; w++)
            for (int i = 0; i < n; i++)
                if (opts[i] == want[w]) return want[w];
        return -1;
    }
    return may_pass ? -1 : opts[0];
}

/* One game between bots. out: scores[4], turns[4], winners mask, capped, error. */
void de_play_bots(const int *kingdom, int kn, uint64_t seed, int nplayers,
                  const int *kinds, const uint64_t *bot_seeds, int *out, uint64_t *trace) {
    de_game g;
    bot bots[DE_MAX_PLAYERS];
    de_agent agents[DE_MAX_PLAYERS];
    for (int i = 0; i < nplayers; i++) {
        bots[i].kind = kinds[i];
        bots[i].rng = bot_seeds[i] ? bot_seeds[i] : 1;
        agents[i].decide = bot_decide;
        agents[i].ctx = &bots[i];
    }
    de_init(&g, kingdom, kn, nplayers, seed);
    de_run(&g, agents);
    for (int i = 0; i < DE_MAX_PLAYERS; i++) {
        out[i] = i < nplayers ? de_score(&g, i) : 0;
        out[4 + i] = i < nplayers ? g.p[i].turns : 0;
    }
    out[8] = de_winners(&g);
    out[9] = g.capped;
    out[10] = g.error;
    *trace = g.trace;
}

/* Card table, 7 ints per card (cost, types, coins, vp, cards, actions, buys): checked
 * against cards.py by the tests. */
int de_card_table(int *out) {
    for (int c = 0; c < DE_NUM_CARDS; c++) {
        const de_card *d = &DE_CARDS[c];
        int *o = out + 7 * c;
        o[0] = d->cost; o[1] = d->types; o[2] = d->coins; o[3] = d->vp;
        o[4] = d->cards; o[5] = d->actions; o[6] = d->buys;
    }
    return DE_NUM_CARDS;
}
