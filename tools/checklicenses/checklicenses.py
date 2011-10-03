#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Makes sure that all files contain proper licensing information."""


import optparse
import os.path
import subprocess
import sys


def PrintUsage():
  print """Usage: python checklicenses.py [--root <root>] [tocheck]
  --root   Specifies the repository root. This defaults to "../.." relative
           to the script file. This will be correct given the normal location
           of the script in "<root>/tools/checklicenses".

  --ignore-suppressions  Ignores path-specific license whitelist. Useful when
                         trying to remove a suppression/whitelist entry.

  tocheck  Specifies the directory, relative to root, to check. This defaults
           to "." so it checks everything.

Examples:
  python checklicenses.py
  python checklicenses.py --root ~/chromium/src third_party"""


WHITELISTED_LICENSES = [
    'Apache (v2.0)',
    'Apache (v2.0) BSD (2 clause)',
    'Apache (v2.0) GPL (v2)',
    'BSD',
    'BSD (2 clause)',
    'BSD (2 clause) MIT/X11 (BSD like)',
    'BSD (3 clause)',
    'BSD (3 clause) ISC',
    'BSD (3 clause) LGPL (v2.1 or later)',
    'BSD (3 clause) MIT/X11 (BSD like)',
    'BSD (4 clause)',
    'BSD-like',

    # TODO(phajdan.jr): Make licensecheck not print BSD-like twice.
    'BSD-like MIT/X11 (BSD like)',

    'BSL (v1.0)',
    # TODO(phajdan.jr): Make licensecheck not print the comma after 3.1.
    'BSL (v1.0) GPL (v3.1,)',
    'ISC',
    'LGPL',
    'LGPL (v2)',
    'LGPL (v2 or later)',
    'LGPL (v2.1)',
    'LGPL (v3 or later)',

    # TODO(phajdan.jr): Make licensecheck convert that comma to a dot.
    'LGPL (v2,1 or later)',

    'LGPL (v2.1 or later)',
    'MPL (v1.0) LGPL (v2 or later)',
    'MPL (v1.1)',
    'MPL (v1.1) BSD-like',
    'MPL (v1.1) BSD-like GPL (unversioned/unknown version)',
    'MPL (v1.1) GPL (unversioned/unknown version)',

    # TODO(phajdan.jr): Make licensecheck not print the comma after 1.1.
    'MPL (v1.1,) GPL (unversioned/unknown version) LGPL (v2 or later)',
    'MPL (v1.1,) GPL (unversioned/unknown version) LGPL (v2.1 or later)',

    'MIT/X11 (BSD like)',
    'Ms-PL',
    'Public domain',
    'libpng',
    'zlib/libpng',
]


