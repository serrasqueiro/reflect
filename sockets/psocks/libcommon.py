# libcommon.py -- (c)2022 Henrique Moreira

""" Common wrappers and basic functions
"""

import socket
import time

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
        self._time = simple_stamp()
        self._last_reads, self._last_writes = [0, 0], [0, 0]

    def closedown(self, shut_up=False):
        """ Shutdown socket - shut_up=True for servers closing client's connections.
        """
        assert isinstance(shut_up, bool)
        if self.sock is None:
            return False
        if shut_up:
            self.sock.shutdown(socket.SHUT_RDWR)
        self.sock.close()
        del self.sock
        self.sock = None
        return True

    def connect_read(self, size:int) -> bool:
        self._last_reads = [simple_stamp(), size]
        return size > 0

    def connect_write(self, size:int) -> bool:
        self._last_writes = [simple_stamp(), size]
        return size > 0

    def since(self):
        return self._time

    def lastly(self):
        return self._last_reads, self._last_writes

def simple_stamp() -> int:
    return int(time.time())

def stamp_string(clk_time) -> str:
    atime = clk_time
    dttm = time.localtime(atime)
    astr = time.strftime("%Y-%m-%d %H:%M:%S", dttm)
    return astr

def dprint(*kwargs, **args):
    if debug <= 0:
        return False
    print(*kwargs, **args)
    return True
