#!/usr/bin/env python3
"""Chapter 1 phone tests for Steam STEINS;GATE (profile sgsteam).

  python3 noberu-dev/tests/test_sgsteam_ch1.py

Setup plays a new game once (fast-forwarding where the story allows) and
saves three points that the tests load:
  slot 01  SG01_03 (script 15): the PhoneWave tutorial, waiting for a call
  slot 02  SG01_12 (script 24): waiting for the second PhoneWave call, with a
           new mail from Faris in the inbox
  slot 03  SG02_01 (script 26): Mayuri's mail arriving (repliable by link)
Setup takes about four minutes; each test then loads its own copy. Muted,
like everything in harness.py.
"""
import os
import shutil
import sys
import tempfile
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Game  # noqa: E402
from test_sgsteam import (GameTest, MENU_SAVE, MENU_SHOWN, load_from_title,  # noqa: E402
                          menu_select, new_game, next_line, skip_until, tap)

# Phone screens (SW 6802), from src/games/sghd/phone.cpp.
HOME, INBOX, VIEW, CONTACTS, CALL_POPUP = 0, 3, 5, 6, 7
DIALING, IN_CALL, MENU, REPLY_PREVIEW, MAIL_SENT = 10, 11, 20, 22, 24
KEYPAD, LINK_MAIL, LINK_OPTIONS, SEND_CONFIRM = 23, 27, 28, 40
# ScrWork / flags the tests read.
SW_MENU_CURSOR, SW_CONTACT_COUNT = 6812, 6820
SW_INBOX_COUNT, SW_SENT_COUNT, SW_RECV_MAIL = 6809, 6808, 6824
SW_KEYPAD_COUNT, SW_KEYPAD_RESULT = 6832, 6833
SW_REPLY_MAIL, SW_REPLY = 6860, 6861
SF_NEW_MAIL, SF_RECEIVING = 2604, 2613
KEYPAD_120_HASH = 1

SLOT_TUTORIAL, SLOT_SECOND_CALL, SLOT_MAIL = (), ("Right",), ("Down",)


# --- phone helpers -----------------------------------------------------------
def open_phone(g):
    g.press_until("Z", lambda s: s["phoneopen"] == 1 and s["phonect"] == 20,
                  15, what="phone open")


def phone_menu(g):
    open_phone(g)
    g.press_until("Return", lambda s: s["phonemode"] in (INBOX, MENU), 10,
                  what="leave home")
    if g.state()["phonemode"] == INBOX:  # a new-mail panel opens the inbox
        g.press_until("Backspace", lambda s: s["phonemode"] == MENU, 10)


def call_first_callable(g):
    """Address book -> the first contact that offers Call -> Call."""
    phone_menu(g)
    for _ in range(4):
        if g.sw(SW_MENU_CURSOR)[SW_MENU_CURSOR] == 1:  # address book icon
            break
        tap(g, "Right")
    g.press_until("Return", lambda s: s["phonemode"] == CONTACTS, 10,
                  what="address book")
    for _ in range(g.sw(SW_CONTACT_COUNT)[SW_CONTACT_COUNT]):
        tap(g, "Return")
        time.sleep(0.3)
        if g.state()["phonemode"] == CALL_POPUP:
            g.press_until("Return", lambda s: s["phonemode"] != CALL_POPUP, 10)
            return
        tap(g, "Down")
    raise AssertionError("no contact offered a call")


def type_on_keypad(g, keys):
    """Keys as typed: digits, "#" (Shift+3) and "Backspace"."""
    counts = []
    for key in keys:
        if key == "#":
            g.send("keydown Left_Shift")
            g.frames(3)
            tap(g, "3")
            g.send("keyup Left_Shift")
            g.frames(3)
        else:
            tap(g, key)
        time.sleep(0.3)
        counts.append(g.sw(SW_KEYPAD_COUNT)[SW_KEYPAD_COUNT])
    return counts


