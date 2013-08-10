#!/usr/bin/python
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# FIXME(merlin): remove this after fixing the current code.
# pylint: disable-msg=W0621

"""Script that attempts to generate an HTML file containing license
information and homepage links for all installed packages.

WARNING: this script in its current form is not finished or considered
production quality/code style compliant. This is an intermediate checkin
to allow for incremental cleanups and improvements that will make it
production quality.

Usage:
For this script to work, you must have built the architecture
this is being run against, _after_ you've last run repo sync.
Otherwise, it will query newer source code and then fail to work on packages
that are out of date in your build.

Recommended build:
  cros_sdk
  export board=x86-alex
  sudo rm -rf /build/$board
  cd ~/trunk/src/scripts
  ./setup_board --board=$board
  ./build_packages --board=$board --nowithautotest --nowithtest --nowithdev
  cd ~/trunk/chromite/scripts/license-generation
  ./licenses.py [--debug] $board out.html 2>&1 | tee output.sav

For debugging during development, you can get a faster run of just one package
with:
  ./licenses.py --testpkg "dev-libs/libatomic_ops-7.2d" $board out.html

The output file is meant to update
http://src.chromium.org/viewvc/chrome/trunk/src/chrome/browser/resources/ +
  chromeos/about_os_credits.html?view=log
(gclient config svn://svn.chromium.org/chrome/trunk/src)
For an example CL, see https://codereview.chromium.org/13496002/

It is recommended that you use a fancy differ like 'meld' to review license
diffs. GNU diff will show too much irrelevant noise and not resync properly.

UPDATE: gcl will probably fail now, because the file is too big. Before it
gets moved somewhere else, you should just use svn diff and svn commit.

Recommended way to diff the html, go outside of the cros_sdk chroot:
grep -E -A5 '(class="title|class="homepage|Provided Stock|Source license)' \
    out.html  > /tmp/new
grep -E -A5 '(class="title|class="homepage|Provided Stock|Source license)' \
    out-sav-new3.html  > /tmp/old
meld /tmp/old /tmp/new (or your favourite fancy diff program)

If you don't get this in before the freeze window, it'll need to be merged into
the branch being released, which is done by adding a Merge-Requested label to
Iteration-xx in the tracking bug.
Once it's been updated to "Merge-Approved" by a TPM, please merge into the
required release branch. You can ask karen@ for merge approve help.
Example: http://crbug.com/221281
"""

import cgi
import codecs
import getopt
import logging
import os
import portage
import subprocess
import sys

EQUERY_BASE = '/usr/local/bin/equery-%s'

STOCK_LICENSE_DIRS = [
    os.path.expanduser('~/trunk/src/third_party/portage/licenses'),
    os.path.expanduser('~/trunk/src/third_party/portage-stable/licenses'),
    os.path.abspath(os.path.join(os.path.dirname(__file__), 'licenses')),
]

# Virtual packages don't need to have a license and often don't, so we skip them
# chromeos-base contains google platform packages that are covered by the
# general license at top of tree, so we skip those too.
SKIPPED_CATEGORIES = [
    'chromeos-base',  # TODO: this shouldn't be excluded ?
    'virtual',
]

