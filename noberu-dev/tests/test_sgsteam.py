#!/usr/bin/env python3
"""End-to-end tests for Steam STEINS;GATE (profile sgsteam) on the native build.

  python3 noberu-dev/tests/test_sgsteam.py            # all but the long ones
  NOBERU_LONG=1 python3 noberu-dev/tests/test_sgsteam.py   # + long skip run
  python3 noberu-dev/tests/test_sgsteam.py SaveLoad    # one class

Needs the installed build (cmake --install impacto-build/Release) and the
game in gamedata/sgsteam. Every run is muted, gets its own empty saves
folder, and ignores the real mouse/keyboard (see harness.py). Screenshots of
failures land in noberu-dev/tests/out/.
"""
import os
import sys
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Game, image_stats, save_png  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")

# Known log errors that don't mean a broken game: the PS3 cursor/icon files
# the base profile names, Bink movies (not supported), and the save file not
# existing yet on a fresh saves folder.
KNOWN_ERRORS = [
    r"resources/sgps3/icondata",
    r"Failed to open movie for playback",
    r"\[IO\] Could not get metadata",
    r"SAVEDATA\.DAT",
]

PROLOGUE = [
    ("???", "“Hey, what are you mumbling about?”"),
    ("", "There's no sound from the phone against my right ear. Only silence."),
    ("", "I am baking in the summer sun."),
    ("", "Sweat slowly slides down my chin and drips onto the asphalt."),
]

# System menu rows, in script order (SW_SYSMENUCNO).
MENU_QUICKSAVE, MENU_QUICKLOAD, MENU_SAVE = 1, 2, 3
MENU_TITLE = 9

MENU_SHOWN, MENU_HIDDEN = 3, 0


def tap(g, key, frames=5):
    """A key press held for a few frames, like a person typing."""
    g.send("keydown " + key)
    g.frames(frames)
    g.send("keyup " + key)
    g.frames(2)


def to_title(g):
    return g.wait_for(lambda s: s["titlemenu"] == MENU_SHOWN, 90,
                      what="title menu")


def new_game(g):
    to_title(g)
    g.press_until("Return", lambda s: s["gamestate"] == 1, 30,
                  what="game start")
    return g.wait_for(lambda s: s["glyphs"] > 0 and s["opaque"], 60,
                      what="first line")


def steer(g, field, target, what):
    """Taps Up/Down until state[field] == target."""
    for _ in range(12):
        state = g.state()
        if state[field] == target:
            return state
        tap(g, "Down" if state[field] < target else "Up")
    raise AssertionError("%s stuck at %d" % (what, g.state()[field]))


TITLE_MAIN, TITLE_SUB = 1, 2
TITLE_ITEM_LOAD, TITLE_SUB_LOAD = 1, 1  # LOAD -> (Quick Load, Load)


def load_from_title(g, slot_keys=(), wait_resume=True):
    """Title -> LOAD -> Load -> slot 01 (after `slot_keys`) -> confirm."""
    to_title(g)
    g.press_until("Return", lambda s: s["titlestage"] == TITLE_MAIN, 20,
                  what="leave Press Enter")
    steer(g, "titlecur", TITLE_ITEM_LOAD, "title cursor")
    g.press_until("Return", lambda s: s["titlestage"] == TITLE_SUB, 10,
                  what="LOAD submenu")
    steer(g, "titlesub", TITLE_SUB_LOAD, "title sub cursor")
    g.press_until("Return", lambda s: s["savemenu"] == MENU_SHOWN, 15,
                  what="load menu")
    for key in slot_keys:
        tap(g, key)
    g.press_until("Return", lambda s: s["savemenu"] != MENU_SHOWN or
                  s["sysmesbox"], 10, what="pick slot")
    if g.state()["sysmesbox"]:
        tap(g, "Return")  # confirm load
    # The load fast-forwards from the scene's checkpoint to the saved line.
    if wait_resume:
        g.wait_for(lambda s: s["gamestate"] == 1 and not s["resuming"] and
                   s["savemenu"] == 0, 60, what="resume to the saved line")


