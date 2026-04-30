#!/usr/bin/env python3
"""
WQX Remote Key Sender — send key events to WQX emulator via TCP.

Usage:
    python3 wqx_remote_key.py [host] [port]
    Default: 192.168.50.150:9527

Protocol: each byte = bit7(press=1/release=0) | bit6..0(WQX key_id)

Key mapping (PC keyboard → WQX):
    Arrow keys: UP/DOWN/LEFT/RIGHT
    Enter, Space, ESC, Backspace
    A-Z, 0-9
    F5-F12: function keys (英汉/名片/计算/行程/测验/时间/网络/ON-OFF)
    [ ] \: 求助/中英数/输入法
    ; ': 发音/报时
    , /: 翻页上/翻页下
"""

import sys
import socket
import tty
import termios
import select

# PC key name → WQX key_id
KEY_MAP = {
    # Arrow keys (escape sequences)
    'UP': 0x1A, 'DOWN': 0x1B, 'LEFT': 0x3F, 'RIGHT': 0x1F,
    # Basic keys
    '\r': 0x1D, '\n': 0x1D,   # Enter
    ' ': 0x3E,                  # Space
    '\x1b': 0x3B,               # ESC
    '\x7f': 0x3F,               # Backspace
    '.': 0x3D,
    '-': 0x0E,
    '=': 0x3E,
    '[': 0x38,
    ']': 0x39,
    '\\': 0x3A,
    ',': 0x37,
    '/': 0x1E,
    ';': 0x15,
    "'": 0x14,
    # Numbers
    '0': 0x3C, '1': 0x34, '2': 0x35, '3': 0x36,
    '4': 0x2C, '5': 0x2D, '6': 0x2E,
    '7': 0x24, '8': 0x25, '9': 0x26,
    # Letters
    'a': 0x28, 'b': 0x34, 'c': 0x32, 'd': 0x2A, 'e': 0x22,
    'f': 0x2B, 'g': 0x2C, 'h': 0x2D, 'i': 0x27, 'j': 0x2E,
    'k': 0x2F, 'l': 0x19, 'm': 0x36, 'n': 0x35, 'o': 0x18,
    'p': 0x1C, 'q': 0x20, 'r': 0x23, 's': 0x29, 't': 0x24,
    'u': 0x26, 'v': 0x33, 'w': 0x21, 'x': 0x31, 'y': 0x25,
    'z': 0x30,
}

# F-key escape sequences (after \x1b[): e.g. \x1b[15~ = F5
FKEY_MAP = {
    '15': 0x0B, '17': 0x0C, '18': 0x0D, '19': 0x0A,  # F5-F8
    '20': 0x09, '21': 0x08, '23': 0x0E, '24': 0x0F,  # F9-F12
}

def send_key(sock, key_id, press):
    byte = (0x80 if press else 0x00) | (key_id & 0x7F)
    sock.send(bytes([byte]))

def main():
    host = sys.argv[1] if len(sys.argv) > 1 else '192.168.50.150'
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 9527

    print(f"Connecting to WQX emulator at {host}:{port}...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    print("Connected! Press keys to send. Ctrl+C to quit.\n")
    print("Arrow keys, A-Z, 0-9, Enter, Space, ESC, F5-F12")
    print("ON/OFF = F12, 英汉 = F5\n")

    old_settings = termios.tcgetattr(sys.stdin)
    try:
        tty.setraw(sys.stdin)

        while True:
            if select.select([sys.stdin], [], [], 0.05)[0]:
                ch = sys.stdin.read(1)
                if ch == '\x03':  # Ctrl+C
                    break

                if ch == '\x1b':
                    # Escape sequence
                    if select.select([sys.stdin], [], [], 0.1)[0]:
                        ch2 = sys.stdin.read(1)
                        if ch2 == '[':
                            ch3 = sys.stdin.read(1)
                            if ch3 == 'A':
                                send_key(sock, KEY_MAP['UP'], True)
                                send_key(sock, KEY_MAP['UP'], False)
                                continue
                            elif ch3 == 'B':
                                send_key(sock, KEY_MAP['DOWN'], True)
                                send_key(sock, KEY_MAP['DOWN'], False)
                                continue
                            elif ch3 == 'C':
                                send_key(sock, KEY_MAP['RIGHT'], True)
                                send_key(sock, KEY_MAP['RIGHT'], False)
                                continue
                            elif ch3 == 'D':
                                send_key(sock, KEY_MAP['LEFT'], True)
                                send_key(sock, KEY_MAP['LEFT'], False)
                                continue
                            elif ch3.isdigit():
                                # F-key: read until ~
                                num = ch3
                                while True:
                                    cn = sys.stdin.read(1)
                                    if cn == '~':
                                        break
                                    num += cn
                                if num in FKEY_MAP:
                                    send_key(sock, FKEY_MAP[num], True)
                                    send_key(sock, FKEY_MAP[num], False)
                                continue
                    # Plain ESC
                    send_key(sock, KEY_MAP['\x1b'], True)
                    send_key(sock, KEY_MAP['\x1b'], False)
                    continue

                key = ch.lower()
                if key in KEY_MAP:
                    send_key(sock, KEY_MAP[key], True)
                    send_key(sock, KEY_MAP[key], False)

    except KeyboardInterrupt:
        pass
    finally:
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)
        sock.close()
        print("\nDisconnected.")

if __name__ == '__main__':
    main()