SKIPPED_PACKAGES = [
    # Fix these packages by adding a real license in the code.
    # You should not skip packages just because the license scraping doesn't
    # work. Stick those special cases into PACKAGE_LICENSES.
    # Packages should only be here because they are sub/split packages already
    # covered by the license of the main package.

    # These are Chrome-OS-specific packages, copyright BSD-Google
    'sys-kernel/chromeos-kernel',  # already manually credit Linux

    # These have been split across several packages, so we skip listing the
    # individual components (and just list the main package instead).
    'app-editors/vim-core',
    'x11-apps/mesa-progs',

    # Portage metapackage.
    'x11-base/xorg-drivers',

    # These are covered by app-i18n/ibus-mozc (BSD, copyright Google).
    'app-i18n/ibus-mozc-chewing',
    'app-i18n/ibus-mozc-hangul',
    'app-i18n/ibus-mozc-pinyin',

    # Those have License: Proprietary in the ebuild.
    'app-i18n/GoogleChineseInput-cangjie',
    'app-i18n/GoogleChineseInput-pinyin',
    'app-i18n/GoogleChineseInput-wubi',
    'app-i18n/GoogleChineseInput-zhuyin',
    'app-i18n/GoogleKoreanInput',

    # These are all X.org sub-packages; shouldn't be any need to list them
    # individually.
    'media-fonts/encodings',
    'x11-apps/iceauth',
    'x11-apps/intel-gpu-tools',
    'x11-apps/mkfontdir',
    'x11-apps/rgb',
    'x11-apps/setxkbmap',
    'x11-apps/xauth',
    'x11-apps/xcursorgen',
    'x11-apps/xdpyinfo',
    'x11-apps/xdriinfo',
    'x11-apps/xev',
    'x11-apps/xgamma',
    'x11-apps/xhost',
    'x11-apps/xinit',
    'x11-apps/xinput',
    'x11-apps/xkbcomp',
    'x11-apps/xlsatoms',
    'x11-apps/xlsclients',
    'x11-apps/xmodmap',
    'x11-apps/xprop',
    'x11-apps/xrandr',
    'x11-apps/xrdb',
    'x11-apps/xset',
    'x11-apps/xset-mini',
    'x11-apps/xwininfo',
    'x11-base/xorg-server',
    'x11-drivers/xf86-input-evdev',
    'x11-drivers/xf86-input-keyboard',
    'x11-drivers/xf86-input-mouse',
    'x11-drivers/xf86-input-synaptics',
    'x11-drivers/xf86-video-intel',
    'x11-drivers/xf86-video-vesa',
    'x11-drivers/xf86-video-vmware',
    'x11-libs/libICE',
    'x11-libs/libSM',
    'x11-libs/libX11',
    'x11-libs/libXScrnSaver',
    'x11-libs/libXau',
    'x11-libs/libXcomposite',
    'x11-libs/libXcursor',
    'x11-libs/libXdamage',
    'x11-libs/libXdmcp',
    'x11-libs/libXext',
    'x11-libs/libXfixes',
    'x11-libs/libXfont',
    'x11-libs/libXfontcache',
    'x11-libs/libXft',
    'x11-libs/libXi',
    'x11-libs/libXinerama',
    'x11-libs/libXmu',
    'x11-libs/libXp',
    'x11-libs/libXrandr',
    'x11-libs/libXrender',
    'x11-libs/libXres',
    'x11-libs/libXt',
    'x11-libs/libXtst',
    'x11-libs/libXv',
    'x11-libs/libXvMC',
    'x11-libs/libXxf86vm',
    'x11-libs/libdrm',
    'x11-libs/libfontenc',
    'x11-libs/libpciaccess',
    'x11-libs/libxkbfile',
    'x11-libs/libxkbui',
    'x11-libs/pixman',
    'x11-libs/xtrans',
    'x11-misc/util-macros',
    'x11-misc/xbitmaps',
    'x11-proto/bigreqsproto',
    'x11-proto/compositeproto',
    'x11-proto/damageproto',
    'x11-proto/dri2proto',
    'x11-proto/fixesproto',
    'x11-proto/fontcacheproto',
    'x11-proto/fontsproto',
    'x11-proto/inputproto',
    'x11-proto/kbproto',
    'x11-proto/printproto',
    'x11-proto/randrproto',
    'x11-proto/recordproto',
    'x11-proto/renderproto',
    'x11-proto/resourceproto',
    'x11-proto/scrnsaverproto',
    'x11-proto/trapproto',
    'x11-proto/videoproto',
    'x11-proto/xcmiscproto',
    'x11-proto/xextproto',
    'x11-proto/xf86bigfontproto',
    'x11-proto/xf86dgaproto',
    'x11-proto/xf86driproto',
    'x11-proto/xf86rushproto',
    'x11-proto/xf86vidmodeproto',
    'x11-proto/xineramaproto',
    'x11-proto/xproto',
]

# TODO(merlin): replace matching with regex matching to simplify this
# Matching is done in lowercase, you MUST give lowercase names.
LICENSE_FILENAMES = [
    'copying',
    'copyright',
    'ipa_font_license_agreement_v1.0.txt',  # used by ja-ipafonts
    'licence',        # used by openssh
    'license',
    'license.txt',    # used by hdparm, used by NumPy, glew
    'licensing.txt',  # used by libatomic_ops
]

