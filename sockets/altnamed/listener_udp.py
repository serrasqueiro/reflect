#-*- coding: utf-8 -*-
# listener_udp.py  (c)2020  Henrique Moreira

"""
listener_udp -- UDP server
"""

# pylint: disable=unused-argument, invalid-name, global-statement

import sys
import socket
from threading import Thread
from time import sleep

exiter = False


def main():
    """ Main script
    """
    code = main_run(sys.stdout, sys.stderr, sys.argv[1:])
    if code is None:
        print(f"""{__file__} command [arg ...]

Commands are:
    bind          Binds, use: bind local-IP local-port
""")
    sys.exit(code if isinstance(code, int) else 0)


def main_run(out, err, args):
    """ Main run """
    # inspired from
    #	http://sfriederichs.github.io/how-to/python/udp/2017/12/07/UDP-Communication.html
    opts = {"verbose": 0,
            }
    if not args:
        params = ("bind", "localhost", "8000")
    else:
        params = args
    cmd = params[0]
    param = params[1:]
    while param and param[0].startswith("-"):
        if param[0].startswith("-v"):
            opts["verbose"] += param[0].count("v")
            del param[0]
            continue
        if param[0] == "-1":
            opts["few"] = 1
            del param[0]
            continue
        return None
    assert opts["verbose"] <= 3
    if cmd == "bind":
        ip = param[0]
        port = param[1]
        if ip == ".":
            ip = ""
        code = do_bind(ip, port, opts)
        return code
    return None


def do_bind(ip, port, opts, debug=0) -> int:
    """ bind (main server) """
    global exiter
    if debug >= 0:
        print(f"do_bind({ip}, {port}, opts, debug={debug}")
    if not port:
        return None
    port_num = int(port)

    udpRxThreadHandle = Thread(target=rxThread,args=(port_num,))
    udpRxThreadHandle.start()

    sleep(0.2)

    #Generate a transmit socket object
    txSocket = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)

    #Do not block when looking for received data (see above note)
    txSocket.setblocking(0)

    print(f"Transmitting to {ip}:{port_num}")
    while True:
        try:
            # Retrieve input data
            txChar = input("TX: ")
            txSend = txChar.encode("ascii")

            # Transmit data to the local server on the agreed-upon port
            txSocket.sendto(txSend, (ip, port_num))

            # Sleep to allow the other thread to process the data
            sleep(0.2)

            # Attempt to receive the echo from the server
            data, addr = txSocket.recvfrom(1024)
        except socket.error:
            #If no data is received you end up here, but you can ignore
            #the error and continue
            continue
        except KeyboardInterrupt:
            exiter = True
            print("Received Ctrl+C... initiating exit")
            break
        print(f"RX from {addr}: {data}")
        sleep(0.2)

    udpRxThreadHandle.join()
    return 0


def rxThread(port):
    """ Receive thread """
    global exiter

    assert isinstance(port, int)

    # Generate a UDP socket
    rxSocket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # DGRAM = UDP

    # Bind to any available address on port 'port'
    rxSocket.bind(("", port))

    #Prevent the socket from blocking until it receives all the data it wants
    #Note: Instead of blocking, it will throw a socket.error exception if it
    #doesn't get any data
    rxSocket.setblocking(0)

    print(f"RX: Receiving data on UDP port {port}")

    while not exiter:
        try:
            # Attempt to receive up to 1024 bytes of data
            data, addr = rxSocket.recvfrom(1024)
            # Echo the data back to the sender
            back = data
            rxSocket.sendto(back, addr)
        except socket.error:
            #If no data is received, you get here, but it's not an error
            #Ignore and continue
            pass
        sleep(0.2)


def txThread(port):
    """ Transmit thread """
    global exiter


# Main script
if __name__ == "__main__":
    main()