def next_line(g):
    """Advances one line (the first press may only finish the typewriter)."""
    start = g.state()
    return g.press_until(
        "Return",
        lambda s: (s["line"], s["script"]) != (start["line"], start["script"])
        and s["opaque"], 20, what="next line")


def open_menu(g):
    return g.press_until("Escape", lambda s: s["sysmenu"] == MENU_SHOWN, 15,
                         what="system menu")


def menu_select(g, row):
    # The menu reopens on the last row used, so steer by its cursor.
    state = open_menu(g)
    for _ in range(12):
        if state["sysmenucur"] == row:
            break
        tap(g, "Down" if state["sysmenucur"] < row else "Up")
        state = g.state()
    else:
        raise AssertionError("menu cursor stuck at %d" % state["sysmenucur"])
    tap(g, "Return")


def skip_until(g, predicate, timeout=120, what="skip target"):
    g.send("keydown Left_Ctrl")
    try:
        return g.wait_for(predicate, timeout, interval=0.3, what=what)
    finally:
        g.send("keyup Left_Ctrl")
        g.frames(5)


class GameTest(unittest.TestCase):
    saves = None  # shared saves folder for classes that restart the game

    def start(self, **kw):
        g = Game(saves=self.saves, **kw).start()
        self.addCleanup(self.finish, g)
        self.game = g
        return g

    def finish(self, g):
        failed = any(e for e in self._outcome.result.errors +
                     self._outcome.result.failures if e[0] is self)
        if failed and g.alive():
            os.makedirs(OUT, exist_ok=True)
            try:
                save_png(g.shot(), os.path.join(OUT, self.id() + ".png"))
            except Exception:
                pass
        g.stop()

    def assertNoNewErrors(self, g):
        errors = g.log_errors(ignore=KNOWN_ERRORS)
        self.assertEqual(errors, [], "unexpected errors in impacto.log")

    def assertNotBlank(self, path):
        brightness, colours = image_stats(path)
        self.assertGreater(brightness, 10, "frame is black")
        self.assertGreater(colours, 30, "frame is a flat colour")


class Boot(GameTest):
    def test_boots_to_title_with_picture(self):
        g = self.start()
        state = to_title(g)
        self.assertEqual(state["mastervol"], 0)
        self.assertEqual(state["gamestate"], 0)
        self.assertNotBlank(g.shot())
        self.assertNoNewErrors(g)


class Dialogue(GameTest):
    def test_new_game_shows_prologue_lines_in_order(self):
        g = self.start()
        state = new_game(g)
        self.assertEqual((state["script"], state["line"]), (12, 0))
        for i, (name, text) in enumerate(PROLOGUE):
            if i:
                state = next_line(g)
            self.assertEqual(state["line"], i)
            self.assertEqual(state["name"], name)
            self.assertEqual(state["text"], text)
        self.assertNotBlank(g.shot())
        self.assertNoNewErrors(g)

    def test_colour_tag_keeps_line_whole(self):
        # Steam strings write colours as expressions (04 82 00 00); reading
        # them as one byte broke the line in three and logged "unrecognized
        # token 0x14" on variable colours.
        g = self.start()
        new_game(g)
        state = g.state()
        while state["line"] < 18:
            state = next_line(g)
        self.assertEqual(state["name"], "Rintaro")
        self.assertEqual(state["text"], "\u201cWhat!? The Organization is "
                         "already on the move!?\u201d")
        self.assertEqual(state["rows"], 1)
        skip_until(g, lambda s: s["line"] >= 450, 120, "line 450")
        self.assertNoNewErrors(g)

    def test_left_click_advances(self):
        g = self.start()
        new_game(g)
        g.press_until("mclick 640 420", lambda s: s["line"] == 1, 15,
                      what="click to line 1")

    def test_ctrl_skips_and_stops_on_release(self):
        g = self.start()
        new_game(g)
        state = skip_until(g, lambda s: s["line"] >= 40, 60, "line 40")
        self.assertGreaterEqual(state["line"], 40)
        time.sleep(1.5)
        settled = g.state()
        time.sleep(1.5)
        self.assertEqual(g.state()["line"], settled["line"],
                         "kept skipping after Ctrl was released")

    def test_auto_mode_toggles_and_advances(self):
        g = self.start()
        new_game(g)
        for key in ("Delete", "F3"):  # the manual's key, and the old F3
            tap(g, key)
            self.assertEqual(g.state()["auto"], 1, key)
            start = g.state()["line"]
            g.wait_for(lambda s: s["line"] > start, 20,
                       what="auto advance (%s)" % key)
            tap(g, key)
            self.assertEqual(g.state()["auto"], 0, key)

    def test_shift_hides_text_window(self):
        g = self.start()
        new_game(g)
        tap(g, "Left_Shift")
        self.assertEqual(g.state()["uihidden"], 1)
        tap(g, "Left_Shift")
        self.assertEqual(g.state()["uihidden"], 0)


