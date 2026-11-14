"""The C engine through ctypes: built-in bots (for parity tests), and helpers for trainers.

Trainers that want their own agent in C compile c/engine.c with it into their own library
(see README.md); `build()` is the generic way to do that.
"""
from __future__ import annotations

import ctypes
import os
import subprocess
from typing import Sequence

from .cards import ID, NUM_CARDS

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
C_DIR = os.path.join(ROOT, "c")
ENGINE_SOURCES = [os.path.join(C_DIR, "engine.c")]
CFLAGS = ["-O3", "-shared", "-fPIC", "-Wall", "-Wextra", "-Wno-unused-parameter"]

BIG_MONEY, RANDOM = 0, 1


def build(out: str, extra_sources: Sequence[str] = (), include: Sequence[str] = ()) -> str:
    """Compile engine.c + extra_sources into shared library `out` if missing or stale."""
    srcs = ENGINE_SOURCES + list(extra_sources)
    deps = srcs + [os.path.join(C_DIR, "engine.h")]
    if not os.path.exists(out) or max(os.path.getmtime(s) for s in deps) > os.path.getmtime(out):
        cmd = ["gcc", *CFLAGS, "-I", C_DIR, *(f"-I{d}" for d in include), "-o", out, *srcs]
        subprocess.check_call(cmd)
    return out


_lib = None


def lib() -> ctypes.CDLL:
    global _lib
    if _lib is None:
        os.makedirs(os.path.join(ROOT, "build"), exist_ok=True)
        _lib = ctypes.CDLL(build(os.path.join(ROOT, "build", "libdominion_engine.so"),
                                 [os.path.join(C_DIR, "bots.c")]))
        _lib.de_play_bots.argtypes = [ctypes.POINTER(ctypes.c_int), ctypes.c_int, ctypes.c_uint64,
                                      ctypes.c_int, ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_uint64)]
        _lib.de_card_table.argtypes = [ctypes.POINTER(ctypes.c_int)]
    return _lib


def kingdom_ids(kingdom: Sequence[str | int]) -> list[int]:
    return [k if isinstance(k, int) else ID[k] for k in kingdom]


def play_bots(kingdom: Sequence[str | int], seed: int, bots: Sequence[tuple[int, int]]) -> dict:
    """One C game between built-in bots: bots = [(BIG_MONEY|RANDOM, bot_seed), ...]."""
    k = kingdom_ids(kingdom)
    n = len(bots)
    out = (ctypes.c_int * 11)()
    trace = ctypes.c_uint64()
    lib().de_play_bots((ctypes.c_int * len(k))(*k), len(k), seed & ((1 << 64) - 1), n,
                       (ctypes.c_int * n)(*[b[0] for b in bots]),
                       (ctypes.c_uint64 * n)(*[b[1] & ((1 << 64) - 1) for b in bots]),
                       out, ctypes.byref(trace))
    return {"scores": list(out[0:n]), "turns": list(out[4:4 + n]),
            "winners": [i for i in range(n) if out[8] >> i & 1],
            "capped": bool(out[9]), "error": bool(out[10]), "trace": trace.value}


def card_table() -> list[tuple[int, ...]]:
    buf = (ctypes.c_int * (7 * NUM_CARDS))()
    lib().de_card_table(buf)
    return [tuple(buf[7 * c:7 * c + 7]) for c in range(NUM_CARDS)]
