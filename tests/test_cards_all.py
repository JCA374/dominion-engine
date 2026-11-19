"""Every one of the 26 kingdom cards works in whole games: with random players (both engines,
identical games) the card is bought and played, and every decision it causes is asked."""
import random

import pytest

from dominion_engine import cards as C, fast, play
from dominion_engine.bots import RandomBot
from dominion_engine.cards import ID
from dominion_engine.game import (ARTISAN_GAIN, ARTISAN_TOPDECK, BANDIT_TRASH, BUREAUCRAT_TOPDECK, BUY,
                                  CELLAR_DISCARD, CHAPEL_TRASH, HARBINGER_TOPDECK, LIBRARY_SKIP,
                                  MILITIA_DISCARD, MINE_GAIN, MINE_TRASH, MONEYLENDER_TRASH, PLAY_ACTION,
                                  POACHER_DISCARD, REMODEL_GAIN, REMODEL_TRASH, SENTRY_CARD, SENTRY_TOP,
                                  THRONE, VASSAL_PLAY, WORKSHOP_GAIN)

ASKS = {"Throne Room": [THRONE], "Chapel": [CHAPEL_TRASH], "Moneylender": [MONEYLENDER_TRASH],
        "Mine": [MINE_TRASH, MINE_GAIN], "Remodel": [REMODEL_TRASH, REMODEL_GAIN], "Workshop": [WORKSHOP_GAIN],
        "Artisan": [ARTISAN_GAIN, ARTISAN_TOPDECK], "Cellar": [CELLAR_DISCARD], "Harbinger": [HARBINGER_TOPDECK],
        "Vassal": [VASSAL_PLAY], "Library": [LIBRARY_SKIP], "Sentry": [SENTRY_CARD, SENTRY_TOP],
        "Poacher": [POACHER_DISCARD], "Militia": [MILITIA_DISCARD], "Bureaucrat": [BUREAUCRAT_TOPDECK],
        "Bandit": [BANDIT_TRASH]}
# Bandit asks only when the victim reveals Silver and Gold together: too rare between random players.
# test_rules.py and test_parity.py (which reaches every kind) cover it.
RARE = {"Bandit": {BANDIT_TRASH}}
# cards whose question only one card in the base set asks: a reached kind proves this card asked it
FILLER = ["Village", "Smithy", "Market", "Laboratory", "Festival", "Council Room", "Merchant", "Witch",
          "Moat", "Gardens"]


class Watch:
    def __init__(self, seed, seen):
        self.bot, self.seen = RandomBot(seed), seen

    def decide(self, game, d):
        a = self.bot.decide(game, d)
        self.seen.add((d.kind, a))
        return a


@pytest.mark.parametrize("name", C.KINGDOM_CARDS)
def test_card_in_whole_games(name):
    rnd = random.Random(name)
    filler = [f for f in FILLER if f != name]
    seen, played = set(), 0
    for s in range(40):
        kingdom = [name] + rnd.sample(filler, 9)
        seed, bots = rnd.getrandbits(64), [rnd.getrandbits(64) | 1 for _ in range(2)]
        py = play(kingdom, [Watch(b, seen) for b in bots], seed)
        c = fast.play_bots(kingdom, seed, [(fast.RANDOM, b) for b in bots])
        assert not c["error"]
        assert (py.scores, py.turns, py.winners, py.trace) == \
            (c["scores"], c["turns"], c["winners"], c["trace"]), (name, s)
    assert (BUY, ID[name]) in seen, f"{name} never bought"
    if C.CARDS[ID[name]].is_action:
        assert (PLAY_ACTION, ID[name]) in seen, f"{name} never played"
    kinds = {k for k, _ in seen}
    for k in set(ASKS.get(name, [])) - RARE.get(name, set()):
        assert k in kinds, f"{name}: decision kind {k} never asked"


def test_a_game_resumed_inside_the_action_phase_finishes_the_same():
    """de_resume_action (turn planners): a copy taken at a PLAY_ACTION decision and resumed with
    the same answer replays the rest of the game exactly."""
    import ctypes
    lib = fast.lib()
    lib.de_resume_action_check.argtypes = [ctypes.POINTER(ctypes.c_int), ctypes.c_int, ctypes.c_uint64,
                                           ctypes.c_int, ctypes.POINTER(ctypes.c_int)]
    k = fast.kingdom_ids(["Village", "Smithy", "Market", "Throne Room", "Militia", "Moat", "Cellar",
                          "Library", "Sentry", "Vassal"])
    kk = (ctypes.c_int * len(k))(*k)
    checked = 0
    for seed in range(1, 60):
        for at in (1, 3, 8, 15):
            out = (ctypes.c_int * 4)()
            lib.de_resume_action_check(kk, len(k), seed, at, out)
            if out[3]:
                assert list(out[:3]) == [1, 1, 1], (seed, at)
                checked += 1
    assert checked > 150
