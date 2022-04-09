# libserver.py -- (c)2022 Henrique Moreira

""" Common server classes, and control class.
"""

# pylint: disable=missing-function-docstring

import selectors

EV_READ = selectors.EVENT_READ
EV_WRITE = selectors.EVENT_WRITE


class Control():
    """ Control selectors, global!
    """
    def __init__(self):
        sel = selectors.DefaultSelector()
        self.sel = sel
        self.hint = None

    def register(self, conn, sels, data):
        return self._registry(conn, sels, data)

    def _registry(self, conn, sels, data):
        self.sel.register(conn, sels, data)


class Message():
    """ basic Message class
    """
    def __init__(self, selector, sock, addr):
        self._sel, self._socket, self._address = selector, sock, addr
        self.outb = b''

    def process_events(self, mask):
        if mask & selectors.EVENT_READ:
            self.read()
        if mask & selectors.EVENT_WRITE:
            self.write()

if __name__ == "__main__":
    print("Please import me")
