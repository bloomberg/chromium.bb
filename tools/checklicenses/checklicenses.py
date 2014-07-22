#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Makes sure that all files contain proper licensing information."""


import json
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
    'Anti-Grain Geometry',
    'Apache (v2.0)',
    'Apache (v2.0) BSD (2 clause)',
    'Apache (v2.0) GPL (v2)',
    'Apple MIT',  # https://fedoraproject.org/wiki/Licensing/Apple_MIT_License
    'APSL (v2)',
    'APSL (v2) BSD (4 clause)',
    'BSD',
    'BSD (2 clause)',
    'BSD (2 clause) ISC',
    'BSD (2 clause) MIT/X11 (BSD like)',
    'BSD (3 clause)',
    'BSD (3 clause) GPL (v2)',
    'BSD (3 clause) ISC',
    'BSD (3 clause) LGPL (v2 or later)',
    'BSD (3 clause) LGPL (v2.1 or later)',
    'BSD (3 clause) MIT/X11 (BSD like)',
    'BSD (4 clause)',
    'BSD-like',

    # TODO(phajdan.jr): Make licensecheck not print BSD-like twice.
    'BSD-like MIT/X11 (BSD like)',

    'BSL (v1.0)',
    'FreeType (BSD like)',
    'FreeType (BSD like) with patent clause',
    'GPL (v2) LGPL (v2.1 or later)',
    'GPL (v2 or later) with Bison parser exception',
    'GPL (v2 or later) with libtool exception',
    'GPL (v3 or later) with Bison parser exception',
    'GPL with Bison parser exception',
    'Independent JPEG Group License',
    'ISC',
    'LGPL (unversioned/unknown version)',
    'LGPL (v2)',
    'LGPL (v2 or later)',
    'LGPL (v2.1)',
    'LGPL (v2.1 or later)',
    'LGPL (v3 or later)',
    'MIT/X11 (BSD like)',
    'MIT/X11 (BSD like) LGPL (v2.1 or later)',
    'MPL (v1.0) LGPL (v2 or later)',
    'MPL (v1.1)',
    'MPL (v1.1) BSD (3 clause) GPL (v2) LGPL (v2.1 or later)',
    'MPL (v1.1) BSD (3 clause) LGPL (v2.1 or later)',
    'MPL (v1.1) BSD-like',
    'MPL (v1.1) BSD-like GPL (unversioned/unknown version)',
    'MPL (v1.1) BSD-like GPL (v2) LGPL (v2.1 or later)',
    'MPL (v1.1) GPL (v2)',
    'MPL (v1.1) GPL (v2) LGPL (v2 or later)',
    'MPL (v1.1) GPL (v2) LGPL (v2.1 or later)',
    'MPL (v1.1) GPL (unversioned/unknown version)',
    'MPL (v1.1) LGPL (v2 or later)',
    'MPL (v1.1) LGPL (v2.1 or later)',
    'MPL (v2.0)',
    'Ms-PL',
    'Public domain',
    'Public domain BSD',
    'Public domain BSD (3 clause)',
    'Public domain BSD-like',
    'Public domain LGPL (v2.1 or later)',
    'libpng',
    'zlib/libpng',
    'SGI Free Software License B',
    'University of Illinois/NCSA Open Source License (BSD like)',
    ('University of Illinois/NCSA Open Source License (BSD like) '
     'MIT/X11 (BSD like)'),
]


