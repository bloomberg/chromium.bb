#!/usr/bin/env python
#
# Copyright 2020 Google LLC
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys

sksl_precompile = sys.argv[1]
targetDir = sys.argv[2]
modules = sys.argv[3:]

dependencies = {
    'sksl_frag': ['sksl_gpu'],
    'sksl_vert': ['sksl_gpu'],
    'sksl_graphite_frag': ['sksl_frag', 'sksl_gpu'],
    'sksl_graphite_vert': ['sksl_vert', 'sksl_gpu'],
    'sksl_rt_shader': ['sksl_public'],
}

for module in modules:
    try:
        moduleDir, moduleName = os.path.split(os.path.splitext(module)[0])
        if not os.path.isdir(targetDir):
            os.mkdir(targetDir)
        target = os.path.join(targetDir, moduleName)
        args = [sksl_precompile, target + ".dehydrated.sksl", module]
        if moduleName in dependencies:
            for dependent in dependencies[moduleName]:
                args.append(os.path.join(moduleDir, dependent) + ".sksl")
        subprocess.check_output(args).decode('utf-8')
    except subprocess.CalledProcessError as err:
        print("### Error compiling " + module + ":")
        print(err.output)
        exit(1)
