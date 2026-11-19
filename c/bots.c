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

/* Test of de_resume_buy: Big Money bots play a game; at the `at`-th buy decision a copy of the
 * game is taken, and after the game the copy is resumed with the same answer. Without de_hide it
 * must finish identically. out: [same turns and decks, same winners, same scores, copied]. */
typedef struct { bot b; int at, seen, copied, answer; de_game *copy; } spy;

static int spy_decide(void *ctx, const de_game *g, int kind, int player, int subject,
                      const int *opts, int n, int may_pass) {
    spy *s = ctx;
    int a = bot_decide(&s->b, g, kind, player, subject, opts, n, may_pass);
    if (kind == DE_BUY && !s->copied && ++s->seen == s->at) {
        *s->copy = *g;
        s->copied = 1;
        s->answer = a;
    }
    return a;
}

void de_resume_check(const int *kingdom, int kn, uint64_t seed, int at, int *out) {
    static de_game g, copy;
    spy spies[2];
    de_agent agents[2];
    for (int i = 0; i < 2; i++) {
        spies[i] = (spy){{BOT_BIG_MONEY, 1}, i == 0 ? at : 0, 0, i != 0, -1, &copy};
        agents[i].decide = spy_decide;
        agents[i].ctx = &spies[i];
    }
    de_init(&g, kingdom, kn, 2, seed);
    de_run(&g, agents);
    out[3] = spies[0].copied;
    if (!spies[0].copied) { out[0] = out[1] = out[2] = 0; return; }
    spies[0].copied = 1;
    de_resume_buy(&copy, agents, spies[0].answer);
    int same = 1;                     /* (the trace differs: the copy skips hashing the first buy) */
    for (int i = 0; i < 2; i++) {
        same &= copy.p[i].turns == g.p[i].turns;
        for (int c = 0; c < DE_NUM_CARDS; c++) same &= de_count(&copy.p[i], c) == de_count(&g.p[i], c);
    }
    out[0] = same;
    out[1] = de_winners(&copy) == de_winners(&g);
    out[2] = de_score(&copy, 0) == de_score(&g, 0) && de_score(&copy, 1) == de_score(&g, 1);
}

/* Test of de_resume_action: random bots play a game; at player 0's `at`-th PLAY_ACTION decision a
 * copy of the game and of both bots is taken; after the game the copy is resumed with the same
 * answer and must finish identically. out: [same turns and decks, same winners, same scores, copied]. */
typedef struct { bot b; int at, seen, copied, answer; de_game *copy; bot saved[2]; void *all; } aspy;

static int aspy_decide(void *ctx, const de_game *g, int kind, int player, int subject,
                       const int *opts, int n, int may_pass) {
    aspy *s = ctx;
    aspy *all = s->all;
    if (kind == DE_PLAY_ACTION && s->at && !s->copied && ++s->seen == s->at) {
        *s->copy = *g;
        s->saved[0] = all[0].b;
        s->saved[1] = all[1].b;
        s->copied = 1;
        int a = bot_decide(&s->b, g, kind, player, subject, opts, n, may_pass);
        s->answer = a;
        return a;
    }
    return bot_decide(&s->b, g, kind, player, subject, opts, n, may_pass);
}

void de_resume_action_check(const int *kingdom, int kn, uint64_t seed, int at, int *out) {
    static de_game g, copy;
    aspy spies[2];
    de_agent agents[2];
    for (int i = 0; i < 2; i++) {
        spies[i] = (aspy){{BOT_RANDOM, seed * 7 + i + 1}, i == 0 ? at : 0, 0, 0, -1, &copy, {{0}}, spies};
        agents[i].decide = aspy_decide;
        agents[i].ctx = &spies[i];
    }
    de_init(&g, kingdom, kn, 2, seed);
    de_run(&g, agents);
    out[3] = spies[0].copied;
    if (!spies[0].copied) { out[0] = out[1] = out[2] = 0; return; }
    spies[0].b = spies[0].saved[0];
    spies[1].b = spies[0].saved[1];
    /* the copy was taken before the answer was drawn: draw it again from the restored bot */
    spies[0].at = 0;
    (void)de_rng_next(&spies[0].b.rng);       /* a random bot draws once per decision */
    de_resume_action(&copy, agents, spies[0].answer, 1);
    int same = 1;
    for (int i = 0; i < 2; i++) {
        same &= copy.p[i].turns == g.p[i].turns;
        for (int c = 0; c < DE_NUM_CARDS; c++) same &= de_count(&copy.p[i], c) == de_count(&g.p[i], c);
    }
    out[0] = same;
    out[1] = de_winners(&copy) == de_winners(&g);
    out[2] = de_score(&copy, 0) == de_score(&g, 0) && de_score(&copy, 1) == de_score(&g, 1);
}
