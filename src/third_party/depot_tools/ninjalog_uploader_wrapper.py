#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import json
import sys

from third_party import httplib2

import ninjalog_uploader

THIS_DIR = os.path.dirname(__file__)
UPLOADER = os.path.join(THIS_DIR, 'ninjalog_uploader.py')
CONFIG = os.path.join(THIS_DIR, 'ninjalog.cfg')
VERSION = 1


def LoadConfig():
    if os.path.isfile(CONFIG):
        with open(CONFIG, 'rb') as f:
            config = json.load(f)
            if config['version'] == VERSION:
                config['countdown'] -= 1
                return config

    return {
        'is-googler': ninjalog_uploader.IsGoogler(
            'chromium-build-stats.appspot.com'),
        'countdown': 10,
        'version': VERSION,
    }


def SaveConfig(config):
    with open(CONFIG, 'wb') as f:
        json.dump(config, f)


def ShowMessage(countdown):
    print """
Your ninjalog will be uploaded to build stats server. Uploaded log will be used
to analyze user side build performance.

The following information will be uploaded with ninjalog.
* OS (e.g. Win, Mac or Linux)
* build directory (e.g. /home/foo/chromium/src/out/Release)
* hostname
* number of cpu cores of building machine
* cmdline passed to ninja (e.g. ninja -C out/Default -j1024 chrome)
* build config (e.g. use_goma=true, is_component_build=true, etc)

Uploading ninjalog will be started after you run autoninja another %d time.

If you don't want to upload ninjalog, please run following command.
$ %s opt-out

If you allow upload ninjalog from next autoninja run, please run the following
command.
$ %s opt-in

If you have question about this, please send mail to infra-dev@chromium.org

""" % (countdown, __file__, __file__)


def main():
    config = LoadConfig()

    if len(sys.argv) == 2 and sys.argv[1] == 'opt-in':
        config['opt-in'] = True
        config['countdown'] = 0
        SaveConfig(config)
        print('ninjalog upload is opted in.')
        return 0

    if len(sys.argv) == 2 and sys.argv[1] == 'opt-out':
        config['opt-in'] = False
        SaveConfig(config)
        print('ninjalog upload is opted out.')
        return 0

    SaveConfig(config)

    if 'opt-in' in config and not config['opt-in']:
        # Upload is opted out.
        return 0

    if not config.get("is-googler", False):
        # Not googler.
        return 0

    if config.get("countdown", 0) > 0:
        # Need to show message.
        ShowMessage(config["countdown"])
        return 0

    if len(sys.argv) == 1:
        # dry-run for debugging.
        print("upload ninjalog dry-run")
        return 0

    # Run upload script without wait.
    devnull = open(os.devnull, "w")
    subprocess.Popen([sys.executable, UPLOADER] + sys.argv[1:],
                     stdout=devnull, stderr=devnull)


if __name__ == '__main__':
    sys.exit(main())
