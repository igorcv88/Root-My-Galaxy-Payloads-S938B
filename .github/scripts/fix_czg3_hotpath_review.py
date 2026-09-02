#!/usr/bin/env python3
from pathlib import Path

header = Path('src/czg3_diag.h')
text = header.read_text()
needle = '''/*
 * Production CZG3 observation lives in the app's sibling observer process.
 * Compile diagnostic call sites out of allocator/race-sensitive code while
 * retaining the implementation for host tests and explicit diagnostic builds.
 */
'''
insert = '''/*
 * Production CZG3 observation lives in the app's sibling observer process.
 * Compile diagnostic call sites out of allocator/race-sensitive code while
 * retaining the implementation for host tests and explicit diagnostic builds.
 */
#if defined(APP_CZG3_DIAGNOSTICS) && APP_CZG3_DIAGNOSTICS && \\
    (!defined(CZG3_EXTERNAL_OBSERVER) || !CZG3_EXTERNAL_OBSERVER)
#define CZG3_INBAND_DIAGNOSTICS 1
#else
#define CZG3_INBAND_DIAGNOSTICS 0
#endif

'''
if text.count(needle) != 1:
    raise SystemExit('expected CZG3 production diagnostics comment exactly once')
header.write_text(text.replace(needle, insert, 1))

path = Path('src/slide_app.c')
text = path.read_text()
old = '''#if defined(APP_CZG3_DIAGNOSTICS) && APP_CZG3_DIAGNOSTICS
    char race_state[192];
'''
new = '''#if CZG3_INBAND_DIAGNOSTICS
    char race_state[192];
'''
if text.count(old) != 1:
    raise SystemExit('expected physical race state block exactly once')
text = text.replace(old, new, 1)

old = '''#if defined(APP_CZG3_DIAGNOSTICS) && APP_CZG3_DIAGNOSTICS
      czg3_diag_event("PHYSICAL_RACE_RESULT", attempt, CZG3_SUCCESS, 1,
                      race_state);
#endif
'''
new = '''#if CZG3_INBAND_DIAGNOSTICS
      czg3_diag_event("PHYSICAL_RACE_RESULT", attempt, CZG3_SUCCESS, 1,
                      race_state);
#endif
'''
if text.count(old) != 1:
    raise SystemExit('expected physical success diagnostic block exactly once')
text = text.replace(old, new, 1)

old = '''#if defined(APP_CZG3_DIAGNOSTICS) && APP_CZG3_DIAGNOSTICS
    /* The route cannot prove PI-tree restoration after a failed child. */
    czg3_diag_event("PHYSICAL_RACE_RESULT", attempt,
                    CZG3_RACE_STATE_UNCERTAIN, 0, race_state);
    break;
#endif
'''
new = '''#if CZG3_INBAND_DIAGNOSTICS
    czg3_diag_event("PHYSICAL_RACE_RESULT", attempt,
                    CZG3_RACE_STATE_UNCERTAIN, 0, race_state);
#endif
#if defined(APP_CZG3_DIAGNOSTICS) && APP_CZG3_DIAGNOSTICS
    /* The route cannot prove PI-tree restoration after a failed child. */
    break;
#endif
'''
if text.count(old) != 1:
    raise SystemExit('expected physical failure block exactly once')
text = text.replace(old, new, 1)

old = '''#if defined(APP_CZG3_DIAGNOSTICS) && APP_CZG3_DIAGNOSTICS
    char diag_state[192];
'''
new = '''#if CZG3_INBAND_DIAGNOSTICS
    char diag_state[192];
'''
if text.count(old) != 1:
    raise SystemExit('expected P0 diagnostic state block exactly once')
text = text.replace(old, new, 1)

old = '''#if defined(APP_CZG3_DIAGNOSTICS) && APP_CZG3_DIAGNOSTICS
      /* A child exit does not prove that a kernel PI waiter was removed. */
      czg3_diag_event("P0_RACE_RESULT", attempt,
                      CZG3_RACE_STATE_UNCERTAIN, 0,
                      "child_failed,kernel_cleanup_unproven");
      break;
#endif
'''
new = '''#if CZG3_INBAND_DIAGNOSTICS
      czg3_diag_event("P0_RACE_RESULT", attempt,
                      CZG3_RACE_STATE_UNCERTAIN, 0,
                      "child_failed,kernel_cleanup_unproven");
#endif
#if defined(APP_CZG3_DIAGNOSTICS) && APP_CZG3_DIAGNOSTICS
      /* A child exit does not prove that a kernel PI waiter was removed. */
      break;
#endif
'''
if text.count(old) != 1:
    raise SystemExit('expected P0 child failure block exactly once')
text = text.replace(old, new, 1)

path.write_text(text)

post = path.read_text()
if post.count('char race_state[192];') != 1 or post.count('char diag_state[192];') != 1:
    raise SystemExit('unexpected diagnostic state construction count')
if post.count('/* The route cannot prove PI-tree restoration after a failed child. */\n    break;') != 1:
    raise SystemExit('physical fail-closed break was not preserved')
if post.count('/* A child exit does not prove that a kernel PI waiter was removed. */\n      break;') != 1:
    raise SystemExit('P0 fail-closed break was not preserved')
