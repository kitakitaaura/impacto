"""Drive a native impacto build through the DevInput file (src/devinput.h).

A Game starts Impacto with its own saves folder and command file, sends
commands, and reads replies ("DevState ...", "DevShot ...") back from
<command file>.out. Only the standard library is needed; screenshots are read
as PPM (Pillow is used for PNG output when it is installed).
"""
import os
import re
import shutil
import signal
import subprocess
import tempfile
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEV = os.path.join(ROOT, "noberu-dev")
BINARY = os.path.join(ROOT, "install", "Release", "Impacto.app", "Contents",
                      "MacOS", "Impacto")
USERCONFIG = os.path.expanduser(
    "~/Library/Application Support/Committee of Zero/Impacto/userconfig.toml")
# Log level for test runs; IMPACTO_TEST_LOG=Trace for VM traces (check.py),
# which grow by hundreds of MB a minute.
LOG_LEVEL = os.environ.get("IMPACTO_TEST_LOG", "Info")

STATE_RE = re.compile(r'(\w+)=("(?:[^"]*)"|\S+)')


class GameExited(Exception):
    pass


def parse_state(line):
    state = {}
    for key, value in STATE_RE.findall(line):
        if value.startswith('"'):
            state[key] = value[1:-1]
        else:
            state[key] = int(value)
    return state


def read_ppm(path):
    """Returns (width, height, bytes) of a binary P6 file."""
    with open(path, "rb") as f:
        data = f.read()
    parts = data.split(maxsplit=4)
    assert parts[0] == b"P6", path
    width, height = int(parts[1]), int(parts[2])
    return width, height, parts[4][: width * height * 3]


