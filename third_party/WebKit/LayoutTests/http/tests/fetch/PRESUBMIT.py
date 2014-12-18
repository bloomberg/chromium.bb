# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Chromium presubmit script for fetch API layout tests.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts.
'''

import os
import os.path
import re


def missing_files(scripts_path, top_path, worker_path):
    for script in os.listdir(scripts_path):
        basename = re.sub(r'\.js$', '.html', os.path.basename(script))
        window_path = os.path.join(top_path, basename)
        worker_path = os.path.join(worker_path, basename)
        for path in [window_path, worker_path]:
            if not os.path.exists(path):
                yield path


def CheckChangeOnUpload(input, output):
    top_path = input.PresubmitLocalPath()
    worker_path = os.path.join(top_path, 'workers')
    script_tests_path = os.path.join(top_path, 'script-tests')

    return [output.PresubmitPromptWarning('%s is missing' % path) for path
            in missing_files(script_tests_path, top_path, worker_path)]