# These are _temporary_ license mappings for packages that do not have a valid
# stock license, or LICENSE file we can use.
# Once this script runs earlier (during the package build process), it will
# block new source without a LICENSE file if the ebuild contains a license
# that requires copyright assignment (BSD and friends).
# At that point, new packages will get fixed to include LICENSE instead of
# adding workaround mappings like those below.
# We should also fix the packages listed below so that the hardcoded
# mappings can be obsoleted (i.e. FIXME for this entire list).
PACKAGE_LICENSES = {
    # One off licenses. Should we check in a custom LICENSE file in upstream?
    'dev-python/netifaces': ['netiface'],
    'net-dialup/ppp': ['ppp-2.4.4'],
    'sys-libs/ncurses': ['ncurses'],

    # BSD and MIT license authorship mapping.
    # Ideally we should have a custom LICENSE file in the upstream source.
    # TODO: BSD-2: bsdiff is missing a license file, add one upstream.
    'dev-util/bsdiff': ['BSD-bsdiff'],
    # TODO: libevent is missing a license file, add one upstream.
    'dev-libs/libevent': ['BSD-libevent'],
    # TODO: dhcpcd is missing a license file, (c) in README. Add one upstream.
    'net-misc/dhcpcd': ['BSD-dhcpcd'],
    # TODO: iputils is missing a license file, add one upstream.
    'net-misc/iputils': ['BSD-iputils'],
    # TODO: c-ares is missing a license file, add one upstream.
    'net-dns/c-ares': ['MIT-MIT'],

    # TODO: We should just check in a LICENSE file in all of these:
    'app-i18n/input-tools': ['BSD-Google'],
    'app-i18n/nacl-mozc': ['BSD-Google'],
    'app-i18n/ibus-mozc': ['BSD-Google'],
    'media-plugins/o3d': ['BSD-Google'],
    'dev-python/unittest2': ['BSD-Google'],

    # Fix ebuild multi license definitions when they define licenses that do
    # not apply to us because we don't use the resulting binaries.

    # Mesa ebuild says MIT and omits LGPL-3 and SGI-B-2.0 mentioned in the
    # docs directory? Either way, I had to create a text license file like so:
    # mesa-9.1-r9/work/Mesa-9.1/docs$ lynx --dump license.html -nolist > license
    'media-libs/mesa': ['MIT-Mesa', 'LGPL-3', 'SGI-B-2.0'],

    # TODO: Ebuild seems to wrongfully say BSD + public-domain.
    # I scanned the unpacked source with licensecheck and didn't find any BSD.
    # FIXME: Do a second review and fix upstream gentoo package
    'sys-libs/timezone-data': ['public-domain'],

    # Ebuild only says 'LGPL-2.1', but source disagrees. I'll include 'as-is'
    # to force reading files from the source (which states some parts are as-is)
    # FIXME? Update ebuild license to match xz-4.999.9beta/COPYING?
    'app-arch/xz-utils': ['public-domain', 'as-is', 'LGPL-2.1', 'GPL-2'],

    # These packages are not in Alex, check and remove later (might be used in
    # other platforms).
    # 'media-libs/freeimage': ['GPL-2'],
    # 'sys-libs/talloc': ['LGPL-3'],  # ebuild incorrectly says GPL-3
    # 'media-libs/jpeg': ['jpeg'],
    # 'app-editors/gentoo-editor': ['MIT-gentoo-editor'],
    #
    # 'media-fonts/font-util': ['font-util'],  # COPYING file from git repo
    # 'net-wireless/iwl1000-ucode': ['Intel-iwl1000'],
    # 'sys-process/vixie-cron': ['vixie-cron'],
}

# Any license listed list here found in the ebuild will make the code look for
# license files inside the package source code in order to get copyright
# attribution from them.
COPYRIGHT_ATTRIBUTION_LICENSES = [
    'BSD',    # requires distribution of copyright notice
    'BSD-2',  # so does BSD-2 http://opensource.org/licenses/BSD-2-Clause
    'BSD-3',  # and BSD-3? http://opensource.org/licenses/BSD-3-Clause
    'BSD-4',  # and 4?
    'BSD-with-attribution',
    'MIT',
    'MIT-with-advertising',
    'Old-MIT',
]

# The following licenses are not invalid or to show as a less helpful stock
# license, but it's better to look in the source code for a more specific
# license if there is one, but not an error if no better one is found.
# Note that you don't want to set just anything here since any license here
# will be included once in stock form and a second time in custom form if
# found (there is no good way to know that a license we found on disk is the
# better version of the stock version, so we show both).
LOOK_IN_SOURCE_LICENSES = [
    'as-is',  # The stock license is very vague, source always has more details.
    'PSF-2',  # The custom license in python is more complete than the template.

    # As far as I know, we have no requirement to do copyright attribution for
    # these licenses, but the license included in the code has slightly better
    # information than the stock Gentoo one (including copyright attribution).
    'BZIP2',     # Single use license, do copyright attribution.
    'OFL',       # Almost single use license, do copyright attribution.
    'OFL-1.1',   # Almost single use license, do copyright attribution.
    'UoI-NCSA',  # Only used by NSCA, might as well show their custom copyright.
]

PACKAGE_HOMEPAGES = {
    # Example:
    # 'x11-proto/glproto': ['http://www.x.org/'],
}

# These are tokens found in LICENSE= in an ebuild that aren't licenses we
# can actually read from disk.
# You should not use this to blacklist real licenses.
LICENCES_IGNORE = [
    ')',              # Ignore OR tokens from LICENSE="|| ( LGPL-2.1 MPL-1.1 )"
    '(',
    '||',
    'International',  # Workaround for LICENSE="Marvell International Ltd."
    'Ltd.',           # Get Marvell & ignore the other 2 tokens (FIXME upstream)
]

TMPL = 'about_credits.tmpl'
ENTRY_TMPL = 'about_credits_entry.tmpl'
SHARED_LICENSE_TMPL = 'about_credits_shared_license_entry.tmpl'


class PackageLicenseError(Exception):
  """Thrown if something fails while getting license information for a package.
  This will cause the processing to error in the end.
  """


class PackageSkipped(Exception):
  """Non error to exclude packages from license processing."""