class Game:
    def __init__(self, profile="sgsteam", saves=None, keep_saves=False):
        self.profile = profile
        self.tmp = tempfile.mkdtemp(prefix="impacto-test-")
        self.saves = saves or os.path.join(self.tmp, "saves")
        os.makedirs(self.saves, exist_ok=True)
        self.keep_saves = keep_saves
        self.cmd = os.path.join(self.tmp, "devinput.txt")
        self.out = self.cmd + ".out"
        self.read_pos = 0
        self.proc = None
        self.log = None

    # -- process ---------------------------------------------------------
    def start(self, extra_args=()):
        basepaths = os.path.join(self.tmp, "basepaths.lua")
        with open(os.path.join(DEV, "basepaths.lua")) as src:
            text = src.read().replace("@ROOT@", ROOT)
        text = re.sub(r'RootSavesDir = "[^"]*"',
                      'RootSavesDir = "%s"' % self.saves, text)
        with open(basepaths, "w") as f:
            f.write(text)
        # Muted, only our injected input counts, and the window doesn't take
        # focus from whatever the user is doing.
        env = dict(os.environ, IMPACTO_MUTE="1", IMPACTO_DEV_INPUT=self.cmd,
                   IMPACTO_DEV_INPUT_ONLY="1",
                   SDL_WINDOW_ACTIVATE_WHEN_SHOWN="0")
        # A private userconfig: its own log (the config's LogFile wins over
        # -lf), our log level, and the user's own config is never rewritten.
        self.log = os.path.join(self.tmp, "impacto.log")
        userconfig = os.path.join(self.tmp, "userconfig.toml")
        text = ""
        if os.path.exists(USERCONFIG):
            with open(USERCONFIG) as f:
                text = f.read()
        text = re.sub(r'(?m)^(LogFile|LogLvl|LoggingToConsole) = .*\n', "", text)
        settings = ('LogFile = "%s"\nLogLvl = "%s"\nLoggingToConsole = false\n'
                    % (self.log, LOG_LEVEL))
        if "[CommonSettings]" in text:
            text = text.replace("[CommonSettings]\n",
                                "[CommonSettings]\n" + settings, 1)
        else:
            text += "\n[CommonSettings]\n" + settings
        with open(userconfig, "w") as f:
            f.write(text)
        extra_args = ("-uc", userconfig, *extra_args)
        self.proc = subprocess.Popen(
            [BINARY, "-bp", basepaths, "-gc",
             os.path.join(DEV, "gamedefinitions.lua"), "-g", self.profile,
             *extra_args],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
        return self

    def alive(self):
        return self.proc is not None and self.proc.poll() is None

    def stop(self):
        if self.alive():
            self.send("quit")
            try:
                self.proc.wait(5)
            except subprocess.TimeoutExpired:
                self.proc.send_signal(signal.SIGKILL)
                self.proc.wait()
        if not self.keep_saves:
            shutil.rmtree(self.tmp, ignore_errors=True)

    def __enter__(self):
        return self.start()

    def __exit__(self, *exc):
        self.stop()

    # -- commands --------------------------------------------------------
    def send(self, *lines):
        # The game deletes the file after reading it; append so nothing sent
        # in the same frame is lost.
        with open(self.cmd, "a") as f:
            for line in lines:
                f.write(line + "\n")

    def _replies(self):
        if not os.path.exists(self.out):
            return []
        with open(self.out, "rb") as f:
            f.seek(self.read_pos)
            data = f.read()
        end = data.rfind(b"\n") + 1
        self.read_pos += end
        return data[:end].decode("utf-8", "replace").splitlines()

    def request(self, command, prefix, timeout=10):
        """Sends `command` and returns the first reply line with `prefix`."""
        self._replies()  # drop stale replies
        self.send(command)
        deadline = time.time() + timeout
        while time.time() < deadline:
            if not self.alive():
                raise GameExited("game exited (code %s)" % self.proc.returncode)
            for line in self._replies():
                if line.startswith(prefix):
                    return line
            time.sleep(0.05)
        raise TimeoutError("no %s reply to %r" % (prefix, command))

    def state(self):
        state = parse_state(self.request("state", "DevState"))
        # Tests must never make sound (IMPACTO_MUTE=1 zeroes the master
        # volume, which every channel and movie is scaled by).
        if state.get("mastervol", 0) != 0:
            self.stop()
            raise AssertionError("game is not muted (mastervol=%s)" %
                                 state["mastervol"])
        return state

    def sw(self, first, last=None):
        """ScrWork[first..last] as {index: value}."""
        return self._dump("sw", first, last)

    def flag(self, first, last=None):
        return self._dump("flag", first, last)

    def _dump(self, kind, first, last):
        line = self.request("%s %d %d" % (kind, first, last or first),
                            "Dev%s:" % kind)
        return {int(k): int(v) for k, v in
                (pair.split("=") for pair in line.split(":", 1)[1].split())}

    def wait_for(self, predicate, timeout=30, interval=0.2, what="condition"):
        """Polls state() until predicate(state) is true; returns that state."""
        deadline = time.time() + timeout
        last = None
        while time.time() < deadline:
            try:
                last = self.state()
            except TimeoutError:  # slow frame (boot, big loads)
                continue
            if predicate(last):
                return last
            time.sleep(interval)
        raise TimeoutError("timed out waiting for %s; last state: %s" %
                           (what, last))

    def press_until(self, key, predicate, timeout=30, every=0.7,
                    what="condition"):
        """Sends `key` until predicate(state): a key name, or a whole
        command such as "pad A", "wheel 1" or "mclick 640 400".

        Menus ignore input while fading, so a single press can be lost;
        retrying until the state changes keeps tests free of fixed sleeps.
        """
        deadline = time.time() + timeout
        last = None
        while time.time() < deadline:
            try:
                last = self.state()
            except TimeoutError:
                continue
            if predicate(last):
                return last
            # "pad X", "wheel N", "mclick X Y"... pass through; a bare name
            # is a key tap.
            self.send(key if " " in key else "key " + key)
            step = time.time() + every
            while time.time() < step:
                time.sleep(0.1)
                try:
                    last = self.state()
                except TimeoutError:
                    continue
                if predicate(last):
                    return last
        raise TimeoutError("timed out pressing %s for %s; last state: %s" %
                           (key, what, last))

    def frames(self, n=2):
        """Waits until at least n more frames have run."""
        start = self.state()["frame"]
        self.wait_for(lambda s: s["frame"] >= start + n, timeout=10,
                      interval=0.02, what="%d frames" % n)

    def pad(self, name):
        self.send("pad " + name)
        self.frames()

    def key(self, name):
        self.send("key " + name)
        self.frames()

    def click(self, x, y):
        self.send("click %d %d" % (x, y))
        self.frames()

    def shot(self, path=None):
        path = path or os.path.join(self.tmp, "shot%d.ppm" % time.time_ns())
        self.request("shot " + path, "DevShot")
        return path

    # -- log -------------------------------------------------------------
    def log_lines(self):
        if not os.path.exists(self.log):
            return []
        with open(self.log, "rb") as f:
            return f.read().decode("utf-8", "replace").splitlines()

    def log_errors(self, ignore=()):
        errors = []
        for line in self.log_lines():
            if line.startswith(("ERROR", "FATAL")) and not any(
                    re.search(p, line) for p in ignore):
                errors.append(line)
        return errors


def image_stats(path):
    """Mean brightness (0-255) and fraction of distinct colours of a PPM."""
    width, height, data = read_ppm(path)
    total = sum(data[::97])  # sampled; plenty for "is it black"
    samples = len(data[::97])
    colours = len({data[i:i + 3] for i in range(0, len(data) - 2, 3 * 101)})
    return total / max(samples, 1), colours


def save_png(ppm, png):
    try:
        from PIL import Image
    except ImportError:
        shutil.copy(ppm, png[:-4] + ".ppm")
        return
    Image.open(ppm).save(png)
