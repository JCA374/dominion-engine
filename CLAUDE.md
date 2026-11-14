# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

`JCA374/dominion-engine`: Dominion base set (2nd edition) with Steam Dominion's rules, shared by every trainer
(`../trainers/*`, as a git submodule `engine/`) and, through them, the Steam autopilot. README.md has the API,
RULES.md the rules, the decision table and how they differ from the first engine.

```bash
python -m pytest -q tests                  # all tests (~5 s; builds build/libdominion_engine.so)
python -m pytest -q tests/test_rules.py -k Bandit
```

- Two engines, one game: `dominion_engine/game.py` (Python reference, a generator yielding `Decision`s) and
  `c/engine.c` (fast, agents are decide callbacks). Same RNG (`rng.py` = `de_rng_next`), same decisions in the same
  order, same card movements (discard-pile order decides shuffles). **Every rules change goes into both**, plus a
  test in `tests/test_rules.py`; `tests/test_parity.py` must stay at zero mismatches and reach every decision kind.
- Card ids (`cards.py` = `engine.h` enum) are permanent: models and genomes store them. New cards get new ids.
- Decisions: options sorted ascending and distinct, pass = -1. Multi-card choices are one card per decision.
- Trainers pin a commit of this repo; changing rules changes what models trained earlier learned, so note it in
  RULES.md. After a change: push, then bump the submodule in each trainer and rerun its tests (and
  `policy.bench --store` in ga-v1, since stored benchmarks were measured on the old rules).
