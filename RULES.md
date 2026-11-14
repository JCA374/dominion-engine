# Rules: what the engine plays

Dominion base set, 2nd edition, as **Steam Dominion** (Temple Gates) plays it. The first engine
(in `dominion-v1`, until 2026-11) differed from Steam in the rows marked *was*; they are what
models trained before this engine learned.

| rule | engine | was |
|---|---|---|
| Provinces, Duchies, Estates in the Supply (2 players) | 8 each (12 with 3-4 players) | 12 / 12 / 6 |
| Victory kingdom card (Gardens) | 8 (12 with 3-4 players) | 12 |
| Copper / Silver / Gold | 60 − 7 per player / 40 / 30 | same |
| Curse pile | always in the Supply, 10 per opponent | only with Witch or Militia |
| End of game | after a turn: no Provinces, or any 3 Supply piles empty (Curse counts) | same, but no Curse pile |
| Turn limit | none (a safety stop at 100 turns each for games nobody can end) | game over at turn 40 |
| Winner | most VP; tied → fewer turns wins; still tied → shared win | VP only, tie = half |
| Kingdom cards | all 26 of the base set | 15 |
| Council Room | each other player draws a card | they drew nothing |
| Merchant | +$1 per Merchant *played* on the first Silver (Throne Room → +$2) | per Merchant in play |
| Throne Room | "you may": declining is a choice | always doubled something if it could |
| Moneylender, Mine | "you may": declining is a choice | always if possible |
| Treasures | all played automatically (never worse in this set) | same |
| Moat | revealed automatically against every attack | same |

The 11 cards added: Artisan, Bandit, Bureaucrat, Cellar, Harbinger, Library, Poacher,
Remodel, Sentry, Vassal, Workshop. Card texts are the 2nd-edition ones (Steam's).

## Every choice is a decision

The engine asks each choice as a `Decision`: pick one of `options` (sorted card ids, or the
three `SENTRY_*` codes) or pass (`-1`) when `may_pass`. Multi-card choices (Chapel, Cellar,
Militia, Poacher) are asked one card at a time; `subject` then says how many are done or left.

| kind | who | options | pass? | subject |
|---|---|---|---|---|
| `PLAY_ACTION` | mover | action cards in hand | yes = end action phase | |
| `BUY` | mover | affordable cards with a non-empty pile | yes = end buying | |
| `THRONE` | mover | action cards in hand | yes | |
| `CHAPEL_TRASH` | mover | cards in hand | yes = stop | trashed so far (max 4) |
| `MONEYLENDER_TRASH` | mover | Copper | yes | |
| `MINE_TRASH` / `MINE_GAIN` | mover | treasures in hand / treasures costing ≤ +3 | yes / no | — / trashed card |
| `REMODEL_TRASH` / `REMODEL_GAIN` | mover | cards in hand / cards costing ≤ +2 | no | — / trashed card |
| `WORKSHOP_GAIN` | mover | cards costing ≤ 4 | no | |
| `ARTISAN_GAIN` / `ARTISAN_TOPDECK` | mover | cards costing ≤ 5 / cards in hand | no | |
| `CELLAR_DISCARD` | mover | cards in hand | yes = stop and draw | discarded so far |
| `HARBINGER_TOPDECK` | mover | cards in discard pile | yes | |
| `VASSAL_PLAY` | mover | the discarded action | yes = leave it | that card |
| `LIBRARY_SKIP` | mover | the drawn action | yes = keep it | that card |
| `SENTRY_CARD` / `SENTRY_TOP` | mover | trash / discard / keep, then which kept card on top | no | the card / — |
| `POACHER_DISCARD` | mover | cards in hand | no | still to discard |
| `MILITIA_DISCARD` | victim | cards in hand | no | still to discard |
| `BUREAUCRAT_TOPDECK` | victim | Victory cards in hand | no | |
| `BANDIT_TRASH` | victim | the revealed Silver/Gold (only when both differ) | no | |

## Sources

Steam: the 2-player supply and the absence of a turn limit were seen in the Steam campaign
(`dominion-steam-autopilot`, 8 Provinces, games of 17-65 turns); the rest is the published
2nd-edition rulebook, which the Steam game implements.