class Menus(GameTest):
    def test_escape_opens_and_closes_menu_every_time(self):
        # Regression: Escape also meant Back/Hide, hiding the text first and
        # making the script skip its menu check about half the time.
        g = self.start()
        new_game(g)
        for i in range(5):
            tap(g, "Escape")
            g.wait_for(lambda s: s["sysmenu"] == MENU_SHOWN, 5,
                       what="menu open #%d" % i)
            self.assertEqual(g.state()["uihidden"], 0)
            tap(g, "Escape")
            g.wait_for(lambda s: s["sysmenu"] == MENU_HIDDEN, 5,
                       what="menu closed #%d" % i)

    def test_right_click_opens_menu(self):
        g = self.start()
        new_game(g)
        g.send("mrclick 640 300")
        g.wait_for(lambda s: s["sysmenu"] == MENU_SHOWN, 5, what="menu")
        g.send("mrclick 640 300")
        g.wait_for(lambda s: s["sysmenu"] == MENU_HIDDEN, 5, what="closed")

    def test_backlog_lists_earlier_lines(self):
        g = self.start()
        new_game(g)
        for _ in range(4):
            next_line(g)
        self.assertEqual(g.state()["nolog"], 0)
        # F2 and the wheel come from the game's HELP page; Insert is the
        # PS3-era key.
        for how in ("F2", "Insert", "wheel 1"):
            g.press_until(how, lambda s: s["backlog"] == MENU_SHOWN, 10,
                          what="backlog via " + how)
            self.assertNotBlank(g.shot())
            tap(g, "Backspace")
            g.wait_for(lambda s: s["backlog"] == MENU_HIDDEN and
                       s["sysmenu"] == MENU_HIDDEN, 10, what="backlog closed")
        self.assertNoNewErrors(g)

    def test_return_to_title_asks_and_goes_back(self):
        g = self.start()
        new_game(g)
        menu_select(g, MENU_TITLE)
        g.wait_for(lambda s: s["sysmesbox"] == 1, 10, what="Yes/No box")
        tap(g, "Return")  # Yes is focused first
        to_title(g)


class Phone(GameTest):
    def test_phone_opens_and_closes_once_allowed(self):
        g = self.start()
        new_game(g)
        # The prologue blocks the phone (flag 2612) until Okabe's call ends.
        skip_until(g, lambda s: s["line"] >= 380, 90, "prologue line 380")
        g.wait_for(lambda s: s["phonect"] == 0, 10)
        g.press_until("Z", lambda s: s["phoneopen"] == 1, 10, what="phone")
        g.wait_for(lambda s: s["phonect"] == 20, 5, what="phone slid in")
        self.assertNotBlank(g.shot())
        # Back closes it from the home screen (Game.exe 0x454be0); Back is
        # also Hide, which must not fire as well.
        g.press_until("Backspace", lambda s: s["phoneopen"] == 0, 10,
                      what="phone closed")
        g.wait_for(lambda s: s["phonect"] == 0, 5)
        self.assertEqual(g.state()["uihidden"], 0)
        self.assertNoNewErrors(g)


