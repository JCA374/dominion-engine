"""Steam rules and every card, on hand-made positions (Python engine; parity covers C)."""
import pytest

from dominion_engine import cards as C, rules
from dominion_engine.cards import ID
from dominion_engine.game import (ARTISAN_GAIN, ARTISAN_TOPDECK, BANDIT_TRASH, BUREAUCRAT_TOPDECK,
                                  CELLAR_DISCARD, CHAPEL_TRASH, MINE_GAIN, MINE_TRASH, MONEYLENDER_TRASH, HARBINGER_TOPDECK, LIBRARY_SKIP, MILITIA_DISCARD,
                                  POACHER_DISCARD, REMODEL_GAIN, REMODEL_TRASH, SENTRY_CARD,
                                  SENTRY_DISCARD, SENTRY_KEEP, SENTRY_TOP, SENTRY_TRASH, THRONE,
                                  VASSAL_PLAY, WORKSHOP_GAIN, Game, play)

KINGDOM = ["Cellar", "Moat", "Village", "Merchant", "Bureaucrat", "Council Room", "Witch",
           "Throne Room", "Gardens", "Bandit"]


def ids(*names):
    return [ID[n] for n in names]


def position(hand, deck=(), discard=(), opp_hand=("Copper",) * 5, kingdom=KINGDOM):
    """Player 0 to move with the given zones (deck listed top first)."""
    g = Game(kingdom, seed=1)
    p, q = g.players
    p.hand, p.deck, p.discard = ids(*hand), ids(*reversed(deck)), ids(*discard)
    q.hand, q.deck, q.discard = ids(*opp_hand), [], []
    g.actions, g.buys, g.coins = 0, 1, 0
    return g


def run(g, card, answers=()):
    """Play `card` from player 0's hand; answers are given in order. Returns the decisions."""
    p = g.players[0]
    p.hand.remove(ID[card])
    p.play.append(ID[card])
    gen, asked, answers = g._play(0, ID[card]), [], list(answers)
    try:
        d = next(gen)
        while True:
            asked.append(d)
            d = gen.send(answers.pop(0))
    except StopIteration:
        pass
    assert not answers, "unused answers"
    return asked


def names(cards):
    return sorted(C.NAME[c] for c in cards)


# -- setup and end of game -----------------------------------------------------------------

def test_two_player_supply_is_steams():
    s = rules.supply(ids(*KINGDOM), 2)
    assert s[C.PROVINCE] == s[C.DUCHY] == s[C.ESTATE] == 8
    assert s[C.COPPER] == 46 and s[C.SILVER] == 40 and s[C.GOLD] == 30
    assert s[C.GARDENS] == 8 and s[C.VILLAGE] == 10
    assert s[C.SMITHY] == -1


def test_curse_pile_always_in_supply():
    s = rules.supply(ids("Village", "Smithy"), 2)
    assert s[C.CURSE] == 10
    assert rules.supply([], 3)[C.CURSE] == 20


def test_three_empty_piles_end_the_game_and_curses_count():
    s = rules.supply(ids(*KINGDOM), 2)
    s[C.CURSE] = s[C.VILLAGE] = 0
    assert not rules.game_over(s)
    s[C.ESTATE] = 0
    assert rules.game_over(s)


def test_tie_goes_to_fewer_turns_then_shared():
    assert rules.winners([30, 30], [20, 19]) == [1]
    assert rules.winners([30, 30], [20, 20]) == [0, 1]
    assert rules.winners([31, 30], [20, 19]) == [0]


def test_no_turn_40_cap():
    class Passive:
        def decide(self, game, d):
            return -1 if d.may_pass else d.options[0]
    r = play(KINGDOM, [Passive(), Passive()], seed=3)
    assert r.capped and r.turns == [rules.ROUND_CAP] * 2 and rules.ROUND_CAP > 40


def test_gardens_count_every_ten_cards():
    assert rules.score(ids("Gardens") + ids("Copper") * 18) == 1
    assert rules.score(ids("Gardens", "Gardens") + ids("Copper") * 28) == 6


# -- cards ---------------------------------------------------------------------------------

def test_council_room_gives_each_opponent_a_card():
    g = position(["Council Room"], deck=["Copper"] * 4)
    g.players[1].deck = ids("Gold")
    run(g, "Council Room")
    assert len(g.players[0].hand) == 4 and g.buys == 2
    assert names(g.players[1].hand) == sorted(["Copper"] * 5 + ["Gold"])


