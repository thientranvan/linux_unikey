#!/usr/bin/env python3
import ctypes
import os
import re
import subprocess
import time


x11 = ctypes.CDLL("libX11.so.6")
xtst = ctypes.CDLL("libXtst.so.6")
x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
x11.XOpenDisplay.restype = ctypes.c_void_p
x11.XSetInputFocus.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
x11.XRaiseWindow.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
x11.XStringToKeysym.argtypes = [ctypes.c_char_p]
x11.XStringToKeysym.restype = ctypes.c_ulong
x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
x11.XKeysymToKeycode.restype = ctypes.c_ubyte
x11.XFlush.argtypes = [ctypes.c_void_p]
xtst.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]

display = x11.XOpenDisplay(None)
assert display


def chrome_windows():
    tree = subprocess.check_output(["xwininfo", "-root", "-tree"], text=True)
    return {
        int(window, 16)
        for window in re.findall(r'^\s+(0x[0-9a-f]+) ".* - Google Chrome"', tree, re.MULTILINE)
    }


def focus(window):
    x11.XRaiseWindow(display, window)
    x11.XSetInputFocus(display, window, 1, 0)
    x11.XFlush(display)
    time.sleep(0.3)


def keycode(name):
    result = x11.XKeysymToKeycode(display, x11.XStringToKeysym(name.encode()))
    assert result, name
    return result


def key(name, control=False, shift=False):
    if control:
        xtst.XTestFakeKeyEvent(display, keycode("Control_L"), 1, 0)
    if shift:
        xtst.XTestFakeKeyEvent(display, keycode("Shift_L"), 1, 0)
    xtst.XTestFakeKeyEvent(display, keycode(name), 1, 0)
    xtst.XTestFakeKeyEvent(display, keycode(name), 0, 0)
    if shift:
        xtst.XTestFakeKeyEvent(display, keycode("Shift_L"), 0, 0)
    if control:
        xtst.XTestFakeKeyEvent(display, keycode("Control_L"), 0, 0)
    x11.XFlush(display)
    time.sleep(0.05)


sequence = os.getenv(
    "SMOKE_SEQUENCE",
    "con meof maf treof caay cau hoir thawm chus chuoojt ddi ddaau vawngs nhaf",
)
expected = os.getenv(
    "SMOKE_EXPECTED",
    "con mèo mà trèo cây cau hỏi thăm chú chuột đi đâu vắng nhà",
)
before = chrome_windows()
assert before, "Chrome must be running"
original = next(iter(before))
clipboard = subprocess.run(
    ["xclip", "-selection", "clipboard", "-o"],
    text=True,
    capture_output=True,
).stdout
settings_schema = "org.freedesktop.ibus.engine.unikey"
original_auto_capitalize = subprocess.check_output(
    ["gsettings", "get", settings_schema, "auto-capitalize"], text=True
).strip()
subprocess.check_call([
    "gsettings", "set", settings_schema, "auto-capitalize",
    os.getenv("SMOKE_AUTO_CAPITALIZE", "false"),
])

try:
    focus(original)
    key("n", control=True)
    time.sleep(1)
    created = chrome_windows() - before
    assert len(created) == 1, created
    window = created.pop()
    focus(window)
    key("l", control=True)
    for index, character in enumerate(sequence):
        names = {" ": "space", "\n": "Return", ".": "period", "!": "1", "?": "slash"}
        key(names.get(character, character.lower()), shift=character.isupper() or character in "!?")
        if index == 0:
            time.sleep(float(os.getenv("FIRST_KEY_DELAY", "0")))
        if index > 0 and sequence[index - 1:index + 1] == "  ":
            time.sleep(float(os.getenv("DOUBLE_SPACE_DELAY", "0")))
    time.sleep(0.5)
    key("a", control=True)
    key("c", control=True)
    result = subprocess.check_output(["xclip", "-selection", "clipboard", "-o"], text=True)
    assert result == expected, (result, expected)
    print(result)
finally:
    subprocess.run([
        "gsettings", "set", settings_schema, "auto-capitalize", original_auto_capitalize,
    ])
    if "window" in locals():
        focus(window)
        key("w", control=True)
    focus(original)
    subprocess.run(["xclip", "-selection", "clipboard"], input=clipboard, text=True)
