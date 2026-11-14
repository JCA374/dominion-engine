/*
 * engine.h -- fast Dominion engine, base set (2nd edition) with Steam Dominion's rules.
 *
 * The C twin of dominion_engine/game.py: same rules, same decisions in the same order, same
 * RNG, so the same seed and the same answers give the same game (tests/test_parity.py).
 *
 * An agent is a callback that answers decisions. Trainers compile engine.c together with their
 * own agent (e.g. an evolved genome) into one shared library; see README.md.
 */
#ifndef DOMINION_ENGINE_H
#define DOMINION_ENGINE_H

#include <stdint.h>

/* Card ids (= dominion_engine/cards.py) */
enum {
    DE_COPPER, DE_SILVER, DE_GOLD, DE_ESTATE, DE_DUCHY, DE_PROVINCE, DE_VILLAGE, DE_SMITHY,
    DE_MARKET, DE_LABORATORY, DE_FESTIVAL, DE_CHAPEL, DE_THRONE_ROOM, DE_COUNCIL_ROOM,
    DE_MONEYLENDER, DE_GARDENS, DE_MINE, DE_MERCHANT, DE_CURSE, DE_MILITIA, DE_WITCH, DE_MOAT,
    DE_CELLAR, DE_HARBINGER, DE_VASSAL, DE_WORKSHOP, DE_BUREAUCRAT, DE_POACHER, DE_REMODEL,
    DE_LIBRARY, DE_SENTRY, DE_ARTISAN, DE_BANDIT,
    DE_NUM_CARDS
};

/* Type flags */
#define DE_TREASURE 1
#define DE_VICTORY  2
#define DE_ACTION   4
#define DE_ATTACK   8
#define DE_REACTION 16
#define DE_CURSE_T  32

typedef struct {
    const char *name;
    int cost, types, coins, vp, cards, actions, buys;
} de_card;

extern const de_card DE_CARDS[DE_NUM_CARDS];

/* Decision kinds (= game.py) */
enum {
    DE_PLAY_ACTION, DE_BUY, DE_THRONE, DE_CHAPEL_TRASH, DE_MONEYLENDER_TRASH, DE_MINE_TRASH,
    DE_MINE_GAIN, DE_REMODEL_TRASH, DE_REMODEL_GAIN, DE_WORKSHOP_GAIN, DE_ARTISAN_GAIN,
    DE_ARTISAN_TOPDECK, DE_CELLAR_DISCARD, DE_HARBINGER_TOPDECK, DE_VASSAL_PLAY,
    DE_LIBRARY_SKIP, DE_SENTRY_CARD, DE_SENTRY_TOP, DE_POACHER_DISCARD, DE_MILITIA_DISCARD,
    DE_BUREAUCRAT_TOPDECK, DE_BANDIT_TRASH,
    DE_NUM_KINDS
};
enum { DE_SENTRY_TRASH, DE_SENTRY_DISCARD, DE_SENTRY_KEEP };

#define DE_MAX_PLAYERS 4
#define DE_ZONE 320          /* more cards than a whole 2-4 player supply */
#define DE_ROUND_CAP 100     /* = rules.ROUND_CAP */

typedef struct {
    int deck[DE_ZONE], deck_n;        /* top of deck = deck[deck_n-1] */
    int hand[DE_ZONE], hand_n;
    int discard[DE_ZONE], discard_n;
    int play[DE_ZONE], play_n;
    int turns;                        /* turns completed */
    uint64_t rng;
} de_player;

typedef struct de_game de_game;

/* Answer one of opts[0..n-1], or -1 if may_pass. `player` is who decides. */
typedef int (*de_decide_fn)(void *ctx, const de_game *g, int kind, int player, int subject,
                            const int *opts, int n, int may_pass);
typedef struct { de_decide_fn decide; void *ctx; } de_agent;

struct de_game {
    int nplayers;
    int supply[DE_NUM_CARDS];         /* -1 = not in this game */
    de_player p[DE_MAX_PLAYERS];
    int trash_n;
    int current;
    int actions, buys, coins, merchants;
    uint64_t trace;
    int over, capped, error;
    const de_agent *agents;
};

void de_init(de_game *g, const int *kingdom, int kn, int nplayers, uint64_t seed);
void de_run(de_game *g, const de_agent *agents);        /* agents[nplayers]; plays to the end */
int de_score(const de_game *g, int player);
int de_winners(const de_game *g);                        /* bit i set = seat i won (or shared) */
int de_empty_piles(const de_game *g);
int de_count(const de_player *p, int card);              /* copies owned */
int de_total(const de_player *p);                        /* cards owned */
uint64_t de_rng_next(uint64_t *state);

#endif
