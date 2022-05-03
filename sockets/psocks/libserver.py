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

    def closed_client(self, sock, nclient=None):
        """ Unregister socket within default selector,
        and cleans up client.
        """
        assert sock
        self.sel.unregister(sock)
        if nclient is None:
            return False
        self.clients.cleanup_client(nclient)
        return True

    def get_client(self, nclient:tuple):
        if not nclient:
            return None
        pair = self.clients.get_client_from_tup(nclient)
        return pair

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

class TabledClients():
    """ List of tabled clients """
    def __init__(self, name=""):
        self.name = name
        self._idx = 0
        self._seq = []
        self._hashed_client = {}

    @staticmethod
    def hash_ip_port(tup) -> str:
        """ tup is ("address", (int)port)) """
        assert isinstance(tup[1], int)
        ip_port = f"{tup[0]}+{tup[1]}"
        return ip_port

    def get_client_from_tup(self, nclient):
        idx = self._hashed_client.get(TabledClients.hash_ip_port(nclient))
        if idx is None:
            return (None, None)
        return self._seq[idx]

    def push(self, nclient) -> list:
        self._idx += 1
        pair = [self._idx, nclient]
        id_list = len(self._seq)
        self._seq.append(pair)
        self._hashed_client[TabledClients.hash_ip_port(nclient.address())] = id_list
        return pair

    def cleanup_client(self, nclient):
        ip_port = TabledClients.hash_ip_port(nclient.address())
        idx = self._hashed_client.get(ip_port)
        if idx is None:
            return False
        del self._seq[idx]
        del self._hashed_client[ip_port]
        return True

    def last_client(self):
        if not self._seq:
            return None
        return self._seq[-1][1]

    def closedown(self, idx) -> bool:
        if not self._seq:
            return False
        nclient = self._seq[idx]
        nclient.closedown()
        # Missing: delete _hashed_client
        del self._seq[idx]
        return True

    def force_closedown(self, idx=None) -> bool:
        """ Forces shutdown for one, or all. """
        if idx is None:
            for idx, nclient in self._seq:
                nclient.closedown(shut_up=True)
            self._seq, self._hashed_client = [], {}
            return True
        _, nclient = self._seq[idx]
        nclient.closedown(shut_all=True)
        del self._seq[idx]
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