class PackageInfo(object):
  """Package info containers, mostly for storing licenses."""

  def __init__(self):

    self.revision = None

    # Array of scanned license texts.
    self.license_text_scanned = []

    self.category = None
    self.name = None
    self.version = None

    # Array of license names retrieved from ebuild or override in this code.
    self.ebuild_license_names = []
    self.description = None
    self.homepages = []
    # This contains stock licenses names we can read from Gentoo.
    self.stock_license_names = []

    # We set this if the ebuild has a BSD/MIT like license that requires
    # scanning for a LICENSE file in the source code, or a static mapping
    # in PACKAGE_LICENSES. Not finding one once this is set, is fatal.
    self.need_copyright_attribution = False
    # This flag just says we'd like to include licenses from the source, but
    # not finding any is not fatal.
    self.scan_source_for_licenses = False

  @property
  def fullnamerev(self):
    s = '%s-%s' % (self.fullname, self.version)
    if self.revision:
      s += '-r%s' % self.revision
    return s

  @property
  def fullname(self):
    return '%s/%s' % (self.category, self.name)

  @staticmethod
  def _RunEbuildPhases(path, *phases, **kwargs):
    """Run a list of ebuild phases on an ebuild.

    Args:
      path: /mnt/host/source/src/
                 third_party/portage-stable/net-misc/rsync/rsync-3.0.8.ebuild
      *phases: list of phases like ['clean', 'fetch'] or ['unpack'].
      **kwargs: dict passed to ebuild
    """

    #logging.debug('ebuild-%s | %s | %s', board, path, str(list(phases)))
    subprocess.check_call(
        ['ebuild-%s' % board, path] + list(phases), **kwargs)

  def _ExtractLicenses(self):
    """Scrounge for text licenses in the source of package we'll unpack.

    This is only called if we couldn't get usable licenses from the ebuild,
    or one of them is BSD/MIT like which forces us to look for a file with
    copyright attribution in the source code itself.
    It'll scan the unpacked source code for what looks like license files
    as defined in LICENSE_FILENAMES.

    Raises:
      AssertionError: on runtime errors
      PackageLicenseError: couldn't find copyright attribution file.
    """

    path = self._GetEbuildPath(board, self.fullnamerev)
    self._RunEbuildPhases(
        path, 'clean', 'fetch',
        stdout=open('/dev/null', 'wb'),
        stderr=subprocess.STDOUT)
    self._RunEbuildPhases(path, 'unpack')

    p = subprocess.Popen(['portageq-%s' % board, 'envvar',
                          'PORTAGE_TMPDIR'], stdout=subprocess.PIPE)
    tmpdir = p.communicate()[0].strip()
    ret = p.wait()
    if ret != 0:
      raise AssertionError('exit code was not 0: got %s' % ret)

    # tmpdir gets something like /build/daisy/tmp/
    workdir = os.path.join(tmpdir, 'portage', self.fullnamerev, 'work')

    # You may wonder how deep should we go?
    # In case of packages with sub-packages, it could be deep.
    # Let's just be safe and get everything we can find.
    # In the case of libatomic_ops, it's actually required to look deep
    # to find the MIT license:
    # dev-libs/libatomic_ops-7.2d/work/gc-7.2/libatomic_ops/doc/LICENSING.txt
    args = ['find', workdir, '-type', 'f']
    p = subprocess.Popen(args, stdout=subprocess.PIPE)
    files = p.communicate()[0].splitlines()
    ret = p.wait()
    if ret != 0:
      raise AssertionError('exit code was not 0: got %s' % ret)

    files = [x[len(workdir):].lstrip('/') for x in files]
    license_files = []
    for name in files:
      if os.path.basename(name).lower() in LICENSE_FILENAMES:
        license_files.append(name)

    if not license_files:
      if self.need_copyright_attribution:
        logging.error("%s used license with copyright attribution, but "
                      "couldn't find license file in %s",
                      self.fullnamerev, workdir)
        raise PackageLicenseError()
      else:
        # We can get called for a license like as-is where it's preferable
        # to find a better one in the source, but not fatal if we didn't.
        logging.info("Was not able to find a better license for %s "
                     "in %s to replace the more generic one from ebuild",
                     self.fullnamerev, workdir)

    # Examples of multiple license matches:
    # dev-lang/swig-2.0.4-r1: swig-2.0.4/COPYRIGHT swig-2.0.4/LICENSE
    # dev-libs/glib-2.32.4-r1: glib-2.32.4/COPYING pkg-config-0.26/COPYING
    # dev-libs/libnl-3.2.14: libnl-doc-3.2.14/COPYING libnl-3.2.14/COPYING
    # dev-libs/libpcre-8.30-r2: pcre-8.30/LICENCE pcre-8.30/COPYING
    # dev-libs/libusb-0.1.12-r6: libusb-0.1.12/COPYING libusb-0.1.12/LICENSE
    # dev-libs/pyzy-0.1.0-r1: db/COPYING pyzy-0.1.0/COPYING
    # net-misc/strongswan-5.0.2-r4: strongswan-5.0.2/COPYING
    #                               strongswan-5.0.2/LICENSE
    # sys-process/procps-3.2.8_p11: debian/copyright procps-3.2.8/COPYING
    logging.info('License(s) for %s: %s', self.fullnamerev,
                 ' '.join(license_files))
    for license_file in sorted(license_files):
      # Joy and pink ponies. Some license_files are encoded as latin1 while
      # others are utf-8 and of course you can't know but only guess.
      license_path = os.path.join(workdir, license_file)
      try:
        license_txt = codecs.open(license_path, encoding="utf-8").read()
        logging.info("Adding license %s: (guessed UTF-8)", license_path)
      except UnicodeDecodeError:
        license_txt = codecs.open(license_path, encoding="latin1").read()
        logging.info("Adding license %s: (guessed latin1)", license_path)

      self.license_text_scanned += [
          "Scanned Source license %s:\n\n%s" % (license_file, license_txt)]

    # We used to clean up here, but there have been many instances where
    # looking at unpacked source to see where the licenses were, was useful
    # so let's disable this for now
    #self._RunEbuildPhases(path, 'clean')

  @staticmethod
  def _GetEbuildPath(board, name):
    """Create a pathname where to find the ebuild of a given package for board.

    Args:
      board: eg x86-alex
      name: eg net-misc/wget-1.12

    Returns:
      /mnt/host/source/src/third_party/portage-stable/net-misc/wget/
                                                               wget-1.12.ebuild

    Raises:
      AssertionError: when equery did not return a valid path.
    """
    p = subprocess.Popen(
        ['equery-%s' % board, 'which', name], stdout=subprocess.PIPE)
    stdout = p.communicate()[0]
    p.wait()
    path = stdout.strip()
    logging.debug("equery-%s which %s", board, name)
    logging.debug("  -> %s", path)
    if not path:
      raise AssertionError('GetEbuildPath for %s failed.\n'
                           'Is your tree clean? Delete /build/%s and rebuild' %
                           (name, board))
    return path

  def _GetPackageInfo(self, fullnamewithrev):
    """Populate PackageInfo with package license, homepage and description.

    Some packages have static license mappings applied to them.

    self.ebuild_license_names will not be filled if the package is skipped
    or if there was an issue getting data from the ebuild.
    self.stock_license_names will only get the licenses that we can paste
    as stock licenses.
    scan_source_for_licenses will be set if we should unpack the source to look
    for licenses
    if need_copyright_attribution is also set, not finding a license in the
    source is fatal (PackageLicenseError will get raised).

    Args:
      fullnamewithrev: e.g. dev-libs/libatomic_ops-7.2d

    Raises:
      AssertionError: on runtime errors
      PackageSkipped: if in skip list
      PackageLicenseError: in case of error getting license file
    """

    try:
      self.category, self.name, self.version, self.revision = \
          portage.versions.catpkgsplit(fullnamewithrev)
    except TypeError:
      raise AssertionError("portage couldn't find %s, missing version number?" %
                           fullnamewithrev)

    if self.revision is not None:
      self.revision = str(self.revision).lstrip('r')
      if self.revision == '0':
        self.revision = None

    if self.category in SKIPPED_CATEGORIES:
      raise PackageSkipped("%s in SKIPPED_CATEGORIES, skip package" %
                           self.fullname)

    if self.fullname in SKIPPED_PACKAGES:
      raise PackageSkipped("%s in SKIPPED_PACKAGES, skip package" %
                           self.fullname)

    ebuild = self._GetEbuildPath(board, self.fullnamerev)

    if not os.access(ebuild, os.F_OK):
      logging.error("Can't access %s", ebuild)
      raise PackageLicenseError()

    cmd = [
        'portageq',
        'metadata',
        '/build/%s' % board,
        'ebuild',
        self.fullnamerev,
        'HOMEPAGE', 'LICENSE', 'DESCRIPTION',
    ]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    lines = [s.strip() for s in p.stdout.readlines()]
    p.wait()

    if p.returncode != 0:
      raise AssertionError("%s failed" % cmd)

    # Runs:
    # portageq metadata /build/x86-alex ebuild net-misc/wget-1.12-r2 \
    #                                               HOMEPAGE LICENSE DESCRIPTION
    # Returns:
    # http://www.gnu.org/software/wget/
    # GPL-3
    # Network utility to retrieve files from the WWW

    (self.homepages,   self.ebuild_license_names, self.description) = (
     lines[0].split(), lines[1].split(),          lines[2:])

    if self.fullname in PACKAGE_HOMEPAGES:
      self.homepages = PACKAGE_HOMEPAGES[self.fullname]

    # Packages with missing licenses or licenses that need mapping (like
    # BSD/MIT) are hardcoded here:
    if self.fullname in PACKAGE_LICENSES:
      self.ebuild_license_names = PACKAGE_LICENSES[self.fullname]
      logging.info("Static license mapping for %s: %s", self.fullnamerev,
                   ",".join(self.ebuild_license_names))
    else:
      logging.info("Read licenses from ebuild for %s: %s", self.fullnamerev,
                   ",".join(self.ebuild_license_names))

  def GetLicenses(self, fullnamewithrev):
    """Get licenses from the ebuild field and the unpacked source code.

    After populating the package info and stock licenses, this figures
    out whether the package source should be scanned to add licenses found
    there.

    Args:
      fullnamewithrev: e.g. dev-libs/libatomic_ops-7.2d

    Raises:
      AssertionError: on runtime errors
      PackageLicenseError: couldn't find license in ebuild.
    """

    # First populate the package basic information
    self._GetPackageInfo(fullnamewithrev)

    # The ebuild license field can look like:
    # LICENSE="GPL-3 LGPL-3 Apache-2.0" (this means AND, as in all 3)
    # for third_party/portage-stable/app-admin/rsyslog/rsyslog-5.8.11.ebuild
    # LICENSE="|| ( LGPL-2.1 MPL-1.1 )"
    # for third_party/portage-stable/x11-libs/cairo/cairo-1.8.8.ebuild
    # LICENSE="Marvell International Ltd." <- invalid syntax to fix upstream
    # for net-wireless/marvell_sd8787/marvell_sd8787-14.64.2.47-r16.ebuild

    # The parser isn't very smart and only has basic support for the
    # || ( X Y ) OR logic to do the following:
    # In order to save time needlessly unpacking packages and looking or a
    # cleartext license (which is really a crapshoot), if we have a license
    # like BSD that requires looking for copyright attribution, but we can
    # chose another license like GPL, we do that.

    if not self.ebuild_license_names:
      logging.error("%s: no license found in ebuild. FIXME!", self.fullnamerev)
      # In a bind, you could comment this out. I'm making the output fail to
      # get your attention since this error really should be fixed, but if you
      # comment out the next line, the script will try to find a license inside
      # the source.
      raise PackageLicenseError()

    # This is not invalid, but the parser can't deal with it, so if it ever
    # happens, error out to tell the programmer to do something.
    if "||" in self.ebuild_license_names[1:]:
      raise AssertionError("%s: Can't parse || in the middle of a license: %s"
                           % (self.fullnamerev,
                              ' '.join(self.ebuild_license_names)))

    self.stock_license_names = []
    or_licenses_and_one_is_no_attribution = False
    # We do a quick early pass first so that the longer pass below can
    # run accordingly.
    for license_name in [x for x in self.ebuild_license_names
                         if x not in LICENCES_IGNORE]:
      # Here we have an OR case, and one license that we can use stock, so
      # we remember that in order to be able to skip license attributions if
      # any were in the OR.
      if (self.ebuild_license_names[0] == "||" and
          license_name not in COPYRIGHT_ATTRIBUTION_LICENSES):
        or_licenses_and_one_is_no_attribution = True

    for license_name in [x for x in self.ebuild_license_names
                         if x not in LICENCES_IGNORE]:
      # Licenses like BSD or MIT can't be used as is because they do not contain
      # copyright self. They have to be replaced by copyright file given in the
      # source code, or manually mapped by us in PACKAGE_LICENSES
      if license_name in COPYRIGHT_ATTRIBUTION_LICENSES:
        # To limit needless efforts, if a package is BSD or GPL, we ignore BSD
        # and use GPL to avoid scanning the package, but we can only do this if
        # or_licenses_and_one_is_no_attribution has been set above.
        # This ensures that if we have License: || (BSD3 BSD4), we will
        # look in the source.
        if or_licenses_and_one_is_no_attribution:
          logging.info("%s: ignore license %s because ebuild LICENSES had %s",
                       self.fullnamerev, license_name,
                       ' '.join(self.ebuild_license_names))
        else:
          logging.info("%s: can't use %s, will scan source code for copyright",
                       self.fullnamerev, license_name)
          self.need_copyright_attribution = True
          self.scan_source_for_licenses = True
      else:
        self.stock_license_names.append(license_name)
        # We can't display just 2+ because it only contains text that says to
        # read v2 or v3.
        if license_name == 'GPL-2+':
          self.stock_license_names.append('GPL-2')
        if license_name == 'LGPL-2+':
          self.stock_license_names.append('LGPL-2')

      if license_name in LOOK_IN_SOURCE_LICENSES:
        logging.info("%s: Got %s, will try to find better license in source...",
                     self.fullnamerev, license_name)
        self.scan_source_for_licenses = True
    if self.stock_license_names:
      logging.info('%s: using stock license(s) %s',
                   self.fullnamerev, ','.join(self.stock_license_names))

    # If the license(s) could not be found, or one requires copyright
    # attribution, dig in the source code for license files:
    # For instance:
    # Read licenses from ebuild for net-dialup/ppp-2.4.5-r3: BSD,GPL-2
    # We need get the substitution file for BSD and add it to GPL.
    if not self.stock_license_names or self.scan_source_for_licenses:
      self._ExtractLicenses()

    if not self.stock_license_names and not self.license_text_scanned:
      logging.error("""
  %s: unable to find usable license.
  Typically this will happen because the ebuild says it's MIT or BSD, but there
  was no license file that this script could find to include along with a
  copyright attribution (required for BSD/MIT).
  Go investigate the unpacked source in /tmp/boardname/tmp/portage/..., and
  find which license to assign. Once you found it, add a static mapping to the
  PACKAGE_LICENSES dict if that license is not in a file, or teach this script
  to find the license file.""",
                    self.fullname)
      raise PackageLicenseError()