PATH_SPECIFIC_WHITELISTED_LICENSES = {
    'base/third_party/dmg_fp': [
        'UNKNOWN',
    ],
    'base/third_party/icu': [
        'UNKNOWN',
    ],
    'breakpad/src': [
        'UNKNOWN',
    ],
    'chrome/common/extensions/docs/examples': [
        'UNKNOWN',
    ],
    'chrome/test/data/layout_tests/LayoutTests': [
        'UNKNOWN',
    ],
    'courgette/third_party/bsdiff_create.cc': [
        'UNKNOWN',
    ],
    'data/mozilla_js_tests': [
        'UNKNOWN',
    ],
    'data/page_cycler': [
        'UNKNOWN',
        'GPL (v2 or later)',
    ],
    'data/tab_switching': [
        'UNKNOWN',
    ],
    'googleurl': [
        'UNKNOWN',
    ],
    'gpu/GLES2': [
        'UNKNOWN',
    ],
    'gpu/KHR': [
        'UNKNOWN',
    ],
    'gpu/gles2_conform_support/egl/native/EGL': [
        'UNKNOWN',
    ],
    'native_client': [
        'UNKNOWN',
    ],
    'native_client/toolchain': [
        'BSD GPL (v2 or later)',
        'BSD (2 clause) GPL (v2 or later)',
        'BSD (3 clause) GPL (v2 or later)',
        'BSL (v1.0) GPL',
        'GPL',
        'GPL (unversioned/unknown version)',
        'GPL (v2)',

        # TODO(phajdan.jr): Make licensecheck not print the comma after v2.
        'GPL (v2,)',

        'GPL (v2 or later)',

        # TODO(phajdan.jr): Make licensecheck not print the comma after 3.1.
        'GPL (v3.1,)',
    ],
    'net/disk_cache/hash.cc': [
        'UNKNOWN',
    ],
    'net/tools/spdyshark': [
        'GPL (v2 or later)',
        'UNKNOWN',
    ],
    'ppapi/c/documentation/check.sh': [
        'UNKNOWN',
    ],
    'ppapi/cpp/documentation/check.sh': [
        'UNKNOWN',
    ],
    'ppapi/lib/gl/include': [
        'UNKNOWN',
    ],
    'ppapi/native_client/tests/earth/earth_image.inc': [
        'UNKNOWN',
    ],
    'sdch/open-vcdiff': [
        'UNKNOWN',
    ],
    'third_party/WebKit': [
        'UNKNOWN',
    ],
    'third_party/WebKit/Source/ThirdParty/ANGLE/src/compiler': [
        'GPL',
    ],
    'third_party/WebKit/Source/JavaScriptCore/tests/mozilla': [
        'GPL',
        'GPL (unversioned/unknown version)',
    ],
    'third_party/active_doc': [
        'UNKNOWN',
    ],
    'third_party/apple/ImageAndTextCell.h': [
        'UNKNOWN',
    ],
    'third_party/apple_apsl': [
        'UNKNOWN',
    ],
    'third_party/angle': [
        'UNKNOWN',
    ],
    'third_party/angle/src/compiler': [
        'GPL',
    ],
    'third_party/ashmem/ashmem.h': [
        'UNKNOWN',
    ],
    'third_party/bsdiff/mbsdiff.cc': [
        'UNKNOWN',
    ],
    'third_party/bzip2': [
        'UNKNOWN',
    ],
    'third_party/cld/encodings/compact_lang_det': [
        'UNKNOWN',
    ],
    'third_party/devscripts': [
        'GPL (v2 or later)',
    ],
    'third_party/expat/files/lib': [
        'UNKNOWN',
    ],
    'third_party/ffmpeg': [
        'GPL',
        'GPL (v2 or later)',
        'UNKNOWN',
    ],
    'third_party/gles2_book': [
        'UNKNOWN',
    ],
    'third_party/gles2_conform/GTF_ES': [
        'UNKNOWN',
    ],
    'third_party/gpsd/release-2.38/gps.h': [
        'UNKNOWN',
    ],
    'third_party/harfbuzz': [
        'UNKNOWN',
    ],
    'third_party/hunspell': [
        'UNKNOWN',
    ],
    'third_party/iccjpeg': [
        'UNKNOWN',
    ],
    'third_party/icu': [
        'UNKNOWN',
    ],
    'third_party/jemalloc': [
        'UNKNOWN',
    ],
    'third_party/lcov': [
        'UNKNOWN',
    ],
    'third_party/lcov/contrib/galaxy/genflat.pl': [
        'GPL (v2 or later)',
    ],
    'third_party/lcov-1.9/contrib/galaxy/genflat.pl': [
        'GPL (v2 or later)',
    ],
    'third_party/leveldatabase/src/util/posix_logger.h': [
        'UNKNOWN',
    ],
    'third_party/libevent': [
        'UNKNOWN',
    ],
    'third_party/libjingle/source/talk': [
        'UNKNOWN',
    ],
    'third_party/libjpeg': [
        'UNKNOWN',
    ],
    'third_party/libjpeg_turbo': [
        'UNKNOWN',
    ],
    'third_party/libphonenumber/cpp/src': [
        'UNKNOWN',
    ],
    'third_party/libpng': [
        'UNKNOWN',
    ],
    'third_party/libvpx/source': [
        'UNKNOWN',
    ],
    'third_party/libvpx/source/libvpx/examples/includes': [
        'GPL (v2 or later)',
    ],
    'third_party/libwebp': [
        'UNKNOWN',
    ],
    'third_party/libxml': [
        'UNKNOWN',
    ],
    'third_party/libxslt': [
        'UNKNOWN',
    ],
    'third_party/lzma_sdk': [
        'UNKNOWN',
    ],
    'third_party/mesa/MesaLib': [
        'GPL (v2)',
        'GPL (v3 or later)',
        'UNKNOWN',
    ],
    'third_party/modp_b64': [
        'UNKNOWN',
    ],
    'third_party/npapi/npspy/extern/java': [
        'GPL (unversioned/unknown version)',
    ],
    'third_party/openssl': [
        'UNKNOWN',
    ],
    'third_party/ots/tools/ttf-checksum.py': [
        'UNKNOWN',
    ],
    'third_party/molokocacao/NSBezierPath+MCAdditions.h': [
        'UNKNOWN',
    ],
    'third_party/npapi/npspy': [
        'UNKNOWN',
    ],
    'third_party/ocmock/OCMock': [
        'UNKNOWN',
    ],
    'third_party/ply/__init__.py': [
        'UNKNOWN',
    ],
    'third_party/protobuf': [
        'UNKNOWN',
    ],
    'third_party/psutil': [
        'UNKNOWN',
    ],
    'third_party/pyftpdlib/src': [
        'UNKNOWN',
    ],
    'third_party/pylib': [
        'UNKNOWN',
    ],
    'third_party/scons-2.0.1/engine/SCons': [
        'UNKNOWN',
    ],
    'third_party/simplejson': [
        'UNKNOWN',
    ],
    'third_party/skia': [
        'UNKNOWN',
    ],
    'third_party/snappy/src': [
        'UNKNOWN',
    ],
    'third_party/smhasher/src': [
        'UNKNOWN',
    ],
    'third_party/sqlite': [
        'UNKNOWN',
    ],
    'third_party/swig/Lib/linkruntime.c': [
        'UNKNOWN',
    ],
    'third_party/talloc': [
        'GPL (v3 or later)',
        'UNKNOWN',
    ],
    'third_party/tcmalloc': [
        'UNKNOWN',
    ],
    'third_party/tlslite': [
        'UNKNOWN',
    ],
    'third_party/webdriver': [
        'UNKNOWN',
    ],
    'third_party/webrtc': [
        'UNKNOWN',
    ],
    'third_party/xdg-utils': [
        'UNKNOWN',
    ],
    'third_party/yasm/source': [
        'UNKNOWN',
    ],
    'third_party/zlib/contrib/minizip': [
        'UNKNOWN',
    ],
    'third_party/zlib/trees.h': [
        'UNKNOWN',
    ],
    'tools/dromaeo_benchmark_runner/dromaeo_benchmark_runner.py': [
        'UNKNOWN',
    ],
    'tools/emacs': [
        'UNKNOWN',
    ],
    'tools/grit/grit/node/custom/__init__.py': [
        'UNKNOWN',
    ],
    'tools/gyp/test': [
        'UNKNOWN',
    ],
    'tools/histograms': [
        'UNKNOWN',
    ],
    'tools/memory_watcher': [
        'UNKNOWN',
    ],
    'tools/playback_benchmark': [
        'UNKNOWN',
    ],
    'tools/python/google/__init__.py': [
        'UNKNOWN',
    ],
    'tools/site_compare': [
        'UNKNOWN',
    ],
    'tools/stats_viewer/Properties/AssemblyInfo.cs': [
        'UNKNOWN',
    ],
    'tools/symsrc/pefile.py': [
        'UNKNOWN',
    ],
    'tools/valgrind': [
        'UNKNOWN',
    ],
    'v8/test/cctest': [
        'UNKNOWN',
    ],
    'webkit/data/ico_decoder': [
        'UNKNOWN',
    ],
}


