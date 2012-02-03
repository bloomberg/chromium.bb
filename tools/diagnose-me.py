#!/usr/bin/python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Diagnose some common system configuration problems on Linux, and
suggest fixes."""

import subprocess
import sys

all_checks = []

def Check(name):
    """Decorator that defines a diagnostic check."""
    def wrap(func):
        all_checks.append((name, func))
        return func
    return wrap


@Check("/usr/bin/ld is not gold")
def CheckSystemLd():
    proc = subprocess.Popen(['/usr/bin/ld', '-v'], stdout=subprocess.PIPE)
    stdout = proc.communicate()[0]
    if 'GNU gold' in stdout:
        return ("When /usr/bin/ld is gold, system updates can silently\n"
                "corrupt your graphics drivers.\n"
                "Try 'sudo apt-get remove binutils-gold'.\n")
    return None


@Check("random lds are not in the $PATH")
def CheckPathLd():
    proc = subprocess.Popen(['which', '-a', 'ld'], stdout=subprocess.PIPE)
    stdout = proc.communicate()[0]
    instances = stdout.split()
    if len(instances) > 1:
        return ("You have multiple 'ld' binaries in your $PATH:\n"
                + '\n'.join(' - ' + i for i in instances) + "\n"
                "You should delete all of them but your system one.\n"
                "gold is hooked into your build via gyp.\n")
    return None


def RunChecks():
    for name, check in all_checks:
        sys.stdout.write("* Checking %s: " % name)
        sys.stdout.flush()
        error = check()
        if not error:
            print "ok"
        else:
            print "FAIL"
            print error


if __name__ == '__main__':
    RunChecks()