class Licensing(object):
  """Do the actual work of extracting licensing info and outputting html."""

  def __init__(self, package_fullnames,
               entry_template_file=ENTRY_TMPL):
    # List of stock licenses referenced in ebuilds. Used to print a report.
    self.stock_licenses = {}

    # This keeps track of whether we have an incomplete license file due to
    # package errors during parsing.
    # Any non empty list at the end shows the list of packages that caused
    # errors.
    self.incomplete_packages = []

    self.package_text = {}
    self.entry_template = codecs.open(entry_template_file, mode='rb',
                                      encoding="utf-8").read()
    self.packages = []
    self._package_fullnames = package_fullnames

  @property
  def sorted_stock_licenses(self):
    return sorted(self.stock_licenses.keys(), key=str.lower)

  def LicensedPackages(self, license_name):
    """Return list of packages using a given license."""
    return self.stock_licenses[license_name]

  def ProcessPackages(self):
    """Iterate through all packages provided and gather their licenses.

    GetLicenses will scrape licenses from the code and/or gather stock license
    names. We gather the list of stock ones for later processing.

    Do not call this after adding virtual packages with AddExtraPkg.
    """
    for package in self._package_fullnames:
      pkg = PackageInfo()
      try:
        pkg.GetLicenses(package)
        self.packages += [pkg]
      except PackageSkipped, e:
        logging.info(e)
      except PackageLicenseError:
        self.incomplete_packages += [pkg.fullnamerev]

  def AddExtraPkg(self, pkg_data):
    """Allow adding pre-created virtual packages.

    GetLicenses will not work on them, so add them after having run
    ProcessPackages.

    Args:
      pkg_data: array of package data as defined below
    """
    pkg = PackageInfo()
    pkg.category = pkg_data[0]
    pkg.name = pkg_data[1]
    pkg.version = pkg_data[2]
    pkg.homepages = pkg_data[3]            # this is a list
    pkg.stock_license_names = pkg_data[4]  # this is also a list
    pkg.ebuild_license_names = pkg_data[4]
    self.packages += [pkg]

  @staticmethod
  def _ReadStockLicense(stock_license_name):
    """Read and return stock license file specified in an ebuild."""

    license_path = None
    for directory in STOCK_LICENSE_DIRS:
      path = '%s/%s' % (directory, stock_license_name)
      if os.access(path, os.F_OK):
        license_path = path
        break
    if license_path:
      # Joy and pink ponies. Some stock licenses are encoded as latin1 while
      # others are utf-8 and of course you can't know but only guess.
      try:
        license_txt = codecs.open(license_path, encoding="utf-8").read()
        logging.info("read stock license %s (UTF-8)", license_path)
      except UnicodeDecodeError:
        license_txt = codecs.open(license_path, encoding="latin1").read()
        logging.info("read stock license %s (latin1)", license_path)
      return license_txt
    else:
      raise AssertionError("stock license %s could not be found in %s"
                           % (stock_license_name,
                              '\n'.join(STOCK_LICENSE_DIRS)))

  @staticmethod
  def EvaluateTemplate(template, env):
    """Expand a template with vars like {{foo}} using a dict of expansions."""
    # TODO switch to stock python templates.
    for key, val in env.iteritems():
      template = template.replace('{{%s}}' % key, val)
    return template

  def _GeneratePackageLicenseText(self, package):
    """Concatenate all licenses related to a package.

    This means a combination of stock gentoo licenses and licenses read from the
    package source tree, if any.

    Args:
      package: PackageInfo object

    Raises:
      AssertionError: on runtime errors
    """

    license_text = []
    for license_text_scanned in package.license_text_scanned:
      license_text.append(license_text_scanned)
      license_text.append('%s\n' % ('-=' * 40))

    license_pointers = []
    for sln in package.stock_license_names:
      license_pointers.append(
          "<li><a href='#%s'>Gentoo Package Stock License %s</a></li>" % (
              sln, sln))
      # Keep track of which stock licenses are used by which packages.
      try:
        self.stock_licenses[sln] += [package.fullnamerev]
      except KeyError:
        self.stock_licenses[sln] = [package.fullnamerev]


    # This should get caught earlier, but one extra check.
    if not (license_text + license_pointers):
      raise AssertionError('Ended up with no license_text')

    env = {
        'name': "%s-%s" % (package.name, package.version),
        'url': package.homepages[0] if package.homepages else '',
        'licenses_txt': cgi.escape('\n'.join(license_text)) or '',
        'licenses_ptr': '\n'.join(license_pointers) or '',
    }
    self.package_text[package] = self.EvaluateTemplate(self.entry_template, env)

  def GenerateHTMLLicenseOutput(self, output_file,
                                output_template=TMPL,
                                stock_license_template=SHARED_LICENSE_TMPL):
    """Generate the combined html license file used in ChromeOS.

    Args:
      output_file: resulting HTML license output.
      output_template: template for the entire HTML file.
      stock_license_template: template for stock license entries.
    """
    sorted_license_txt = []
    self.packages.sort(key=lambda x: (x.name.lower(), x.version, x.revision))
    for pkg in self.packages:
      self._GeneratePackageLicenseText(pkg)
      sorted_license_txt += [self.package_text[pkg]]

    # Now generate the bottom of the page that will contain all the stock
    # licenses and a list of who is pointing to them.
    stock_license_template = codecs.open(stock_license_template, mode='rb',
                                         encoding="utf-8").read()
    # TODO: stock licenses that are only used once should be migrated to the
    # per package license list. No need to factor out a stock license if
    # it's only used once.
    stock_licenses_txt = []
    for license_name in self.sorted_stock_licenses:
      env = {
          'license_name': license_name,
          'license': self._ReadStockLicense(license_name),
          'license_packages': ' '.join(
              licensing.LicensedPackages(license_name)),
      }
      stock_licenses_txt += [self.EvaluateTemplate(stock_license_template, env)]

    file_template = codecs.open(output_template, mode='rb',
                                encoding="utf-8").read()
    env = {
        'entries': '\n'.join(sorted_license_txt),
        'licenses': '\n'.join(stock_licenses_txt),
    }
    out_file = codecs.open(output_file, mode="w", encoding="utf-8")
    out_file.write(self.EvaluateTemplate(file_template, env))


