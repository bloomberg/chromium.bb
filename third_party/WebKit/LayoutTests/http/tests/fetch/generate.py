# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Generator script that, for each script-tests/X.js, creates
- window/X.html
- worker/X.html
- serviceworker/X.html
from templates in script-tests/TEMPLATE*.html.

The following tokens in the template files are replaced:
- TESTNAME -> X
- INITSCRIPT -> <script src="../resources/X-init.js"></script>
  and X-init.js is executed on window before X.js is started,
  when X-init.js exists.
  Otherwise, replaced with an empty string.
- OPTIONS -> see below for fetch-access-control tests,
  or empty string otherwise.

Run
$ python generate.py
at this (/LayoutTests/http/tests/fetch/) directory, and
commit the generated files.

fetch-access-control* tests:
These tests contain three configurations based on
BASE_ORIGIN and OTHER_ORIGIN are HTTP or HTTPS:
  ----------------------------- ----------------------- ----------- ------------
  Filename                      OPTIONS                 BASE_ORIGIN OTHER_ORIGIN
  ----------------------------- ----------------------- ----------- ------------
  X.html                        (empty)                 HTTP        HTTP
  X-other-https.html            -other-https            HTTP        HTTPS
  X-base-https-other-https.html -base-https-other-https HTTPS       HTTPS
  ----------------------------- ----------------------- ----------- ------------
'base-https' indicates that BASE_ORIGIN is HTTPS and fetch() is executed on
that HTTPS BASE_ORIGIN.
'other-https' indicates that OTHER_ORIGIN is HTTPS.
(There are no X-base-https.html because of mixed content blocking.)
These test switches by location.href at runtime.
'''

import os
import os.path
import re
import sys

top_path = os.path.dirname(os.path.abspath(__file__))
script_tests_path = os.path.join(top_path, 'script-tests')

def generate(templatename, context, testname, options):
    template_path = os.path.join(
        script_tests_path, templatename + '-' + context + '.html')

    output_basename = testname + options + '.html'

    output_path = os.path.join(top_path, context, output_basename)
    with open(template_path, 'r') as template_file:
        template_data = template_file.read()
        output_data = re.sub(r'TESTNAME', testname, template_data)
        output_data = re.sub(r'OPTIONS', options, output_data)
        if os.path.exists(os.path.join(top_path,
                                       'resources',
                                       testname + '-init.js')):
            output_data = re.sub(r'INITSCRIPT',
                                 '<script src = "../resources/' +
                                 testname + '-init.js"></script>',
                                 output_data)
        else:
            output_data = re.sub(r'INITSCRIPT\n*', '', output_data)

    with open(output_path, 'w') as output_file:
        output_file.write(output_data)

def main():
    basic_contexts = ['window', 'workers', 'serviceworker']

    for script in os.listdir(script_tests_path):
        if script.startswith('.') or not script.endswith('.js'):
            continue
        testname = re.sub(r'\.js$', '', os.path.basename(script))
        templatename = 'TEMPLATE'

        if script.startswith('fetch-access-control'):
            templatename = 'TEMPLATE-fetch-access-control'
            contexts = list(basic_contexts)
            contexts.append('serviceworker-proxied')
            for context in contexts:
                generate(templatename, context, testname, '')
                generate(templatename, context, testname,
                         '-other-https')
                generate(templatename, context, testname,
                         '-base-https-other-https')
        else:
            for context in basic_contexts:
                generate(templatename, context, testname, '')

    return 0

if __name__ == "__main__":
    sys.exit(main())
