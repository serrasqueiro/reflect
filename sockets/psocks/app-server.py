# app-server.py

# pylint: disable=missing-function-docstring

import sys
import time
import socket
import psocks.libserver as libserver
import psocks.libcommon
from psocks.libcommon import dprint, NiceHost, NiceClient

DEBUG = 1
DEFAULT_PORT = 5000


def main():
    psocks.libcommon.debug = DEBUG
    code = run_server(sys.argv[1:])
    if code is None:
        print(f"""Usage:
{__file__} [host] port
""")
    sys.exit(0 if code is None else code)

def run_server(args):
    param = args
    if not param:
        param = ["127.0.0.1", DEFAULT_PORT]
    if len(param) == 1:
        param = ["0.0.0.0", int(param[0])]
    if len(param) != 2:
        return None
    host, port = param[0], int(param[1])
    ctrl = libserver.Control()
    dprint(f"Starting server {host}:{port}", end=" ...")
    is_ok = do_listen(ctrl, (host, port))
    if not is_ok:
        print(f"Failed to listen: {host}:{port}")
    del ctrl
    return 0

def do_listen(ctrl, addr: tuple):
    host, port = addr
    lsock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    lsock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    #print("Binding socket...", addr)
    try:
        lsock.bind(addr)
    except OSError as os_error:
        ctrl.hint = os_error
        dprint(ctrl.hint, ":::", type(ctrl.hint))
        return False
    lsock.listen()
    dprint(f"Listening at {host}:{port}", end="\n\n")
    # Non-blocking: see example at https://realpython.com/python-sockets/
    lsock.setblocking(False)
    ctrl.register(lsock, libserver.EV_READ, data=None)
    #sel.register(lsock, selectors.EVENT_READ, data=None)
    finito, iter_idx = False, 0
    try:
        while not finito:
            iter_idx += 1
            if not ctrl.is_server_up():
                break
            dprint(f"select() #{iter_idx}", NiceHost(addr))
            events = ctrl.sel.select(timeout=None)
            finito, _ = process_events(ctrl, events, addr)
    except KeyboardInterrupt:
        print("\n... Caught keyboard interrupt, exiting")
    finally:
        dprint("Closing select()")
        ctrl.server_shutdown()
    return True

def process_events(ctrl, events, addr):
    """
    :param ctrl: control object
    :param events:  select() events
    :param addr: just the server information
    :return:
    """
    for key, mask in events:
        #nclient = ctrl.clients.last_client()
        psock, fdclient, iomask, pmsg = key
        if key.data is None:
            new_client = accept_wrapper(ctrl, key.fileobj)
            _, nclient = ctrl.register_client(new_client)
            continue
        clnt_tup = pmsg._address
        idx, nclient = ctrl.get_client(clnt_tup)
        if nclient is None:
            time.sleep(1)
            continue
        dealt = service_connection(ctrl, key, mask, addr, nclient)
        tup = nclient.lastly()[0]
        shown = (psocks.libcommon.stamp_string(tup[0]), f"{tup[1]}byte(s)")
        dprint("stats:", shown)
        dprint(f"key.data.outb {nclient}:", key.data.outb)
        finito = dealt.endswith(".\n")
        shown = dealt.replace("\n", "\\n")
        print(f"::: {nclient} done? {finito}\n" + shown, end="<<<\n\n")
        if finito:
            dprint(f"Closed select for client: {nclient}")
            is_ok = ctrl.closedown(idx)
            assert is_ok, f"Failed to close {nclient}"
            return finito, "OUT"
    return False, "INI"

def accept_wrapper(ctrl, sock):
    """ TCP socket accept wrapper """
    conn, addr = sock.accept()  # Should be ready to read
    print(f"Accepted connection from {NiceHost(addr)}")
    conn.setblocking(False)
    message = libserver.Message(ctrl.sel, conn, addr)
    ctrl.register(conn, libserver.EV_READ, data=message)
    #sel.register(conn, selectors.EVENT_READ, data=message)
    new_client = NiceClient(addr)
    new_client.sock = sock
    return new_client


def service_connection(ctrl, key, mask, addr, nclient):
    dprint("service_connection():", addr)
    sock = key.fileobj
    data = key.data
    if mask & libserver.EV_READ:
        recv_data = sock.recv(1024)  # Should be ready to read
        nclient.connect_read(len(recv_data))
        if recv_data:
            to_str = recv_data.decode("ascii").replace("\r", "")
            data.outb += bytes(to_str, encoding="ascii")
            # ... or data.outb.decode("ascii").endswith("\n.\n")
            return to_str
        dprint(f"Closing connection to {NiceHost(addr)}")
        ctrl.closed_client(sock, nclient)
        #sock.close()
    if mask & libserver.EV_WRITE:
        if data.outb:
            dprint(f"Echoing {data.outb!r} {NiceHost(addr)}")
            sent = sock.send(data.outb)  # Should be ready to write
            data.outb = data.outb[sent:]
    return ""


### Main script ###
if __name__ == "__main__":
    main()