def ListInstalledPackages(board):
  """Return a list of all packages installed for a particular board."""
  # TODO(merlin): davidjames pointed out that this is not the right way to
  # get the package list as it does not apply filters. This should change to
  # ~/trunk/src/scripts/get_package_list which lists fewer packages, but it's
  # not a drop in since equery list gives version numbers while get_package_list
  # does not.
  # get_package_list gives a shorter list, and it'll have to be reviewed to make
  # sure it does not mistakenly remove anything it shouldn't have.
  args = [EQUERY_BASE % board, 'list', '*']
  p = subprocess.Popen(args, stdout=subprocess.PIPE)
  return [s.strip() for s in p.stdout.readlines()]


def usage():
  print >> sys.stderr, (__doc__)
  sys.exit(1)


# TODO make this a main function after fixing those:
# board needs to be passed instead of being global
# licensing too
# so does SKIPPED_PACKAGES
if __name__ == '__main__':
  debug = False
  testpkg = None
  try:
    opts, args = getopt.getopt(sys.argv[1:], "hdt:",
                               ["help", "debug", "testpkg="])
  except getopt.GetoptError:
    usage()
  for opt, arg in opts:
    if opt in ("-h", "--help"):
      usage()
    elif opt in ("-d", "--debug"):
      debug = True
    elif opt in ("-t", "--testpkg"):
      testpkg = arg

  if len(args) != 2:
    usage()
  board, output_file = args

  if debug:
    logging.basicConfig(format='%(levelname)s:%(message)s', level=logging.DEBUG)
  else:
    logging.basicConfig(format='%(levelname)s:%(message)s', level=logging.INFO)

  # We have a hardcoded list of skipped packages for various reasons, but we
  # also exclude any google platform package from needing a license since they
  # are covered by the top license in the tree.
  cmd = "cros_workon info --all --host | grep src/platform/ |"\
      "awk '{print $1}'"
  p = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
  packages = p.communicate()[0].splitlines()
  ret = p.wait()
  if ret != 0:
    raise AssertionError('%s exit code was not 0: got %s' % (cmd, ret))
  SKIPPED_PACKAGES += packages

  # For temporary single package debugging (make sure to include trailing -ver):
  if testpkg:
    logging.info("Will only generate license for %s", testpkg)
    packages = [testpkg]
  else:
    packages = ListInstalledPackages(board)
  # If the caller forgets to set $board, it'll default to beaglebone, and return
  # no packages. Catch this and give a hint that the wrong board was given.
  if not packages:
    raise AssertionError('FATAL: Could not get any packages for board %s' %
                         board)
  logging.debug("Package list to work through:")
  logging.debug('\n'.join(packages))
  logging.debug("Will skip these packages:")
  logging.debug('\n'.join(SKIPPED_PACKAGES))
  licensing = Licensing(packages)
  licensing.ProcessPackages()
  if not testpkg:
    for extra_pkg in [
        ['x11-base', 'X.Org', '1.9.3', ['http://www.x.org/'], ['X']],
        ['sys-kernel', 'Linux', '2.6', ['http://www.kernel.org/'], ['GPL-2']]
    ]:
      licensing.AddExtraPkg(extra_pkg)
  licensing.GenerateHTMLLicenseOutput(output_file)
  if licensing.incomplete_packages:
    raise AssertionError("""
DO NOT USE OUTPUT!!!
Some packages are missing due to errors, please look at errors generated during
this run.
List of packages with errors:
%s
  """ % '\n'.join(licensing.incomplete_packages))