def test_throne_room_merchant_gives_two_on_first_silver():
    g = position(["Throne Room", "Merchant", "Silver", "Silver"], deck=["Estate", "Estate"])
    run(g, "Throne Room", [ID["Merchant"]])
    assert g.merchants == 2


def test_moat_blocks_witch():
    g = position(["Witch"], deck=["Copper", "Copper"], opp_hand=["Moat"] + ["Copper"] * 4)
    run(g, "Witch")
    assert C.CURSE not in g.players[1].discard


def test_cellar_discards_and_draws_as_many():
    g = position(["Cellar", "Estate", "Duchy", "Copper"], deck=["Gold", "Silver", "Copper"])
    asked = run(g, "Cellar", [C.ESTATE, C.DUCHY, -1])
    assert [d.kind for d in asked] == [CELLAR_DISCARD] * 3
    assert names(g.players[0].hand) == ["Copper", "Gold", "Silver"] and g.actions == 1


def test_harbinger_puts_a_discarded_card_on_top():
    g = position(["Harbinger"], deck=["Copper", "Estate"], discard=["Gold", "Copper"])
    asked = run(g, "Harbinger", [C.GOLD])
    assert asked[0].kind == HARBINGER_TOPDECK and asked[0].options == (C.COPPER, C.GOLD)
    assert g.players[0].deck[-1] == C.GOLD


def test_vassal_may_play_the_discarded_action():
    g = position(["Vassal"], deck=["Smithy", "Copper", "Copper", "Copper"],
                 kingdom=KINGDOM[:-1] + ["Smithy"])
    asked = run(g, "Vassal", [C.SMITHY])
    assert asked[0].kind == VASSAL_PLAY
    assert g.coins == 2 and len(g.players[0].hand) == 3 and C.SMITHY in g.players[0].play


def test_workshop_gains_up_to_four():
    g = position(["Workshop"], kingdom=KINGDOM[:-1] + ["Workshop"])
    asked = run(g, "Workshop", [C.GARDENS])
    assert asked[0].kind == WORKSHOP_GAIN
    assert all(C.CARDS[o].cost <= 4 for o in asked[0].options)
    assert g.players[0].discard == [C.GARDENS]


def test_bureaucrat_silver_on_deck_and_victim_topdecks_victory():
    g = position(["Bureaucrat"], opp_hand=["Estate", "Duchy", "Copper", "Copper", "Copper"])
    asked = run(g, "Bureaucrat", [C.DUCHY])
    assert g.players[0].deck[-1] == C.SILVER
    assert asked[0].kind == BUREAUCRAT_TOPDECK and asked[0].player == 1
    assert g.players[1].deck[-1] == C.DUCHY


def test_poacher_discards_per_empty_pile():
    g = position(["Poacher", "Estate", "Copper"], deck=["Silver"],
                 kingdom=KINGDOM[:-1] + ["Poacher"])
    g.supply[C.CURSE] = g.supply[C.VILLAGE] = 0
    asked = run(g, "Poacher", [C.ESTATE, C.COPPER])
    assert [d.kind for d in asked] == [POACHER_DISCARD] * 2
    assert g.players[0].hand == [C.SILVER] and g.coins == 1


def test_remodel_trash_and_gain_up_to_two_more():
    g = position(["Remodel", "Gold"], kingdom=KINGDOM[:-1] + ["Remodel"])
    asked = run(g, "Remodel", [C.GOLD, C.PROVINCE])
    assert [d.kind for d in asked] == [REMODEL_TRASH, REMODEL_GAIN]
    assert g.players[0].discard == [C.PROVINCE] and g.trash == [C.GOLD]


def test_library_draws_to_seven_and_may_skip_actions():
    g = position(["Library", "Copper", "Copper"],
                 deck=["Village", "Copper", "Copper", "Copper", "Estate", "Silver"],
                 kingdom=KINGDOM[:-1] + ["Library"])
    asked = run(g, "Library", [C.VILLAGE])
    assert asked[0].kind == LIBRARY_SKIP
    assert len(g.players[0].hand) == 7 and g.players[0].discard == [C.VILLAGE]