class SaveLoad(GameTest):
    def setUp(self):
        import tempfile
        self.saves = tempfile.mkdtemp(prefix="impacto-saves-")
        self.addCleanup(__import__("shutil").rmtree, self.saves, True)

    def test_save_then_load_resumes_at_same_line(self):
        g = self.start(keep_saves=True)
        new_game(g)
        for _ in range(5):
            saved = next_line(g)
        menu_select(g, MENU_SAVE)
        g.wait_for(lambda s: s["savemenu"] == MENU_SHOWN, 10, what="save menu")
        tap(g, "Return")  # slot 01 (cursor starts there)
        save_file = os.path.join(self.saves, "sgsteam", "SAVEDATA.DAT")
        deadline = time.time() + 10
        while not os.path.exists(save_file) and time.time() < deadline:
            time.sleep(0.2)
        self.assertTrue(os.path.exists(save_file), "no SAVEDATA.DAT written")
        g.stop()

        g = self.start(keep_saves=True)
        to_title(g)
        load_from_title(g)
        state = g.wait_for(
            lambda s: s["gamestate"] == 1 and s["line"] == saved["line"] and
            s["opaque"], 60, what="resume at line %d" % saved["line"])
        self.assertEqual(state["script"], saved["script"])
        self.assertEqual(state["text"], saved["text"])
        self.assertNoNewErrors(g)

    def test_quick_save_and_quick_load(self):
        g = self.start(keep_saves=True)
        new_game(g)
        for _ in range(3):
            saved = next_line(g)
        menu_select(g, MENU_QUICKSAVE)
        time.sleep(1.5)
        tap(g, "Escape")
        g.wait_for(lambda s: s["sysmenu"] == MENU_HIDDEN, 10)
        for _ in range(3):
            next_line(g)
        self.assertNotEqual(g.state()["line"], saved["line"])
        menu_select(g, MENU_QUICKLOAD)  # a list of quick saves
        g.wait_for(lambda s: s["savemenu"] == MENU_SHOWN, 10,
                   what="quick load list")
        g.press_until("Return", lambda s: s["savemenu"] != MENU_SHOWN or
                      s["sysmesbox"], 10, what="pick the quick save")
        if g.state()["sysmesbox"]:
            tap(g, "Return")  # confirm load
        state = g.wait_for(
            lambda s: s["line"] == saved["line"] and s["sysmenu"] ==
            MENU_HIDDEN and s["opaque"], 60, what="quick load")
        self.assertEqual(state["text"], saved["text"])


@unittest.skipUnless(os.environ.get("NOBERU_LONG"), "set NOBERU_LONG=1")
class LongSkip(GameTest):
    def test_skip_far_without_script_errors(self):
        # Wrong SGHD operand lengths run data as code: watch for VM errors and
        # crashes while fast-forwarding well past the prologue.
        g = self.start()
        new_game(g)
        g.send("keydown Left_Ctrl")
        start = time.time()
        scripts = set()
        while time.time() - start < 240:
            time.sleep(2)
            state = g.state()
            scripts.add(state["script"])
            # Answer anything that waits for a key (phone calls, Yes/No).
            g.send("pad A")
        g.send("keyup Left_Ctrl")
        self.assertTrue(g.alive())
        self.assertGreater(len(scripts), 1, "never left the first script")
        vm_errors = [l for l in g.log_lines()
                     if "unrecognized" in l.lower() or
                     ("ERROR" in l and "[VM]" in l)]
        self.assertEqual(vm_errors[:20], [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
