/*
 * engine.c -- the C twin of dominion_engine/game.py. Keep the two in step: same rules, same
 * decisions, same order of card movements (the order of the discard pile decides shuffles).
 */
#include <string.h>
#include "engine.h"

#define T DE_TREASURE
#define V DE_VICTORY
#define A DE_ACTION
const de_card DE_CARDS[DE_NUM_CARDS] = {
    /* name, cost, types, coins, vp, cards, actions, buys */
    {"Copper", 0, T, 1, 0, 0, 0, 0},
    {"Silver", 3, T, 2, 0, 0, 0, 0},
    {"Gold", 6, T, 3, 0, 0, 0, 0},
    {"Estate", 2, V, 0, 1, 0, 0, 0},
    {"Duchy", 5, V, 0, 3, 0, 0, 0},
    {"Province", 8, V, 0, 6, 0, 0, 0},
    {"Village", 3, A, 0, 0, 1, 2, 0},
    {"Smithy", 4, A, 0, 0, 3, 0, 0},
    {"Market", 5, A, 1, 0, 1, 1, 1},
    {"Laboratory", 5, A, 0, 0, 2, 1, 0},
    {"Festival", 5, A, 2, 0, 0, 2, 1},
    {"Chapel", 2, A, 0, 0, 0, 0, 0},
    {"Throne Room", 4, A, 0, 0, 0, 0, 0},
    {"Council Room", 5, A, 0, 0, 4, 0, 1},
    {"Moneylender", 4, A, 0, 0, 0, 0, 0},
    {"Gardens", 4, V, 0, 0, 0, 0, 0},
    {"Mine", 5, A, 0, 0, 0, 0, 0},
    {"Merchant", 3, A, 0, 0, 1, 1, 0},
    {"Curse", 0, DE_CURSE_T, 0, -1, 0, 0, 0},
    {"Militia", 4, A | DE_ATTACK, 2, 0, 0, 0, 0},
    {"Witch", 5, A | DE_ATTACK, 0, 0, 2, 0, 0},
    {"Moat", 2, A | DE_REACTION, 0, 0, 2, 0, 0},
    {"Cellar", 2, A, 0, 0, 0, 1, 0},
    {"Harbinger", 3, A, 0, 0, 1, 1, 0},
    {"Vassal", 3, A, 2, 0, 0, 0, 0},
    {"Workshop", 3, A, 0, 0, 0, 0, 0},
    {"Bureaucrat", 4, A | DE_ATTACK, 0, 0, 0, 0, 0},
    {"Poacher", 4, A, 1, 0, 1, 1, 0},
    {"Remodel", 4, A, 0, 0, 0, 0, 0},
    {"Library", 5, A, 0, 0, 0, 0, 0},
    {"Sentry", 5, A, 0, 0, 1, 1, 0},
    {"Artisan", 6, A, 0, 0, 0, 0, 0},
    {"Bandit", 5, A | DE_ATTACK, 0, 0, 0, 0, 0},
};
#undef T
#undef V
#undef A

#define IS(c, t) (DE_CARDS[c].types & (t))

/* ── RNG (= rng.py) ── */
uint64_t de_rng_next(uint64_t *s) {
    uint64_t x = *s;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *s = x;
    return x;
}