def test_sentry_trash_discard_keep_and_order():
    g = position(["Sentry"], deck=["Copper", "Estate", "Curse", "Gold"],
                 kingdom=KINGDOM[:-1] + ["Sentry"])
    asked = run(g, "Sentry", [SENTRY_TRASH, SENTRY_DISCARD])
    assert [d.kind for d in asked] == [SENTRY_CARD] * 2
    assert g.trash == [C.ESTATE] and g.players[0].discard == [C.CURSE]

    g = position(["Sentry"], deck=["Copper", "Silver", "Gold"], kingdom=KINGDOM[:-1] + ["Sentry"])
    asked = run(g, "Sentry", [SENTRY_KEEP, SENTRY_KEEP, C.SILVER])
    assert asked[-1].kind == SENTRY_TOP
    assert g.players[0].deck[-2:] == [C.GOLD, C.SILVER]


def test_artisan_gains_to_hand_and_topdecks():
    g = position(["Artisan", "Estate"], kingdom=KINGDOM[:-1] + ["Artisan"])
    asked = run(g, "Artisan", [C.DUCHY, C.ESTATE])
    assert [d.kind for d in asked] == [ARTISAN_GAIN, ARTISAN_TOPDECK]
    assert all(C.CARDS[o].cost <= 5 for o in asked[0].options)
    assert g.players[0].hand == [C.DUCHY] and g.players[0].deck[-1] == C.ESTATE


def test_bandit_gains_gold_and_trashes_victims_best_choice():
    g = position(["Bandit"])
    g.players[1].deck = ids("Silver", "Gold")
    asked = run(g, "Bandit", [C.SILVER])
    assert g.players[0].discard == [C.GOLD]
    assert asked[0].kind == BANDIT_TRASH and asked[0].player == 1
    assert g.trash == [C.SILVER] and g.players[1].discard == [C.GOLD]


def test_bandit_never_trashes_copper():
    g = position(["Bandit"])
    g.players[1].deck = ids("Copper", "Estate")
    run(g, "Bandit")
    assert g.trash == [] and names(g.players[1].discard) == ["Copper", "Estate"]


def test_militia_victim_discards_down_to_three():
    g = position(["Militia"], kingdom=KINGDOM[:-1] + ["Militia"],
                 opp_hand=["Estate", "Copper", "Copper", "Silver", "Gold"])
    asked = run(g, "Militia", [C.ESTATE, C.COPPER])
    assert [d.kind for d in asked] == [MILITIA_DISCARD] * 2 and asked[0].player == 1
    assert names(g.players[1].hand) == ["Copper", "Gold", "Silver"] and g.coins == 2


def test_throne_room_may_decline():
    g = position(["Throne Room", "Village"])
    asked = run(g, "Throne Room", [-1])
    assert asked[0].kind == THRONE and C.VILLAGE in g.players[0].hand


# -- the plain cards (no choices) and the "you may" trashers -------------------------------

def test_market_lab_festival_give_their_plus():
    g = position(["Market"], deck=["Gold", "Silver"], kingdom=KINGDOM[:-1] + ["Market"])
    run(g, "Market")
    assert (g.actions, g.buys, g.coins) == (1, 2, 1) and g.players[0].hand == [C.GOLD]
    g = position(["Laboratory"], deck=["Gold", "Silver", "Copper"], kingdom=KINGDOM[:-1] + ["Laboratory"])
    run(g, "Laboratory")
    assert (g.actions, g.buys, g.coins) == (1, 1, 0) and names(g.players[0].hand) == ["Gold", "Silver"]
    g = position(["Festival"], deck=["Gold"], kingdom=KINGDOM[:-1] + ["Festival"])
    run(g, "Festival")
    assert (g.actions, g.buys, g.coins) == (2, 2, 2) and g.players[0].hand == []


def test_chapel_trashes_up_to_four_and_may_stop():
    hand = ["Chapel", "Estate", "Estate", "Copper", "Copper", "Curse"]
    g = position(hand, kingdom=KINGDOM[:-1] + ["Chapel"])
    asked = run(g, "Chapel", [C.ESTATE, C.ESTATE, C.CURSE, C.COPPER])
    assert [d.kind for d in asked] == [CHAPEL_TRASH] * 4 and [d.subject for d in asked] == [0, 1, 2, 3]
    assert g.players[0].hand == [C.COPPER] and names(g.trash) == ["Copper", "Curse", "Estate", "Estate"]
    g = position(hand, kingdom=KINGDOM[:-1] + ["Chapel"])
    run(g, "Chapel", [C.CURSE, -1])
    assert g.trash == [C.CURSE] and len(g.players[0].hand) == 4
    g = position(["Chapel"], kingdom=KINGDOM[:-1] + ["Chapel"])
    assert run(g, "Chapel") == []                     # empty hand: nothing to ask


