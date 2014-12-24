# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Generator script that, for each script-tests/X.js, creates
- X.html
- worker/X.html
- serviceworker/X.html
from templates in script-tests/TEMPLATE*.html.

Run
$ python generate.py
at this (/LayoutTests/http/tests/fetch/) directory, and
commit the generated files.
'''

import os
import os.path
import re
import sys

def main():
    contexts = ['window', 'workers', 'serviceworker']

    top_path = os.path.dirname(os.path.abspath(__file__))
    script_tests_path = os.path.join(top_path, 'script-tests')

    for script in os.listdir(script_tests_path):
        if script.startswith('.') or not script.endswith('.js'):
            continue
        testname = re.sub(r'\.js$', '', os.path.basename(script))
        basename = testname + '.html'

        for context in contexts:
            template_path = os.path.join(
                script_tests_path, 'TEMPLATE-' + context + '.html')
            with open(template_path, 'r') as template_file:
                template_data = template_file.read()
                output_data = re.sub(r'TESTNAME', testname, template_data)

            output_path = os.path.join(top_path, context, basename)
            with open(output_path, 'w') as output_file:
                output_file.write(output_data)
    return 0

if __name__ == "__main__":
    sys.exit(main())
