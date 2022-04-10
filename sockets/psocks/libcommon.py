# libcommon.py -- (c)2022 Henrique Moreira

""" Common wrappers and basic functions
"""

import socket

debug = 0


class OSErrorWrap(OSError):
    def __init__(self, errno=0, strerror=""):
        super().__init__(errno, strerror)


class NiceHost():
    """ Exposes host in a more elegant way
    """
    def __init__(self, addr=None, name=""):
        if addr is None:
            addr = ("?", 0)
        assert isinstance(addr, tuple)
        assert isinstance(name, str)
        self._addr = addr
        self.name = name

    def address(self):
        return self._addr

    def __str__(self):
        host, port = self._addr
        assert int(port) >= 0, f"Bogus port: {port}"
        return f"{host}:{port}"


class NiceClient(NiceHost):
    """ Client socket with host information
    """
    def __init__(self, addr=None, name="", rdns=False):
        super().__init__(addr, name)
        self.sock = None

    def closedown(self):
        """ Shutdown socket
        """
        if self.sock is None:
            return False
        #self.sock.shutdown(socket.SHUT_RDWR)
        self.sock.close()
        del self.sock
        self.sock = None
        return True


def dprint(*kwargs, **args):
    if debug <= 0:
        return False
    print(*kwargs, **args)
    return True