static void shuffle(int *a, int n, uint64_t *rng) {
    for (int i = n - 1; i > 0; i--) {
        int j = (int)(de_rng_next(rng) % (uint64_t)(i + 1));
        int t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

/* ── zones ── */
static int rm(int *a, int *n, int v) {          /* remove first occurrence (= list.remove) */
    for (int i = 0; i < *n; i++)
        if (a[i] == v) {
            memmove(a + i, a + i + 1, (size_t)(*n - i - 1) * sizeof(int));
            (*n)--;
            return 1;
        }
    return 0;
}

static int has(const int *a, int n, int v) {
    for (int i = 0; i < n; i++) if (a[i] == v) return 1;
    return 0;
}

/* distinct cards of a[0..n-1] with any of `types` (0 = all), ascending id */
static int distinct(const int *a, int n, int types, int *out) {
    char seen[DE_NUM_CARDS] = {0};
    for (int i = 0; i < n; i++) seen[a[i]] = 1;
    int k = 0;
    for (int c = 0; c < DE_NUM_CARDS; c++)
        if (seen[c] && (!types || IS(c, types))) out[k++] = c;
    return k;
}

static int reveal(de_player *p) {
    if (p->deck_n == 0) {
        if (p->discard_n == 0) return -1;
        memcpy(p->deck, p->discard, (size_t)p->discard_n * sizeof(int));
        p->deck_n = p->discard_n;
        p->discard_n = 0;
        shuffle(p->deck, p->deck_n, &p->rng);
    }
    return p->deck[--p->deck_n];
}

static void draw(de_player *p, int n) {
    for (int i = 0; i < n; i++) {
        int c = reveal(p);
        if (c < 0) return;
        p->hand[p->hand_n++] = c;
    }
}

enum { TO_DISCARD, TO_HAND, TO_DECK };
static int gain(de_game *g, de_player *p, int c, int to) {
    if (g->supply[c] <= 0) return 0;
    g->supply[c]--;
    if (to == TO_HAND) p->hand[p->hand_n++] = c;
    else if (to == TO_DECK) p->deck[p->deck_n++] = c;
    else p->discard[p->discard_n++] = c;
    return 1;
}

static int gainable(const de_game *g, int max_cost, int types, int *out) {
    int k = 0;
    for (int c = 0; c < DE_NUM_CARDS; c++)
        if (g->supply[c] > 0 && DE_CARDS[c].cost <= max_cost && (!types || IS(c, types)))
            out[k++] = c;
    return k;
}

int de_empty_piles(const de_game *g) {
    int e = 0;
    for (int c = 0; c < DE_NUM_CARDS; c++) if (g->supply[c] == 0) e++;
    return e;
}

/* ── decisions ── */
static int ask(de_game *g, int kind, int player, const int *opts, int n, int may_pass, int subject) {
    int ans = g->agents[player].decide(g->agents[player].ctx, g, kind, player, subject, opts, n, may_pass);
    int ok = (ans == -1 && may_pass);
    for (int i = 0; i < n && !ok; i++) if (opts[i] == ans) ok = 1;
    if (!ok) {
        g->error = 1;
        ans = may_pass ? -1 : opts[0];
    }
    uint64_t h = g->trace;
    int head[4] = {kind, player, subject, n};
    for (int i = 0; i < 4; i++) h = (h ^ (uint32_t)head[i]) * 0x100000001b3ULL;
    for (int i = 0; i < n; i++) h = (h ^ (uint32_t)opts[i]) * 0x100000001b3ULL;
    h = (h ^ (uint32_t)ans) * 0x100000001b3ULL;
    g->trace = h;
    return ans;
}

/* ── playing cards ── */
static void play_card(de_game *g, int i, int card) {
    de_player *p = &g->p[i];
    const de_card *c = &DE_CARDS[card];
    int opts[DE_NUM_CARDS], n, t;
    g->actions += c->actions;
    g->buys += c->buys;
    g->coins += c->coins;
    draw(p, c->cards);

    switch (card) {
    case DE_MERCHANT:
        g->merchants++;
        break;
    case DE_THRONE_ROOM:
        n = distinct(p->hand, p->hand_n, DE_ACTION, opts);
        t = ask(g, DE_THRONE, i, opts, n, 1, -1);
        if (t >= 0) {
            rm(p->hand, &p->hand_n, t);
            p->play[p->play_n++] = t;
            play_card(g, i, t);
            play_card(g, i, t);
        }
        break;
    case DE_CHAPEL:
        for (int k = 0; k < 4 && p->hand_n; k++) {
            n = distinct(p->hand, p->hand_n, 0, opts);
            t = ask(g, DE_CHAPEL_TRASH, i, opts, n, 1, k);
            if (t < 0) break;
            rm(p->hand, &p->hand_n, t);
            g->trash_n++;
        }
        break;
    case DE_CELLAR: {
        int k = 0;
        while (p->hand_n) {
            n = distinct(p->hand, p->hand_n, 0, opts);
            t = ask(g, DE_CELLAR_DISCARD, i, opts, n, 1, k);
            if (t < 0) break;
            rm(p->hand, &p->hand_n, t);
            p->discard[p->discard_n++] = t;
            k++;
        }
        draw(p, k);
        break;
    }
    case DE_MONEYLENDER:
        if (has(p->hand, p->hand_n, DE_COPPER)) {
            opts[0] = DE_COPPER;
            if (ask(g, DE_MONEYLENDER_TRASH, i, opts, 1, 1, -1) == DE_COPPER) {
                rm(p->hand, &p->hand_n, DE_COPPER);
                g->trash_n++;
                g->coins += 3;
            }
        }
        break;
    case DE_MINE:
        n = distinct(p->hand, p->hand_n, DE_TREASURE, opts);
        if (n) {
            t = ask(g, DE_MINE_TRASH, i, opts, n, 1, -1);
            if (t >= 0) {
                rm(p->hand, &p->hand_n, t);
                g->trash_n++;
                n = gainable(g, DE_CARDS[t].cost + 3, DE_TREASURE, opts);
                if (n) gain(g, p, ask(g, DE_MINE_GAIN, i, opts, n, 0, t), TO_HAND);
            }
        }
        break;
    case DE_REMODEL:
        if (p->hand_n) {
            n = distinct(p->hand, p->hand_n, 0, opts);
            t = ask(g, DE_REMODEL_TRASH, i, opts, n, 0, -1);
            rm(p->hand, &p->hand_n, t);
            g->trash_n++;
            n = gainable(g, DE_CARDS[t].cost + 2, 0, opts);
            if (n) gain(g, p, ask(g, DE_REMODEL_GAIN, i, opts, n, 0, t), TO_DISCARD);
        }
        break;
    case DE_WORKSHOP:
        n = gainable(g, 4, 0, opts);
        if (n) gain(g, p, ask(g, DE_WORKSHOP_GAIN, i, opts, n, 0, -1), TO_DISCARD);
        break;
    case DE_ARTISAN:
        n = gainable(g, 5, 0, opts);
        if (n) gain(g, p, ask(g, DE_ARTISAN_GAIN, i, opts, n, 0, -1), TO_HAND);
        if (p->hand_n) {
            n = distinct(p->hand, p->hand_n, 0, opts);
            t = ask(g, DE_ARTISAN_TOPDECK, i, opts, n, 0, -1);
            rm(p->hand, &p->hand_n, t);
            p->deck[p->deck_n++] = t;
        }
        break;
    case DE_HARBINGER:
        if (p->discard_n) {
            n = distinct(p->discard, p->discard_n, 0, opts);
            t = ask(g, DE_HARBINGER_TOPDECK, i, opts, n, 1, -1);
            if (t >= 0) {
                rm(p->discard, &p->discard_n, t);
                p->deck[p->deck_n++] = t;
            }
        }
        break;
    case DE_VASSAL:
        t = reveal(p);
        if (t >= 0) {
            p->discard[p->discard_n++] = t;
            if (IS(t, DE_ACTION)) {
                opts[0] = t;
                if (ask(g, DE_VASSAL_PLAY, i, opts, 1, 1, t) == t) {
                    p->discard_n--;
                    p->play[p->play_n++] = t;
                    play_card(g, i, t);
                }
            }
        }
        break;
    case DE_POACHER: {
        int k = de_empty_piles(g);
        for (int j = 0; j < k && p->hand_n; j++) {
            n = distinct(p->hand, p->hand_n, 0, opts);
            t = ask(g, DE_POACHER_DISCARD, i, opts, n, 0, k - j);
            rm(p->hand, &p->hand_n, t);
            p->discard[p->discard_n++] = t;
        }
        break;
    }
    case DE_LIBRARY: {
        int aside[DE_ZONE], an = 0;
        while (p->hand_n < 7) {
            t = reveal(p);
            if (t < 0) break;
            if (IS(t, DE_ACTION)) {
                opts[0] = t;
                if (ask(g, DE_LIBRARY_SKIP, i, opts, 1, 1, t) == t) {
                    aside[an++] = t;
                    continue;
                }
            }
            p->hand[p->hand_n++] = t;
        }
        for (int j = 0; j < an; j++) p->discard[p->discard_n++] = aside[j];
        break;
    }
    case DE_SENTRY: {
        int seen[2], sn = 0, kept[2], kn = 0;
        int a = reveal(p), b = reveal(p);
        if (a >= 0) seen[sn++] = a;
        if (b >= 0) seen[sn++] = b;
        int choice[3] = {DE_SENTRY_TRASH, DE_SENTRY_DISCARD, DE_SENTRY_KEEP};
        for (int j = 0; j < sn; j++) {
            int x = ask(g, DE_SENTRY_CARD, i, choice, 3, 0, seen[j]);
            if (x == DE_SENTRY_TRASH) g->trash_n++;
            else if (x == DE_SENTRY_DISCARD) p->discard[p->discard_n++] = seen[j];
            else kept[kn++] = seen[j];
        }
        if (kn == 2 && kept[0] != kept[1]) {
            int lo = kept[0] < kept[1] ? kept[0] : kept[1], hi = kept[0] ^ kept[1] ^ lo;
            opts[0] = lo; opts[1] = hi;
            int top = ask(g, DE_SENTRY_TOP, i, opts, 2, 0, -1);
            kept[0] = (top == kept[0]) ? kept[1] : kept[0];
            kept[1] = top;
        }
        for (int j = 0; j < kn; j++) p->deck[p->deck_n++] = kept[j];
        break;
    }
    case DE_COUNCIL_ROOM:
        for (int k = 1; k < g->nplayers; k++) draw(&g->p[(i + k) % g->nplayers], 1);
        break;
    case DE_BUREAUCRAT:
        gain(g, p, DE_SILVER, TO_DECK);
        for (int k = 1; k < g->nplayers; k++) {
            int j = (i + k) % g->nplayers;
            de_player *q = &g->p[j];
            if (has(q->hand, q->hand_n, DE_MOAT)) continue;
            n = distinct(q->hand, q->hand_n, DE_VICTORY, opts);
            if (n) {
                t = ask(g, DE_BUREAUCRAT_TOPDECK, j, opts, n, 0, -1);
                rm(q->hand, &q->hand_n, t);
                q->deck[q->deck_n++] = t;
            }
        }
        break;
    case DE_MILITIA:
        for (int k = 1; k < g->nplayers; k++) {
            int j = (i + k) % g->nplayers;
            de_player *q = &g->p[j];
            if (has(q->hand, q->hand_n, DE_MOAT)) continue;
            while (q->hand_n > 3) {
                n = distinct(q->hand, q->hand_n, 0, opts);
                t = ask(g, DE_MILITIA_DISCARD, j, opts, n, 0, q->hand_n - 3);
                rm(q->hand, &q->hand_n, t);
                q->discard[q->discard_n++] = t;
            }
        }
        break;
    case DE_WITCH:
        for (int k = 1; k < g->nplayers; k++) {
            de_player *q = &g->p[(i + k) % g->nplayers];
            if (!has(q->hand, q->hand_n, DE_MOAT)) gain(g, q, DE_CURSE, TO_DISCARD);
        }
        break;
    case DE_BANDIT:
        gain(g, p, DE_GOLD, TO_DISCARD);
        for (int k = 1; k < g->nplayers; k++) {
            int j = (i + k) % g->nplayers;
            de_player *q = &g->p[j];
            if (has(q->hand, q->hand_n, DE_MOAT)) continue;
            int seen[2], sn = 0;
            int a = reveal(q), b = reveal(q);
            if (a >= 0) seen[sn++] = a;
            if (b >= 0) seen[sn++] = b;
            int cand[2], cn = 0;
            for (int x = 0; x < sn; x++)
                if (IS(seen[x], DE_TREASURE) && seen[x] != DE_COPPER) cand[cn++] = seen[x];
            if (cn == 2 && cand[0] == cand[1]) cn = 1;
            if (cn == 2 && cand[0] > cand[1]) { int s = cand[0]; cand[0] = cand[1]; cand[1] = s; }
            if (cn) {
                t = cand[0];
                if (cn > 1) t = ask(g, DE_BANDIT_TRASH, j, cand, cn, 0, -1);
                rm(seen, &sn, t);
                g->trash_n++;
            }
            for (int x = 0; x < sn; x++) q->discard[q->discard_n++] = seen[x];
        }
        break;
    }
}

/* ── turns and the game ── */
static void buy_phase(de_game *g, int i);
static void cleanup(de_game *g, int i);

static void action_phase(de_game *g, int i) {
    de_player *p = &g->p[i];
    int opts[DE_NUM_CARDS], n;
    while (g->actions > 0) {
        n = distinct(p->hand, p->hand_n, DE_ACTION, opts);
        if (!n) break;
        int a = ask(g, DE_PLAY_ACTION, i, opts, n, 1, -1);
        if (a < 0) break;
        g->actions--;
        rm(p->hand, &p->hand_n, a);
        p->play[p->play_n++] = a;
        play_card(g, i, a);
    }
}

static void treasures_buys_cleanup(de_game *g, int i) {
    de_player *p = &g->p[i];
    int silver = 0, rest = 0;
    for (int k = 0; k < p->hand_n; k++) {
        int c = p->hand[k];
        if (IS(c, DE_TREASURE)) {
            p->play[p->play_n++] = c;
            g->coins += DE_CARDS[c].coins;
            if (c == DE_SILVER && !silver) { silver = 1; g->coins += g->merchants; }
        } else {
            p->hand[rest++] = c;
        }
    }
    p->hand_n = rest;
    buy_phase(g, i);
    cleanup(g, i);
}

static void turn(de_game *g, int i) {
    g->current = i;
    g->actions = 1; g->buys = 1; g->coins = 0; g->merchants = 0;
    action_phase(g, i);
    treasures_buys_cleanup(g, i);
}

static void buy_card(de_game *g, int i, int b) {
    de_player *p = &g->p[i];
    g->supply[b]--;
    p->discard[p->discard_n++] = b;
    g->coins -= DE_CARDS[b].cost;
    g->buys--;
}

static void buy_phase(de_game *g, int i) {
    int opts[DE_NUM_CARDS], n;
    while (g->buys > 0) {
        n = gainable(g, g->coins, 0, opts);
        if (!n) break;
        int b = ask(g, DE_BUY, i, opts, n, 1, -1);
        if (b < 0) break;
        buy_card(g, i, b);
    }
}

static void cleanup(de_game *g, int i) {
    de_player *p = &g->p[i];
    memcpy(p->discard + p->discard_n, p->play, (size_t)p->play_n * sizeof(int));
    p->discard_n += p->play_n;
    memcpy(p->discard + p->discard_n, p->hand, (size_t)p->hand_n * sizeof(int));
    p->discard_n += p->hand_n;
    p->play_n = p->hand_n = 0;
    draw(p, 5);
    p->turns++;
}

static int game_over(const de_game *g) {
    return g->supply[DE_PROVINCE] == 0 || de_empty_piles(g) >= 3;
}

void de_init(de_game *g, const int *kingdom, int kn, int nplayers, uint64_t seed) {
    memset(g, 0, sizeof(*g));
    g->nplayers = nplayers;
    int victory = nplayers == 2 ? 8 : 12;
    for (int c = 0; c < DE_NUM_CARDS; c++) g->supply[c] = -1;
    g->supply[DE_COPPER] = 60 - 7 * nplayers;
    g->supply[DE_SILVER] = 40;
    g->supply[DE_GOLD] = 30;
    g->supply[DE_ESTATE] = g->supply[DE_DUCHY] = g->supply[DE_PROVINCE] = victory;
    g->supply[DE_CURSE] = 10 * (nplayers - 1);
    for (int k = 0; k < kn; k++)
        g->supply[kingdom[k]] = IS(kingdom[k], DE_VICTORY) ? victory : 10;

    uint64_t master = seed ? seed : 1;
    for (int i = 0; i < nplayers; i++) {
        de_player *p = &g->p[i];
        p->rng = de_rng_next(&master);
        if (!p->rng) p->rng = 1;
        for (int k = 0; k < 7; k++) p->deck[k] = DE_COPPER;
        for (int k = 7; k < 10; k++) p->deck[k] = DE_ESTATE;
        p->deck_n = 10;
        shuffle(p->deck, p->deck_n, &p->rng);
        draw(p, 5);
    }
    g->trace = 0xcbf29ce484222325ULL;
}

void de_run(de_game *g, const de_agent *agents) {
    g->agents = agents;
    while (!g->over) {
        int ended = 0;
        for (int i = 0; i < g->nplayers; i++) {
            turn(g, i);
            if (game_over(g)) { g->over = 1; ended = 1; break; }
        }
        if (!ended && g->p[g->nplayers - 1].turns >= DE_ROUND_CAP) g->over = g->capped = 1;
    }
}

/* ── rollouts: continue a copied game (search agents) ── */

static void run_from(de_game *g, int next) {
    /* the round in progress, from seat `next`, then whole rounds as in de_run */
    for (int i = next; i < g->nplayers && !g->over; i++) {
        turn(g, i);
        if (game_over(g)) g->over = 1;
    }
    if (!g->over && g->p[g->nplayers - 1].turns >= DE_ROUND_CAP) g->over = g->capped = 1;
    if (!g->over) de_run(g, g->agents);
}

void de_resume_buy(de_game *g, const de_agent *agents, int first) {
    int i = g->current;
    g->agents = agents;
    if (first >= 0) {
        buy_card(g, i, first);
        buy_phase(g, i);
    }
    cleanup(g, i);
    if (game_over(g)) { g->over = 1; return; }
    run_from(g, i + 1);
}

void de_resume_action(de_game *g, const de_agent *agents, int first, int whole_game) {
    int i = g->current;
    de_player *p = &g->p[i];
    g->agents = agents;
    if (first >= 0) {
        g->actions--;
        rm(p->hand, &p->hand_n, first);
        p->play[p->play_n++] = first;
        play_card(g, i, first);
        action_phase(g, i);
    }
    treasures_buys_cleanup(g, i);
    if (game_over(g)) { g->over = 1; return; }
    if (whole_game) run_from(g, i + 1);
}

void de_hide(de_game *g, int me, uint64_t seed) {
    uint64_t master = seed ? seed : 1;
    for (int i = 0; i < g->nplayers; i++) {
        de_player *p = &g->p[i];
        p->rng = de_rng_next(&master) | 1;
        if (i != me) {                    /* their hand is unseen: back into the deck, redraw */
            int h = p->hand_n;
            memcpy(p->deck + p->deck_n, p->hand, (size_t)h * sizeof(int));
            p->deck_n += h;
            p->hand_n = 0;
            shuffle(p->deck, p->deck_n, &p->rng);
            draw(p, h);
        } else {
            shuffle(p->deck, p->deck_n, &p->rng);
        }
    }
}

int de_count(const de_player *p, int card) {
    int n = 0;
    for (int k = 0; k < p->deck_n; k++) n += p->deck[k] == card;
    for (int k = 0; k < p->hand_n; k++) n += p->hand[k] == card;
    for (int k = 0; k < p->discard_n; k++) n += p->discard[k] == card;
    for (int k = 0; k < p->play_n; k++) n += p->play[k] == card;
    return n;
}

int de_total(const de_player *p) {
    return p->deck_n + p->hand_n + p->discard_n + p->play_n;
}

int de_score(const de_game *g, int i) {
    const de_player *p = &g->p[i];
    int vp = 0;
    for (int c = 0; c < DE_NUM_CARDS; c++) {
        int n = de_count(p, c);
        if (n) vp += n * DE_CARDS[c].vp;
    }
    return vp + de_count(p, DE_GARDENS) * (de_total(p) / 10);
}

int de_winners(const de_game *g) {
    int best = -1000, fewest = 1 << 30, mask = 0, s[DE_MAX_PLAYERS];
    for (int i = 0; i < g->nplayers; i++) {
        s[i] = de_score(g, i);
        if (s[i] > best) best = s[i];
    }
    for (int i = 0; i < g->nplayers; i++)
        if (s[i] == best && g->p[i].turns < fewest) fewest = g->p[i].turns;
    for (int i = 0; i < g->nplayers; i++)
        if (s[i] == best && g->p[i].turns == fewest) mask |= 1 << i;
    return mask;
}
