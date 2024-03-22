#!/usr/bin/python
#
#  BigPostman - Copyright (c) 2022 Francisco Castro <http://fran.cc>
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in
#  all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.

import time
import crc32
import struct
import serial
import bigpacks

PM_GET    = 0x01
PM_POST   = 0x02
PM_PUT    = 0x03
PM_DELETE = 0x04

PM_201_Created            = 0x21
PM_202_Deleted            = 0x22
PM_204_Changed            = 0x24
PM_205_Content            = 0x25
PM_400_Bad_Request        = 0x40
PM_401_Unauthorized       = 0x41
PM_403_Forbidden          = 0x43
PM_404_Not_Found          = 0x44
PM_405_Method_Not_Allowed = 0x45
PM_413_Request_Entity_Too_Large     = 0x4D

PM_RESPONSE_TEXT = {
    0x21: "201 Created",
    0x22: "202 Deleted",
    0x24: "204 Changed",
    0x25: "205 Content",
    0x40: "400 Bad Request",
    0x41: "401 Unauthorized",
    0x43: "403 Forbidden",
    0x44: "404 Not Found",
    0x45: "405 Method Not Allowed",
    0x4D: "413 Request Entity Too Large",
}

class BigPostmanError(IOError):
    pass

class BigPostman:
    def __init__(self, device, timeout=5):
        self.debug = False
        self.token = 0
        try:
            self.fd = serial.Serial(device, 115200, stopbits=serial.STOPBITS_ONE, parity=serial.PARITY_NONE, timeout=timeout)
        except IOError as err:
            raise BigPostmanError("Failed to open port: %s" % err)

    def send(self, method, token, path, has_payload=False, payload=None):
        frame = bigpacks.pack(method << 24 | token) + bigpacks.pack(path)
        if has_payload:
            frame += bigpacks.pack(payload)
        frame +=  struct.pack("<I", crc32.crc32str(frame))

        if self.debug:
            print("TX:", end='')
            for c in frame:
                print("%02X " % c, end='')
            print("[%i]" % len(frame))

        try:
            self.fd.write(b"\x7E")
            for byte in frame:
                if byte == 0x7E:
                    self.fd.write(b"\x7D\x5E")
                elif byte == 0x7D:
                    self.fd.write(b"\x7D\x5D")
                else:
                    self.fd.write(byte.to_bytes(1, "little"))
            self.fd.write(b"\x7E")
        except IOError as err:
            raise BigPostmanError("Frame sending failed: %s" % err)

    def send_null(self):
        try:
            self.fd.write(b"\x7E")
        except  IOError as err:
            raise BigPostmanError("Frame sending failed: %s" % err)

    def receive(self):
        frame = b""
        byte = b""
        try:
            while (byte != b"\x7E" or len(frame) == 0):
                byte = self.fd.read(1)
                if not byte:
                    raise BigPostmanError("Frame receiving timed out.")
                if byte == b"\x7D":
                    frame += (self.fd.read(1)[0] | 32).to_bytes(1, "little")
                elif byte != b"\x7E":
                    frame += byte
        except BigPostmanError:
            raise
        except  IOError as err:
            raise BigPostmanError("Frame receiving failed: %s" % err)

        if self.debug:
            print("RX:", end='')
            for c in frame:
                print("%02X " % c, end='')
            print("[%i]" % len(frame))


        if len(frame) >= 12 and crc32.crc32str(frame) == 0x2144df1c:
            frame = frame[:-4]
            path = None
            payload = None
            timestamp = None
            identifier = None
            signature = None

            response_token, frame = bigpacks.unpack(frame)
            if frame:
                path, frame = bigpacks.unpack(frame)
            if frame:
                payload, frame = bigpacks.unpack(frame)
            if frame:
                timestamp, frame = bigpacks.unpack(frame)
            if frame:
                identifier, frame = bigpacks.unpack(frame)
            if frame:
                signature, frame = bigpacks.unpack(frame)

            return [response_token >> 24, response_token & 0x00FFFFFF, path, payload, timestamp, identifier, signature]
        else:
            raise BigPostmanError("Frame receiving failed, bad CRC %s != %s " % (hex(crc32.crc32str(frame[:-4])), hex(struct.unpack("<I", frame[-4:])[0])))

    def get(self, path, query=None):
        self.send(PM_GET, self.token, path, query is not None, query)
        response = self.receive()
        if len(response) > 1 and response[1] != self.token:
            raise BigPostmanError("Response token does not match request token.")
        self.token = (self.token + 1) & 0xFFFFFFFF
        return response[0:1] + response[3:]

    def put(self, path, data=None):
        self.send(PM_PUT, self.token, path, data is not None, data)
        response = self.receive()
        if len(response) > 1 and response[1] != self.token:
            raise BigPostmanError("Response token does not match request token.")
        self.token = (self.token + 1) & 0xFFFFFFFF
        return response[0:1] + response[3:]

    def post(self, path, data=None):
        self.send(PM_POST, self.token, path, data is not None, data)
        response = self.receive()
        if len(response) > 1 and response[1] != self.token:
            raise BigPostmanError("Response token does not match request token.")
        self.token = (self.token + 1) & 0xFFFFFFFF
        return response[0:1] + response[3:]

    def delete(self, path, query=None):
        self.send(PM_DELETE, self.token, path, query is not None, query)
        response = self.receive()
        if len(response) > 1 and response[1] != self.token:
            raise BigPostmanError("Response token does not match request token.")
        self.token = (self.token + 1) & 0xFFFFFFFF
        return response[0:1] + response[3:]