PATH_SPECIFIC_WHITELISTED_LICENSES = {
    'base/third_party/icu': [  # http://crbug.com/98087
        'UNKNOWN',
    ],

    # http://code.google.com/p/google-breakpad/issues/detail?id=450
    'breakpad/src': [
        'UNKNOWN',
    ],

    'chrome/common/extensions/docs/examples': [  # http://crbug.com/98092
        'UNKNOWN',
    ],
    # This contains files copied from elsewhere from the tree. Since the copied
    # directories might have suppressions below (like simplejson), whitelist the
    # whole directory. This is also not shipped code.
    'chrome/common/extensions/docs/server2/third_party': [
        'UNKNOWN',
    ],
    'courgette/third_party/bsdiff_create.cc': [  # http://crbug.com/98095
        'UNKNOWN',
    ],
    'native_client': [  # http://crbug.com/98099
        'UNKNOWN',
    ],
    'native_client/toolchain': [
        'BSD GPL (v2 or later)',
        'BSD (2 clause) GPL (v2 or later)',
        'BSD (3 clause) GPL (v2 or later)',
        'BSL (v1.0) GPL',
        'BSL (v1.0) GPL (v3.1)',
        'GPL',
        'GPL (unversioned/unknown version)',
        'GPL (v2)',
        'GPL (v2 or later)',
        'GPL (v3.1)',
        'GPL (v3 or later)',
    ],
    'third_party/WebKit': [
        'UNKNOWN',
    ],

    # http://code.google.com/p/angleproject/issues/detail?id=217
    'third_party/angle': [
        'UNKNOWN',
    ],

    # http://crbug.com/222828
    # http://bugs.python.org/issue17514
    'third_party/chromite/third_party/argparse.py': [
        'UNKNOWN',
    ],

    # http://crbug.com/326117
    # https://bitbucket.org/chrisatlee/poster/issue/21
    'third_party/chromite/third_party/poster': [
        'UNKNOWN',
    ],

    # http://crbug.com/333508
    'third_party/clang_format/script': [
        'UNKNOWN',
    ],

    # http://crbug.com/333508
    'buildtools/clang_format/script': [
        'UNKNOWN',
    ],

    # https://mail.python.org/pipermail/cython-devel/2014-July/004062.html
    'third_party/cython': [
        'UNKNOWN',
    ],

    'third_party/devscripts': [
        'GPL (v2 or later)',
    ],
    'third_party/expat/files/lib': [  # http://crbug.com/98121
        'UNKNOWN',
    ],
    'third_party/ffmpeg': [
        'GPL',
        'GPL (v2)',
        'GPL (v2 or later)',
        'UNKNOWN',  # http://crbug.com/98123
    ],
    'third_party/fontconfig': [
        # https://bugs.freedesktop.org/show_bug.cgi?id=73401
        'UNKNOWN',
    ],
    'third_party/freetype2': [ # http://crbug.com/177319
        'UNKNOWN',
    ],
    'third_party/hunspell': [  # http://crbug.com/98134
        'UNKNOWN',
    ],
    'third_party/iccjpeg': [  # http://crbug.com/98137
        'UNKNOWN',
    ],
    'third_party/icu': [  # http://crbug.com/98301
        'UNKNOWN',
    ],
    'third_party/lcov': [  # http://crbug.com/98304
        'UNKNOWN',
    ],
    'third_party/lcov/contrib/galaxy/genflat.pl': [
        'GPL (v2 or later)',
    ],
    'third_party/libc++/trunk/include/support/solaris': [
        # http://llvm.org/bugs/show_bug.cgi?id=18291
        'UNKNOWN',
    ],
    'third_party/libc++/trunk/src/support/solaris/xlocale.c': [
        # http://llvm.org/bugs/show_bug.cgi?id=18291
        'UNKNOWN',
    ],
    'third_party/libc++/trunk/test': [
        # http://llvm.org/bugs/show_bug.cgi?id=18291
        'UNKNOWN',
    ],
    'third_party/libevent': [  # http://crbug.com/98309
        'UNKNOWN',
    ],
    'third_party/libjingle/source/talk': [  # http://crbug.com/98310
        'UNKNOWN',
    ],
    'third_party/libjpeg_turbo': [  # http://crbug.com/98314
        'UNKNOWN',
    ],
    'third_party/libpng': [  # http://crbug.com/98318
        'UNKNOWN',
    ],

    # The following files lack license headers, but are trivial.
    'third_party/libusb/src/libusb/os/poll_posix.h': [
        'UNKNOWN',
    ],

    'third_party/libvpx/source': [  # http://crbug.com/98319
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
    'third_party/mesa/src': [
        'GPL (v2)',
        'GPL (v3 or later)',
        'MIT/X11 (BSD like) GPL (v3 or later) with Bison parser exception',
        'UNKNOWN',  # http://crbug.com/98450
    ],
    'third_party/modp_b64': [
        'UNKNOWN',
    ],
    'third_party/openmax_dl/dl' : [
        'Khronos Group',
    ],
    'third_party/openssl': [  # http://crbug.com/98451
        'UNKNOWN',
    ],
    'third_party/boringssl': [
        # There are some files in BoringSSL which came from OpenSSL and have no
        # license in them. We don't wish to add the license header ourselves
        # thus we don't expect to pass license checks.
        'UNKNOWN',
    ],
    'third_party/ots/tools/ttf-checksum.py': [  # http://code.google.com/p/ots/issues/detail?id=2
        'UNKNOWN',
    ],
    'third_party/molokocacao': [  # http://crbug.com/98453
        'UNKNOWN',
    ],
    'third_party/npapi/npspy': [
        'UNKNOWN',
    ],
    'third_party/ocmock/OCMock': [  # http://crbug.com/98454
        'UNKNOWN',
    ],
    'third_party/ply/__init__.py': [
        'UNKNOWN',
    ],
    'third_party/protobuf': [  # http://crbug.com/98455
        'UNKNOWN',
    ],

    # http://crbug.com/222831
    # https://bitbucket.org/eliben/pyelftools/issue/12
    'third_party/pyelftools': [
        'UNKNOWN',
    ],

    'third_party/scons-2.0.1/engine/SCons': [  # http://crbug.com/98462
        'UNKNOWN',
    ],
    'third_party/simplejson': [
        'UNKNOWN',
    ],
    'third_party/skia': [  # http://crbug.com/98463
        'UNKNOWN',
    ],
    'third_party/snappy/src': [  # http://crbug.com/98464
        'UNKNOWN',
    ],
    'third_party/smhasher/src': [  # http://crbug.com/98465
        'UNKNOWN',
    ],
    'third_party/speech-dispatcher/libspeechd.h': [
        'GPL (v2 or later)',
    ],
    'third_party/sqlite': [
        'UNKNOWN',
    ],

    # http://crbug.com/334668
    # MIT license.
    'tools/swarming_client/third_party/httplib2': [
        'UNKNOWN',
    ],

    # http://crbug.com/334668
    # Apache v2.0.
    'tools/swarming_client/third_party/oauth2client': [
        'UNKNOWN',
    ],

    # https://github.com/kennethreitz/requests/issues/1610
    'tools/swarming_client/third_party/requests': [
        'UNKNOWN',
    ],

    'third_party/swig/Lib/linkruntime.c': [  # http://crbug.com/98585
        'UNKNOWN',
    ],
    'third_party/talloc': [
        'GPL (v3 or later)',
        'UNKNOWN',  # http://crbug.com/98588
    ],
    'third_party/tcmalloc': [
        'UNKNOWN',  # http://crbug.com/98589
    ],
    'third_party/tlslite': [
        'UNKNOWN',
    ],
    'third_party/webdriver': [  # http://crbug.com/98590
        'UNKNOWN',
    ],

    # https://github.com/html5lib/html5lib-python/issues/125
    # https://github.com/KhronosGroup/WebGL/issues/435
    'third_party/webgl/src': [
        'UNKNOWN',
    ],

    'third_party/webrtc': [  # http://crbug.com/98592
        'UNKNOWN',
    ],
    'third_party/xdg-utils': [  # http://crbug.com/98593
        'UNKNOWN',
    ],
    'third_party/yasm/source': [  # http://crbug.com/98594
        'UNKNOWN',
    ],
    'third_party/zlib/contrib/minizip': [
        'UNKNOWN',
    ],
    'third_party/zlib/trees.h': [
        'UNKNOWN',
    ],
    'tools/emacs': [  # http://crbug.com/98595
        'UNKNOWN',
    ],
    'tools/gyp/test': [
        'UNKNOWN',
    ],
    'tools/python/google/__init__.py': [
        'UNKNOWN',
    ],
    'tools/stats_viewer/Properties/AssemblyInfo.cs': [
        'UNKNOWN',
    ],
    'tools/symsrc/pefile.py': [
        'UNKNOWN',
    ],
    'tools/telemetry/third_party/pyserial': [
        # https://sourceforge.net/p/pyserial/feature-requests/35/
        'UNKNOWN',
    ],
    'v8/test/cctest': [  # http://crbug.com/98597
        'UNKNOWN',
    ],
    'v8/src/third_party/kernel/tools/perf/util/jitdump.h': [  # http://crbug.com/391716
        'UNKNOWN',
    ],
}


def check_licenses(options, args):
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
    return 1

  print "Using base directory:", options.base_directory
  print "Checking:", start_dir
  print

  licensecheck_path = os.path.abspath(os.path.join(options.base_directory,
                                                   'third_party',
                                                   'devscripts',
                                                   'licensecheck.pl'))

  licensecheck = subprocess.Popen([licensecheck_path,
                                   '-l', '100',
                                   '-r', start_dir],
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
    return 1

  used_suppressions = set()
  errors = []

  for line in stdout.splitlines():
    filename, license = line.split(':', 1)
    filename = os.path.relpath(filename.strip(), options.base_directory)

    # All files in the build output directory are generated one way or another.
    # There's no need to check them.
    if filename.startswith('out/'):
      continue

    # For now we're just interested in the license.
    license = license.replace('*No copyright*', '').strip()

    # Skip generated files.
    if 'GENERATED FILE' in license:
      continue

    if license in WHITELISTED_LICENSES:
      continue

    if not options.ignore_suppressions:
      matched_prefixes = [
          prefix for prefix in PATH_SPECIFIC_WHITELISTED_LICENSES
          if filename.startswith(prefix) and
          license in PATH_SPECIFIC_WHITELISTED_LICENSES[prefix]]
      if matched_prefixes:
        used_suppressions.update(set(matched_prefixes))
        continue

    errors.append({'filename': filename, 'license': license})

  if options.json:
    with open(options.json, 'w') as f:
      json.dump(errors, f)

  if errors:
    for error in errors:
      print "'%s' has non-whitelisted license '%s'" % (
          error['filename'], error['license'])
    print "\nFAILED\n"
    print "Please read",
    print "http://www.chromium.org/developers/adding-3rd-party-libraries"
    print "for more info how to handle the failure."
    print
    print "Please respect OWNERS of checklicenses.py. Changes violating"
    print "this requirement may be reverted."

    # Do not print unused suppressions so that above message is clearly
    # visible and gets proper attention. Too much unrelated output
    # would be distracting and make the important points easier to miss.

    return 1

  print "\nSUCCESS\n"

  if not len(args):
    unused_suppressions = set(
        PATH_SPECIFIC_WHITELISTED_LICENSES.iterkeys()).difference(
            used_suppressions)
    if unused_suppressions:
      print "\nNOTE: unused suppressions detected:\n"
      print '\n'.join(unused_suppressions)

  return 0


def main():
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
  option_parser.add_option('--json', help='Path to JSON output file')
  options, args = option_parser.parse_args()
  return check_licenses(options, args)


if '__main__' == __name__:
  sys.exit(main())