def main(options, args):
  # Figure out which directory we have to check.
  if len(args) == 0:
    # No directory to check specified, use the repository root.
    start_dir = options.base_directory
  elif len(args) == 1:
    # Directory specified. Start here. It's supposed to be relative to the
    # base directory.
    start_dir = os.path.abspath(os.path.join(options.base_directory, args[0]))
  else:
    # More than one argument, we don't handle this.
    PrintUsage()
    sys.exit(1)

  print "Using base directory:", options.base_directory
  print "Checking:", start_dir
  print

  licensecheck_path = os.path.abspath(os.path.join(options.base_directory,
                                                   'third_party',
                                                   'devscripts',
                                                   'licensecheck.pl'))

  licensecheck = subprocess.Popen([licensecheck_path, '-r', start_dir],
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE)
  stdout, stderr = licensecheck.communicate()
  if options.verbose:
    print '----------- licensecheck stdout -----------'
    print stdout
    print '--------- end licensecheck stdout ---------'
  if licensecheck.returncode != 0 or stderr:
    print '----------- licensecheck stderr -----------'
    print stderr
    print '--------- end licensecheck stderr ---------'
    print "\nFAILED\n"
    sys.exit(1)

  success = True
  for line in stdout.splitlines():
    filename, license = line.split(':', 1)
    filename = os.path.relpath(filename.strip(), options.base_directory)

    # All files in the build output directory are generated one way or another.
    # There's no need to check them.
    if filename.startswith('out/') or filename.startswith('sconsbuild/'):
      continue

    # For now we're just interested in the license.
    license = license.replace('*No copyright*', '').strip()

    # Skip generated files.
    if 'GENERATED FILE' in license:
      continue

    if license in WHITELISTED_LICENSES:
      continue

    if not options.ignore_suppressions:
      found_path_specific = False
      for prefix in PATH_SPECIFIC_WHITELISTED_LICENSES:
        if (filename.startswith(prefix) and
            license in PATH_SPECIFIC_WHITELISTED_LICENSES[prefix]):
          found_path_specific = True
          break
      if found_path_specific:
        continue

    print "'%s' has non-whitelisted license '%s'" % (filename, license)
    success = False

  if success:
    print "\nSUCCESS\n"
    sys.exit(0)
  else:
    print "\nFAILED\n"
    print "Please read",
    print "http://www.chromium.org/developers/adding-3rd-party-libraries"
    print "for more info how to handle the failure."
    print
    print "Please respect OWNERS of checklicenses.py. Changes violating"
    print "this requirement may be reverted."

    sys.exit(1)


if '__main__' == __name__:
  default_root = os.path.abspath(
      os.path.join(os.path.dirname(__file__), '..', '..'))
  option_parser = optparse.OptionParser()
  option_parser.add_option('--root', default=default_root,
                           dest='base_directory',
                           help='Specifies the repository root. This defaults '
                           'to "../.." relative to the script file, which '
                           'will normally be the repository root.')
  option_parser.add_option('-v', '--verbose', action='store_true',
                           default=False, help='Print debug logging')
  option_parser.add_option('--ignore-suppressions',
                           action='store_true',
                           default=False,
                           help='Ignore path-specific license whitelist.')
  options, args = option_parser.parse_args()
  main(options, args)
