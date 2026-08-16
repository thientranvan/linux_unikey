#!/usr/bin/env python3
import ctypes
import threading
import time

import gi

gi.require_version("Gtk", "3.0")
from gi.repository import Gio, GLib, Gtk

sequence = list("tieengs") + ["BackSpace"] + list(" con meof maf treof caay cau") + ["BackSpace", "BackSpace"]
expected = "tiến con mèo mà trèo cây c"
result = []

settings = Gio.Settings.new("org.freedesktop.ibus.engine.unikey")
assert settings.get_boolean("direct-forward")

window = Gtk.Window(title="linux-unikey-lag-smoke")
entry = Gtk.Entry()
window.add(entry)
window.set_default_size(480, 60)
window.show_all()
window.present()
entry.grab_focus()


def send_keys():
    x11 = ctypes.CDLL("libX11.so.6")
    xtst = ctypes.CDLL("libXtst.so.6")
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XStringToKeysym.argtypes = [ctypes.c_char_p]
    x11.XStringToKeysym.restype = ctypes.c_ulong
    x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
    x11.XKeysymToKeycode.restype = ctypes.c_ubyte
    x11.XFlush.argtypes = [ctypes.c_void_p]
    xtst.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]

    display = x11.XOpenDisplay(None)
    assert display
    for key in sequence:
        name = b"space" if key == " " else key.encode()
        keycode = x11.XKeysymToKeycode(display, x11.XStringToKeysym(name))
        assert keycode
        xtst.XTestFakeKeyEvent(display, keycode, 1, 0)
        xtst.XTestFakeKeyEvent(display, keycode, 0, 0)
        x11.XFlush(display)
        time.sleep(0.001)


def finish():
    result.append(entry.get_text())
    window.destroy()
    Gtk.main_quit()
    return False


def lag_while_typing():
    threading.Thread(target=send_keys, daemon=True).start()
    time.sleep(0.5)
    GLib.timeout_add(800, finish)
    return False


GLib.timeout_add(700, lag_while_typing)
GLib.timeout_add_seconds(5, lambda: (Gtk.main_quit(), False)[1])
Gtk.main()

assert result == [expected], (result, expected)
print(result[0])
