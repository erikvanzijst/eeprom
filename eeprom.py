#!/usr/bin/env python3
#
# Copyright 2019, Erik van Zijst <erik.van.zijst@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import sys
from struct import pack

from serial import Serial
from serial.tools import list_ports


def atoi(val: str) -> int:
    """Parses an address string into an integer.

    Supports decimal, hexadecimal and octal notation.
    """
    return int(val,
               16 if val.startswith('0x') else
               8 if val.startswith('0o') else
               10)


def read(port: Serial, addr: str) -> None:
    port.write(pack('>BcH', 3, b'r', atoi(addr)))
    val = port.read(1)
    print(int.from_bytes(val, 'big'), '/', '0x' + val.hex())


def write(port: Serial, addr: str, val: str) -> None:
    port.write(pack('>BcHB', 4, b'w', atoi(addr), atoi(val)))
    print(port.read(1))
    print('OK')


def dump(port: Serial, filename: str) -> None:
    with open(filename, 'wb') as f:
        port.write(pack('>Bc', 1, b'd'))

        for i in range(2**15):
            f.write(port.read(1))
            f.flush()
            if i % 100 == 0:
                print('\r%d%%' % ((i / 2**15) * 100), end='')
    print('\nComplete.')


def quit(*args) -> None:
    raise EOFError()


if __name__ == '__main__':
    usage = """AT28C256 EEPROM Programmer

Read or write individual addresses, dump out the full contents to a file, or\
load an image file onto the ERPROM.

To read a single byte:
> [r|read] [addr]

To write a byte to a specific address:
> [w|write] [addr] [value]

To dump the entire EEPROM to a file:
> [d|dump] [filename]

To load a local file into the EEPROM:
> [l|load] [filename]

Address supports hex (0xFF) and octal (0o7) notation.
"""
    parser = argparse.ArgumentParser(description='AT28C256 EEPROM Programmer')
    parser.add_argument('port', nargs='?',
                        help='the serial port the Arduino is '
                             'connected to (on OSX typically '
                             '/dev/tty.usbmodemXXXX)')
    args = parser.parse_args()

    dev = args.port
    if not args.port:
        try:
            # attempt to autodetect the Arduino
            dev = next(
                filter(lambda p: p.product and 'arduino' in p.product.lower(),
                       list_ports.comports())).device
            print('Found Arduino at port', dev)
        except StopIteration:
            print('Cannot find Arduino. If it is connected, specify the port '
                  'manually.', file=sys.stderr)
            exit(1)

    port = Serial(dev, 19200, timeout=3)
    print(usage)
    while True:
        try:
            expr = input('> ').split()
            {'r': read,
             'read': read,
             'w': write,
             'write': write,
             'd': dump,
             'dump': dump,
             'quit': quit,
             'q': quit}[expr[0]](port, *expr[1:])

        except EOFError:
            break
        except (ValueError, KeyError, IndexError, TypeError) as e:
            print('Invalid command:', e)
