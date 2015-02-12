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
- OPTIONS -> OPTIONS string (see README).

Run
$ python generate.py
at this (/LayoutTests/http/tests/fetch/) directory, and
commit the generated files.
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

    with open(output_path, 'w') as output_file:
        output_file.write(output_data)

def main():
    basic_contexts = ['window', 'workers', 'serviceworker']

    for script in os.listdir(script_tests_path):
        if script.startswith('.') or not script.endswith('.js'):
            continue
        testname = re.sub(r'\.js$', '', os.path.basename(script))
        templatename = 'TEMPLATE'
        contexts = list(basic_contexts)
        options = ['', '-base-https-other-https']

        # fetch-access-control tests.
        if script.startswith('fetch-access-control'):
            templatename = 'TEMPLATE-fetch-access-control'
            contexts.append('serviceworker-proxied')
            options = ['', '-other-https', '-base-https-other-https']

        # Read OPTIONS list.
        with open(os.path.join(script_tests_path, script), 'r') as script_file:
            script = script_file.read()
            m = re.search(r'// *OPTIONS: *([a-z\-,]*)', script)
            if m:
                options = re.split(',', m.group(1))

        for context in contexts:
            for option in options:
                assert(option in ['', '-other-https', '-base-https',
                                  '-base-https-other-https'])
                generate(templatename, context, testname, option)

    return 0

if __name__ == "__main__":
    sys.exit(main())
