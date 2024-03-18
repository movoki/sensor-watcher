#!/usr/bin/env python3

#  TinyPostman - Copyright (c) 2012 Francisco Castro <http://fran.cc>
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

import sys
import time
import json
from postman import *

class BytesEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, bytes):
            return obj.hex().upper()
        return json.JSONEncoder.default(self, obj)

usage = """\nUsage: %s <serial port> [action] [resource] [file]

Actions:

    get [resource] [file]     Gets the content of the specified resource. If
                              a JSON file is provided, its content is used as
                              query.

    put <resource> <file>     Puts the content of a JSON file to the specified
                              resource.

    post <resource> <file>    Posts the content of a JSON file to the
                              specified resource.

    delete <resource> [file]  Deletes the specified resource. If a JSON file
                              is provided, its content is used as query.

""" % sys.argv[0]

if len(sys.argv) < 2:
    print(usage)
    sys.exit()

pm = Postman(sys.argv[1], timeout=10)
pm.debug = False
time.sleep(1.5)     # workaround for Arduino bootloader bug that eats the first bytes after opening the port

try:
    if len(sys.argv) > 3:
        action = sys.argv[2]
        resource = sys.argv[3]
        filename = sys.argv[4] if len(sys.argv) > 4 else None
        content = (json.loads(filename) if filename[0] in '[{' else json.load(open(filename, "r"))) if filename else None

        resource = [int(x) if x.isnumeric() else x for x in resource.split('/')]
        resource = [ x for x in resource if x != '' ]

        if action == "get":
            response = pm.get(resource, content)
            if response[0] != PM_205_Content:
                raise PostmanError(PM_RESPONSE_TEXT.get(response[0], hex(response[0])))
            print(json.dumps(response[1], sort_keys=False, indent=4, cls=BytesEncoder))
        elif action == "put" and len(sys.argv) > 4:
            response = pm.put(resource, content)
            if response[0] != PM_204_Changed:
                raise PostmanError(PM_RESPONSE_TEXT.get(response[0], hex(response[0])))
        elif action == "post" and len(sys.argv) > 4:
            response = pm.post(resource, content)
            if response[0] != PM_201_Created:
                raise PostmanError(PM_RESPONSE_TEXT.get(response[0], hex(response[0])))
        elif action == "delete" and len(sys.argv) > 3:
            response = pm.delete(resource, content)
            if response[0] != PM_202_Deleted:
                raise PostmanError(PM_RESPONSE_TEXT.get(response[0], hex(response[0])))
        else:
            print(usage)
            sys.exit()
    else:
        action = "get"
        resource = []
        response = pm.get(resource)
        if response[0] != PM_205_Content:
            raise PostmanError(PM_RESPONSE_TEXT.get(response[0], hex(response[0])))
        print("Resource index:")
        print(json.dumps(response[1], sort_keys=True, indent=4, cls=BytesEncoder))
except PostmanError as err:
    print("Cannot %s the '%s' resource: %s" % (action, resource, err))
