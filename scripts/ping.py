#!/usr/bin/env python3
"""Minecraft server-list ping against a Cinder instance."""
import socket
import struct
import sys


def varint(n: int) -> bytes:
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        if n:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def pack(pkt_id: int, payload: bytes) -> bytes:
    body = varint(pkt_id) + payload
    return varint(len(body)) + body


def read_varint(sock: socket.socket) -> int:
    n = 0
    shift = 0
    while True:
        b = sock.recv(1)
        if not b:
            raise EOFError("eof in varint")
        n |= (b[0] & 0x7F) << shift
        if b[0] < 0x80:
            return n
        shift += 7


def ping(host: str = "127.0.0.1", port: int = 25565, proto: int = 776) -> str:
    s = socket.create_connection((host, port), timeout=5)
    try:
        addr = host.encode()
        handshake = (
            varint(proto)
            + varint(len(addr))
            + addr
            + struct.pack(">H", port)
            + varint(1)
        )
        s.sendall(pack(0, handshake))
        s.sendall(pack(0, b""))
        length = read_varint(s)
        data = b""
        while len(data) < length:
            chunk = s.recv(length - len(data))
            if not chunk:
                raise EOFError("eof in status")
            data += chunk
        # skip packet id
        i = 0
        n = 0
        shift = 0
        while True:
            b = data[i]
            i += 1
            n |= (b & 0x7F) << shift
            if b < 0x80:
                break
            shift += 7
        slen = 0
        shift = 0
        while True:
            b = data[i]
            i += 1
            slen |= (b & 0x7F) << shift
            if b < 0x80:
                break
            shift += 7
        return data[i : i + slen].decode("utf-8")
    finally:
        s.close()


if __name__ == "__main__":
    host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 25565
    print(ping(host, port))
