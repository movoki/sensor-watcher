#!/usr/bin/python
#
#  BigPAcks - Copyright (c) 2022 Francisco Castro <http://fran.cc>
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

import struct

BP_FALSE    = 0x00000000
BP_TRUE     = 0x10000000
BP_NONE     = 0x20000000
BP_INTEGER  = 0x40000000
BP_FLOAT    = 0x50000000

BP_LIST     = 0x80000000
BP_MAP      = 0x90000000
BP_STRING   = 0xC0000000
BP_BINARY   = 0xD0000000

BP_LENGTH_MAX      = 0x0FFFFFFF
BP_LENGTH_MASK     = 0x0FFFFFFF
BP_TYPE_MASK       = 0xF0000000
BP_TYPE_GROUP_MASK = 0xE0000000
BP_BOOLEAN_MASK    = 0x10000000


def bit_len(n):
    s = bin(n)       # binary representation:  bin(-37) --> '-0b100101'
    s = s.lstrip('-0b') # remove leading zeros and minus sign
    return len(s)       # len('100101') --> 6

def pack(obj, use_double=False):
    if obj is None:
        return struct.pack("<I", BP_NONE)
    elif isinstance(obj, bool):
        return struct.pack("<I", BP_TRUE if obj else BP_FALSE)
    elif isinstance(obj, int):
        bit_length = bit_len(obj)
        if bit_length <= 32:
            return struct.pack("<Ii", BP_INTEGER | 1, obj)
        elif bit_length <= 64:
            return struct.pack("<Iq", BP_INTEGER | 2, obj)
        else:
            raise ValueError("Integer number too big")
    elif isinstance(obj, float):
        if not use_double:
            return struct.pack("<If", BP_FLOAT | 1, obj)
        else:
            return struct.pack("<Id", BP_FLOAT | 2, obj)
    elif isinstance(obj, str):
        content = obj.encode('utf_8')
        content_length = (len(content) + 1 + 3) // 4
        if content_length <= BP_LENGTH_MAX:
            return struct.pack("<I%is" % (content_length * 4), BP_STRING | content_length, content)
        else:
            raise ValueError("String too long")
    elif isinstance(obj, bytearray) or isinstance(obj, bytes):
        content_length = (len(obj) + 3) // 4
        if content_length <= BP_LENGTH_MAX:
            return struct.pack("<I%is" % (content_length * 4), BP_BINARY | content_length, obj)
        else:
            raise ValueError("Bytearray too long")
    elif isinstance(obj, (list, tuple)):
        content = b''.join([pack(value) for value in obj])
        content_length = len(content) // 4
        if content_length <= BP_LENGTH_MAX:
            return struct.pack("<I%is" % (content_length * 4), BP_LIST | content_length, content)
        else:
            raise ValueError("List too long")
    elif isinstance(obj, dict):
        elements = []
        for item in obj.items():
            elements.append(pack(item[0]))
            elements.append(pack(item[1]))
        content = b''.join(elements)
        content_length = len(content) // 4
        if content_length <= BP_LENGTH_MAX:
            return struct.pack("<I%is" % (content_length * 4), BP_MAP | content_length, content)
        else:
            raise ValueError("Dict too long")
    else:
        raise ValueError("Unknown type")
        

def unpack(data):
    if len(data) == 0:
        ValueError("Cannot unpack an empty pack")
    element_header = struct.unpack("<I", data[0:4])[0]
    content_type = element_header & BP_TYPE_MASK
    content_length = element_header & BP_LENGTH_MASK
    element_length = content_length + 1
    content_raw = data[4 : element_length * 4]
    
    if content_type == BP_NONE:
        obj = None
    elif content_type == BP_TRUE:
        obj = True
    elif content_type == BP_FALSE:
        obj = False
    elif content_type == BP_INTEGER:
        if content_length == 1:
            obj = struct.unpack("<i", content_raw)[0]
        elif content_length == 2:
            obj = struct.unpack("<q", content_raw)[0]
        else:
            raise ValueError("Integer number too big")
    elif content_type == BP_FLOAT:
        if content_length == 1:
            obj = struct.unpack("<f", content_raw)[0]
        elif content_length == 2:
            obj = struct.unpack("<d", content_raw)[0]
        else:
            raise ValueError("Float number too big")
    elif content_type == BP_STRING:
        try:
            obj = content_raw.decode("utf8").split('\0',1)[0]
        except:
            obj = ""
    elif content_type == BP_BINARY:
        obj = content_raw
    elif content_type == BP_LIST:
        obj = []
        while(content_raw):
            item, content_raw = unpack(content_raw)
            obj.append(item)
    elif content_type == BP_MAP:
        obj = {}
        while(content_raw):
            key, content_raw = unpack(content_raw)
            value, content_raw = unpack(content_raw)
            obj[key] = value
    
    return (obj, data[element_length * 4:])
    