def skip_to_wait(g, limit=240):
    """Holds Ctrl until the story stops moving (it waits on the player)."""
    g.send("keydown Left_Ctrl")
    last, still = None, 0
    try:
        end = time.time() + limit
        while time.time() < end:
            time.sleep(1)
            s = g.state()
            key = (s["script"], s["line"], s["phonemode"], s["phoneopen"],
                   s["sysmesbox"])
            still = still + 1 if key == last else 0
            last = key
            if still >= 3:
                return s
    finally:
        g.send("keyup Left_Ctrl")
        g.frames(5)
    raise AssertionError("story never waited; last state %s" % (last,))


def save_to_slot(g, slot_keys):
    menu_select(g, MENU_SAVE)
    g.wait_for(lambda s: s["savemenu"] == MENU_SHOWN, 10, what="save menu")
    for key in slot_keys:
        tap(g, key)
    tap(g, "Return")
    time.sleep(1.5)
    if g.state()["sysmesbox"]:
        tap(g, "Return")  # overwrite
        time.sleep(1.5)
    g.press_until("Escape", lambda s: s["savemenu"] == 0, 10)
    g.press_until("Escape", lambda s: s["sysmenu"] == 0, 10)


def wait_for_phonewave_loop(g):
    """SG01_03 repeats lines 80-81 until the player calls."""
    return g.wait_for(lambda s: s["script"] == 15 and s["line"] in (80, 81)
                      and s["opaque"], 60, what="phone hunt (80/81)")


def call_and_dial(g):
    call_first_callable(g)
    g.wait_for(lambda s: s["phonemode"] in (DIALING, IN_CALL), 10)
    state = skip_to_wait(g)
    assert state["phonemode"] == KEYPAD, state
    type_on_keypad(g, "120#")


