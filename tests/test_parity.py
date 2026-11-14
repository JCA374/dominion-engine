"""The Python and C engines play identical games: same scores, turns, winners and every decision."""
import random

from dominion_engine import cards, fast, play
from dominion_engine.bots import BigMoney, RandomBot
from dominion_engine.game import NUM_KINDS


def _case(s):
    rnd = random.Random(s)
    kingdom = rnd.sample(cards.KINGDOM_CARDS, 10)
    nplayers = rnd.choice([2, 2, 2, 3, 4])
    bots = [(rnd.choice([fast.BIG_MONEY, fast.RANDOM, fast.RANDOM]), rnd.getrandbits(64))
            for _ in range(nplayers)]
    return kingdom, rnd.getrandbits(64), bots


class Counting:
    def __init__(self, agent, seen):
        self.agent, self.seen = agent, seen

    def decide(self, game, d):
        self.seen.add(d.kind)
        return self.agent.decide(game, d)


def test_card_tables_match():
    for c, row in zip(cards.CARDS, fast.card_table()):
        assert row == (c.cost, c.types, c.coins, c.vp, c.cards, c.actions, c.buys), c.name


def test_python_and_c_play_the_same_games():
    seen = set()
    for s in range(1500):
        kingdom, seed, bots = _case(s)
        agents = [Counting(BigMoney() if kind == fast.BIG_MONEY else RandomBot(bs), seen)
                  for kind, bs in bots]
        py = play(kingdom, agents, seed)
        c = fast.play_bots(kingdom, seed, bots)
        assert not c["error"]
        assert (py.scores, py.turns, py.winners, py.capped, py.trace) == \
            (c["scores"], c["turns"], c["winners"], c["capped"], c["trace"]), (s, kingdom, bots)
    assert seen == set(range(NUM_KINDS)), f"decision kinds never reached: {set(range(NUM_KINDS)) - seen}"
