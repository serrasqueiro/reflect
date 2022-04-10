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
        self.state = "STT"
        self.hint = None
        self.clients = TabledClients()

    def register(self, conn, sels, data):
        self.state = "REG"
        return self._registry(conn, sels, data)

    def server_shutdown(self):
        assert self.state == "REG", f"Invalid server state: {self.state}"
        self.state = "STT"
        self.sel.close()

    def is_server_up(self) -> bool:
        return self.state == "REG"

    def _registry(self, conn, sels, data):
        self.sel.register(conn, sels, data)

    def register_client(self, nclient) -> list:
        new = self.clients.push(nclient)
        return new

    def closedown_last(self) -> bool:
        return self.clients.closedown(-1)

class TabledClients():
    """ List of tabled clients """
    def __init__(self, name=""):
        self.name = name
        self._idx = 0
        self._seq = []

    def push(self, nclient) -> list:
        self._idx += 1
        pair = [self._idx, nclient]
        self._seq.append(pair)
        return pair

    def closedown(self, idx) -> bool:
        if not self._seq:
            return False
        last = self._seq[-1]
        _, nclient = last
        nclient.closedown()
        del self._seq[-1]
        return True

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
