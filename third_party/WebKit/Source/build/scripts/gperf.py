# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Invokes gperf for the GN build.
# Usage: gperf.py [--developer_dir PATH_TO_XCODE] gperf ...
# TODO(brettw) this can be removed once the run_executable rules have been
# checked in for the GN build.

import argparse
import os
import subprocess
import sys
import template_expander


def generate_gperf(gperf_path, gperf_input, extra_args=None):
    # FIXME: If we could depend on Python 2.7, we would use
    # subprocess.check_output
    gperf_args = [gperf_path, '--key-positions=*', '-P', '-n']
    gperf_args.extend(['-m', '50'])  # Pick best of 50 attempts.
    gperf_args.append('-D')  # Allow duplicate hashes -> More compact code.
    if extra_args:
        gperf_args.extend(extra_args)

    # If gperf isn't in the path we get an OSError. We don't want to use
    # the normal solution of shell=True (as this has to run on many
    # platforms), so instead we catch the error and raise a
    # CalledProcessError like subprocess would do when shell=True is set.
    try:
        gperf = subprocess.Popen(
            gperf_args,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            universal_newlines=True)
        return gperf.communicate(gperf_input)[0]
    except OSError:
        raise subprocess.CalledProcessError(
            127, gperf_args, output='Command not found.')


def use_jinja_gperf_template(template_path, gperf_extra_args=None):
    def wrapper(generator):
        def generator_internal(*args, **kwargs):
            parameters = generator(*args, **kwargs)
            assert 'gperf_path' in parameters, 'Must specify gperf_path in ' \
                'template map returned from decorated function'
            gperf_path = parameters['gperf_path']
            gperf_input = template_expander.apply_template(template_path,
                                                           parameters)
            return generate_gperf(gperf_path, gperf_input, gperf_extra_args)
        generator_internal.func_name = generator.func_name
        return generator_internal
    return wrapper


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--developer_dir", required=False)
    args, unknownargs = parser.parse_known_args()
    if args.developer_dir:
        os.environ['DEVELOPER_DIR'] = args.developer_dir

    subprocess.check_call(unknownargs)

if __name__ == '__main__':
    main()
