# app-server.py

import sys
import socket
import selectors

DEBUG = 1
DEFAULT_PORT = 5000

hint = None


def main():
    run_server(sys.argv[1:])

def run_server(args):
    param = args
    if not param:
        param = ["127.0.0.1", DEFAULT_PORT]
    host, port = param[0], int(param[1])
    sel = selectors.DefaultSelector()
    is_ok = do_listen((host, port), sel)
    if not is_ok:
        print(f"Failed to listen: {host}:{port}")

def do_listen(addr: tuple, sel):
    global hint
    hint = None
    host, port = addr
    lsock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    lsock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    #print("Binding socket...", addr)
    try:
        lsock.bind(addr)
    except OSError as os_error:
        hint = os_error
        dprint(hint, ":::", type(hint))
        return False
    lsock.listen()
    dprint(f"Listening on {host}:{port}", end="\n...\n\n")
    lsock.setblocking(False)
    sel.register(lsock, selectors.EVENT_READ, data=None)
    return True


def dprint(*kwargs, **args):
    if DEBUG <= 0:
        return False
    print(*kwargs)
    return True

### Main script ###
if __name__ == "__main__":
    main()
