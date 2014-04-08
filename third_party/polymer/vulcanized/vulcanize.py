#!/usr/bin/python2.6
# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess

# This script vulcanizes all parts of the polymer library that are included in
# polymer-elements.in.html. This is required because the polymer library is not
# CSP compliant and Chrome extensions that use polymer have CSP enforced.
# The vulcanize tool cannot currently be added to the Chrome build which is why
# this must be run manually when polymer is revved. This script should be re-run
# whenever we want polymer is updated.
#
# See https://code.google.com/p/chromium/issues/detail?id=359333 for more
# details.
#
# Once this script has been run, the entire polymer library will be inside of
# polymer-elements.html and polymer-elements.js.
#
# TODO(raymes): This is ugly. Remove this as soon as we can fix
# crbug.com/359333.

POLYMER_ELEMENTS_INPUT = "polymer-elements.in.html"
POLYMER_ELEMENTS_OUTPUT = "polymer-elements.html"

subprocess.call(["vulcanize", "--csp", POLYMER_ELEMENTS_INPUT,
                 "-o", POLYMER_ELEMENTS_OUTPUT]);