class Chapter1(GameTest):
    @classmethod
    def setUpClass(cls):
        cls.built = tempfile.mkdtemp(prefix="impacto-ch1-")
        g = Game(saves=cls.built, keep_saves=True).start()
        try:
            new_game(g)
            skip_until(g, lambda s: s["script"] == 15 and s["sysmesbox"], 150,
                       "PhoneWave tutorial box")
            g.press_until("Return", lambda s: s["sysmesbox"] == 0, 10)
            wait_for_phonewave_loop(g)
            save_to_slot(g, SLOT_TUTORIAL)

            call_first_callable(g)
            skip_to_wait(g)  # the keypad
            type_on_keypad(g, "120#")
            state = skip_to_wait(g)
            assert state["script"] == 24, state
            save_to_slot(g, SLOT_SECOND_CALL)

            call_and_dial(g)
            skip_until(g, lambda s: s["script"] == 26 and s["line"] >= 50, 180,
                       "SG02_01 line 50")
            save_to_slot(g, SLOT_MAIL)
        finally:
            g.stop()

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.built, ignore_errors=True)

    def load(self, slot_keys, script):
        self.saves = tempfile.mkdtemp(prefix="impacto-saves-")
        self.addCleanup(shutil.rmtree, self.saves, True)
        shutil.copytree(self.built, self.saves, dirs_exist_ok=True)
        g = self.start()
        load_from_title(g, slot_keys)
        g.wait_for(lambda s: s["gamestate"] == 1 and s["script"] == script and
                   s["savemenu"] == 0 and s["opaque"], 60,
                   what="loaded into script %d" % script)
        return g

    # -- SG01_03: the PhoneWave tutorial ---------------------------------
    def test_phonewave_tutorial_call(self):
        g = self.load(SLOT_TUTORIAL, 15)
        wait_for_phonewave_loop(g)
        open_phone(g)
        self.assertEqual(g.state()["phonemode"], HOME)
        call_first_callable(g)
        g.wait_for(lambda s: s["phonemode"] == IN_CALL and s["line"] == 82, 15,
                   what="connected")
        self.assertEqual(g.state()["text"], "I'm connected.")
        self.assertNotBlank(g.shot())
        self.assertNoNewErrors(g)

    def test_keypad_types_120_hash(self):
        # Regression: "1" is also the system menu key and Shift is Back, so
        # typed digits stalled the keypad and "#" deleted a digit.
        g = self.load(SLOT_TUTORIAL, 15)
        wait_for_phonewave_loop(g)
        call_first_callable(g)
        state = skip_to_wait(g)
        self.assertEqual(state["phonemode"], KEYPAD)
        counts = type_on_keypad(g, ["1", "2", "Backspace", "2", "0", "#"])
        self.assertEqual(counts, [1, 2, 1, 2, 3, 4])
        self.assertEqual(g.sw(SW_KEYPAD_RESULT)[SW_KEYPAD_RESULT],
                         KEYPAD_120_HASH)
        self.assertEqual(g.state()["sysmenu"], 0)
        g.wait_for(lambda s: s["text"] == "That should do it.", 20,
                   what="story after dialing")
        self.assertNoNewErrors(g)

    def test_load_after_dial_does_not_hang(self):
        # Saves restart at the scene's checkpoint and fast-forward to the
        # saved line. A save after the dial whose checkpoint is before the
        # phone hunt must stop at the hunt's loop, not spin there (input is
        # blocked while fast-forwarding).
        g = self.load(SLOT_TUTORIAL, 15)
        wait_for_phonewave_loop(g)
        call_first_callable(g)
        skip_to_wait(g)
        type_on_keypad(g, "120#")
        saved = g.wait_for(lambda s: s["text"] == "That should do it." and
                           s["opaque"], 20, what="after dialing")
        save_to_slot(g, ("Down", "Right"))  # slot 04
        g.stop()

        g = self.start()
        load_from_title(g, ("Down", "Right"), wait_resume=False)
        g.wait_for(lambda s: s["resuming"] or s["script"] == 15, 30,
                   what="load started")
        state = g.wait_for(lambda s: s["script"] == 15 and s["glyphs"] and
                           not s["resuming"] and s["opaque"], 45,
                           what="fast-forward to end")
        self.assertEqual(state["script"], 15)
        self.assertIn(state["line"], (80, 81, saved["line"]))
        self.assertNoNewErrors(g)

    def test_120_hash_stays_on_one_line(self):
        # "#" (glyph 416) was not a word character, so "120#" wrapped.
        g = self.load(SLOT_TUTORIAL, 15)
        wait_for_phonewave_loop(g)
        call_first_callable(g)
        state = skip_to_wait(g)
        self.assertIn("“120#”", state["text"])
        rows = state["wrapped"].split("/")
        self.assertTrue(any("120#" in row for row in rows), rows)

    # -- SG01_12: Faris's mail and the second call -------------------------
    def test_new_mail_opens_inbox(self):
        g = self.load(SLOT_SECOND_CALL, 24)
        self.assertEqual(g.flag(SF_NEW_MAIL)[SF_NEW_MAIL], 1)
        open_phone(g)
        g.press_until("Return", lambda s: s["phonemode"] == INBOX, 10,
                      what="new-mail panel to inbox")
        self.assertEqual(g.flag(SF_NEW_MAIL)[SF_NEW_MAIL], 0)
        self.assertEqual(g.sw(SW_INBOX_COUNT)[SW_INBOX_COUNT], 6)
        g.press_until("Return", lambda s: s["phonemode"] == VIEW, 10,
                      what="open Faris's mail")
        for _ in range(10):  # scroll to the end
            tap(g, "Down")
        self.assertNotBlank(g.shot())
        g.press_until("Backspace", lambda s: s["phonemode"] == INBOX, 10)
        g.press_until("Backspace", lambda s: s["phonemode"] == MENU, 10)
        g.press_until("Backspace", lambda s: s["phonemode"] == HOME, 10)
        # One Back closes it from home; that press must not hide the text.
        tap(g, "Backspace")
        g.wait_for(lambda s: s["phoneopen"] == 0 and s["phonect"] == 0, 10,
                   what="phone closed")
        self.assertEqual(g.state()["uihidden"], 0)
        self.assertNoNewErrors(g)

    def test_input_ignored_while_load_fast_forwards(self):
        # Regression: the phone opened during the post-load fast-forward was
        # closed by the script underneath, and later Back presses hid text.
        self.saves = tempfile.mkdtemp(prefix="impacto-saves-")
        self.addCleanup(shutil.rmtree, self.saves, True)
        shutil.copytree(self.built, self.saves, dirs_exist_ok=True)
        g = self.start()
        load_from_title(g, SLOT_SECOND_CALL, wait_resume=False)
        g.wait_for(lambda s: s["resuming"], 30, what="fast-forward")
        # The story itself opens the phone at line 38 (Faris's mail) during
        # the fast-forward, so only look before that.
        pressed = 0
        while pressed < 20:
            state = g.state()
            if not state["resuming"] or state["line"] >= 36:
                break
            self.assertEqual(state["phoneopen"], 0, "opened at line %d" %
                             state["line"])
            tap(g, "Z")
            pressed += 1
        self.assertGreater(pressed, 0)
        g.wait_for(lambda s: not s["resuming"] and s["line"] in (61, 62), 30)
        if g.state()["phoneopen"]:
            g.press_until("Backspace", lambda s: s["phoneopen"] == 0, 10)
        open_phone(g)
        self.assertEqual(g.state()["uihidden"], 0)
        self.assertNoNewErrors(g)

    def test_second_phonewave_call_continues_story(self):
        g = self.load(SLOT_SECOND_CALL, 24)
        g.wait_for(lambda s: s["line"] in (61, 62), 30, what="phone hunt")
        call_and_dial(g)
        self.assertEqual(g.sw(SW_KEYPAD_RESULT)[SW_KEYPAD_RESULT],
                         KEYPAD_120_HASH)
        skip_until(g, lambda s: s["script"] >= 26, 120, "chapter 2")
        self.assertNoNewErrors(g)

    # -- SG02_01: replying to Mayuri ----------------------------------------
    def test_reply_to_arriving_mail_by_link(self):
        g = self.load(SLOT_MAIL, 26)
        for _ in range(20):  # read on until Mayuri's mail arrives (~line 59)
            if g.flag(SF_RECEIVING)[SF_RECEIVING]:
                break
            next_line(g)
        self.assertEqual(g.flag(SF_RECEIVING)[SF_RECEIVING], 1)
        mail = g.sw(SW_RECV_MAIL)[SW_RECV_MAIL]
        open_phone(g)
        g.press_until("Return", lambda s: s["phonemode"] == INBOX, 10)
        sent = g.sw(SW_SENT_COUNT)[SW_SENT_COUNT]  # lists build on open
        g.press_until("Return", lambda s: s["phonemode"] == LINK_MAIL, 10,
                      what="arriving mail with links")
        time.sleep(1)  # the screen ignores input while it opens
        for _ in range(3):  # "spending money", then "food"
            tap(g, "Down")
            time.sleep(0.3)
        for screen in (LINK_OPTIONS, REPLY_PREVIEW, SEND_CONFIRM):
            g.press_until("Return", lambda s, m=screen: s["phonemode"] == m,
                          10, what="screen %d" % screen)
        g.press_until("Return", lambda s: s["phonemode"] == MAIL_SENT, 10,
                      what="Mail Sent")
        self.assertEqual(g.sw(SW_REPLY_MAIL)[SW_REPLY_MAIL], mail)
        self.assertEqual(g.sw(SW_REPLY)[SW_REPLY], 403)  # "Go on a diet"
        self.assertEqual(g.sw(SW_SENT_COUNT)[SW_SENT_COUNT], sent + 1)
        self.assertNotBlank(g.shot())
        g.press_until("Return", lambda s: s["phonemode"] == LINK_MAIL, 10)
        # Back: mail -> inbox -> folders -> menu -> home -> closed.
        g.press_until("Backspace", lambda s: s["phoneopen"] == 0, 15,
                      what="phone closed")
        self.assertEqual(g.state()["uihidden"], 0)
        before = g.state()["line"]
        state = next_line(g)
        self.assertNotEqual(state["line"], before)
        self.assertNoNewErrors(g)


if __name__ == "__main__":
    unittest.main(verbosity=2)
