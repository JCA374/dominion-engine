# dominion-engine

Dominion base set (2nd edition) with **Steam Dominion's rules**, for training any kind of model
on the same game the Steam autopilot plays. [RULES.md](RULES.md) lists the rules and how they
differ from the first engine.

Two engines, one game:

* `dominion_engine/` (Python): the reference. The game is a generator that yields every
  decision and is resumed with the answer, so a scripted bot, an evolved genome, a person in a
  GUI or a neural net stepping it like an RL environment can all play it.
* `c/` (C): the same rules, about 60x faster (measured on GA agents), for training. An agent is a decide callback;
  a trainer compiles `c/engine.c` together with its own agent into one shared library.

Both use the same xorshift64 RNG and ask the same decisions in the same order, so the same
seed and the same answers give the same game. `tests/test_parity.py` checks this on random
bots, random kingdoms and 2-4 players, and checks every kind of decision comes up.

## Use

```python
from dominion_engine import play, Game
from dominion_engine.bots import BigMoney, RandomBot

r = play(["Smithy", "Witch", ...], [BigMoney(), RandomBot(7)], seed=1)
r.scores, r.turns, r.winners          # winners: [i], or [0, 1] for a shared win

g = Game(kingdom, seed=1)              # step it yourself
gen = g.run()
d = next(gen)                          # a Decision: kind, player, options, may_pass, subject
d = gen.send(answer)                   # ... until StopIteration(value=Result)
```

An agent is any object with `decide(game, decision) -> int` (an option, or -1 to pass when
`may_pass`). During a decision `game.players[i]` (deck/hand/discard/play), `game.supply`,
`game.actions/buys/coins` and `game.turn_number(i)` are the state to read.

In C (see `c/engine.h`):

```c
int my_decide(void *ctx, const de_game *g, int kind, int player, int subject,
              const int *opts, int n, int may_pass);
de_agent agents[2] = {{my_decide, &me}, {my_decide, &other}};
de_game g; de_init(&g, kingdom_ids, 10, 2, seed); de_run(&g, agents);
de_score(&g, 0); de_winners(&g);
```

Build a trainer's library with `dominion_engine.fast.build(out, [my_agent.c])` (or gcc
`-I engine/c engine/c/engine.c my_agent.c`).

## In a trainer

Add this repo as a git submodule named `engine/` and put it on `sys.path`
(`trainers/ga-v1/core/__init__.py` shows how). Pinning the submodule records the exact rules a
model was trained on. A trainer whose agent exists in Python and C should have its own parity
test (`trainers/ga-v1/tests/test_agent_parity.py`).

```bash
python -m pytest -q tests      # rules on hand-made positions + Python/C parity
```

**A rules change goes into both `game.py` and `c/engine.c`**, with a test in
`tests/test_rules.py`; the parity test then shows whether the two still agree.