def test_moneylender_trashes_a_copper_for_three_or_declines():
    g = position(["Moneylender", "Copper", "Copper"], kingdom=KINGDOM[:-1] + ["Moneylender"])
    asked = run(g, "Moneylender", [C.COPPER])
    assert asked[0].kind == MONEYLENDER_TRASH and asked[0].may_pass
    assert g.coins == 3 and g.trash == [C.COPPER] and g.players[0].hand == [C.COPPER]
    g = position(["Moneylender", "Copper"], kingdom=KINGDOM[:-1] + ["Moneylender"])
    run(g, "Moneylender", [-1])
    assert g.coins == 0 and g.trash == []
    g = position(["Moneylender", "Silver"], kingdom=KINGDOM[:-1] + ["Moneylender"])
    assert run(g, "Moneylender") == []                # no Copper: no question


def test_mine_upgrades_a_treasure_into_hand():
    g = position(["Mine", "Silver", "Copper"], kingdom=KINGDOM[:-1] + ["Mine"])
    asked = run(g, "Mine", [C.SILVER, C.GOLD])
    assert [d.kind for d in asked] == [MINE_TRASH, MINE_GAIN] and asked[1].subject == C.SILVER
    assert asked[1].options == (C.COPPER, C.SILVER, C.GOLD) and not asked[1].may_pass
    assert names(g.players[0].hand) == ["Copper", "Gold"] and g.trash == [C.SILVER]
    g = position(["Mine", "Copper"], kingdom=KINGDOM[:-1] + ["Mine"])
    asked = run(g, "Mine", [C.COPPER, C.SILVER])
    assert C.GOLD not in asked[1].options             # Copper costs 0: up to $3
    g = position(["Mine", "Copper"], kingdom=KINGDOM[:-1] + ["Mine"])
    run(g, "Mine", [-1])
    assert g.trash == [] and g.players[0].hand == [C.COPPER]


# -- Throne Room on every action card ------------------------------------------------------

ACTION_CARDS = [n for n in C.KINGDOM_CARDS if C.CARDS[ID[n]].is_action and n != "Throne Room"]


def _play_answering(g, card, first):
    """Play `card` from player 0's hand; the first decision answers `first`, every later one
    passes when it may, else takes the first option."""
    p = g.players[0]
    p.hand.remove(ID[card])
    p.play.append(ID[card])
    gen, asked = g._play(0, ID[card]), []
    try:
        d = next(gen)
        while True:
            asked.append(d)
            a = first if len(asked) == 1 else (-1 if d.may_pass else d.options[0])
            d = gen.send(a)
    except StopIteration:
        pass
    return asked


@pytest.mark.parametrize("name", ACTION_CARDS)
def test_throne_room_plays_every_action_twice(name):
    kingdom = [k for k in KINGDOM if k != name][:9] + [name]
    if "Throne Room" not in kingdom:
        kingdom[0] = "Throne Room"
    g = position(["Throne Room", name], deck=["Copper"] * 20, kingdom=kingdom)
    g.actions = 0
    asked = _play_answering(g, "Throne Room", ID[name])
    c = C.CARDS[ID[name]]
    assert asked[0].kind == THRONE and asked[0].options == (ID[name],)
    assert g.players[0].play.count(ID[name]) == 1 and C.THRONE_ROOM in g.players[0].play
    assert g.actions == 2 * c.actions and g.buys == 1 + 2 * c.buys
    if name not in ("Moneylender",):                  # passing Moneylender gives no $
        assert g.coins == 2 * c.coins, (g.coins, c.coins)
    if name not in ("Library", "Cellar", "Mine", "Remodel", "Artisan", "Moat"):
        assert len(g.players[0].hand) == 2 * c.cards
    if name == "Moat":
        assert len(g.players[0].hand) == 4


def test_throne_room_on_throne_room_plays_two_actions_twice_each():
    g = position(["Throne Room", "Throne Room", "Village", "Smithy"], deck=["Copper"] * 20)
    asked = run(g, "Throne Room", [C.THRONE_ROOM, C.VILLAGE, C.SMITHY])
    assert [d.kind for d in asked] == [THRONE] * 3
    assert g.actions == 4 and len(g.players[0].hand) == 2 + 6


def test_throne_room_with_no_action_offers_only_pass():
    g = position(["Throne Room", "Copper", "Estate"])
    asked = run(g, "Throne Room", [-1])
    assert asked[0].kind == THRONE and asked[0].options == () and asked[0].may_pass
