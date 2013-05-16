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
  ./licenses.py --debug $board out.html 2>&1 | tee output.sav

The script is still experimental. Review at least ERROR output from it.

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
grep -E -A5 '(class=title|Gentoo Package Provided Stock|Source license)' \
    out.html  > /tmp/new
grep -E -A5 '(class=title|Gentoo Package Provided Stock|Source license)' \
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
import logging
import os
import portage
import subprocess
import sys

# This keeps track of whether we have an incomplete license file due to package
# errors during parsing.
# Any non empty list at the end shows the list of packages that caused errors.
OUTPUT_INCOMPLETE = []

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
  'chromeos-base', # TODO: this shouldn't be excluded ?
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
  'licence',        # used by openssh
  'license',
  'license.txt',    # used by hdparm, used by NumPy, glew
  'licensing.txt',  # used by libatomic_ops
  'copyright',
  'ipa_font_license_agreement_v1.0.txt',  # used by ja-ipafonts
]

SKIPPED_LICENSE_FILENAME_COMPONENTS = [
  # FIXME: check whether this should be excluded.
  'third_party'
]

# These are _temporary_ license mappings for packages that do not have a valid
# stock license, or LICENSE file we can use.
# Once this script runs earlier (during the package build process), it will
# block new source without a LICENSE file if the ebuild contains a license
# that requires copyright assignment (BSD and friends).
# At that point, new packages will get fixed to include LICENSE instead of
# adding workaround mappings like those below.
# We should also fix the packages listed below so that the hardcoded
# mappings can be obsoleted.
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

  # Mesa ebuild says MIT and seems to omit LGPL-3 and SGI-B-2.0 mentioned in the
  # docs directory? Either way, I had to create a text license file like so:
  # mesa-9.1-r9/work/Mesa-9.1/docs$ lynx --dump license.html -nolist > license
  'media-libs/mesa': [ 'MIT-Mesa', 'LGPL-3','SGI-B-2.0' ],

  # TODO: Ebuild seems to wrongfully say BSD + public-domain.
  # I scanned the unpacked source with licensecheck and didn't find any BSD.
  # FIXME: Do a second review and fix upstream gentoo package
  'sys-libs/timezone-data': [ 'public-domain' ],

  # Ebuild only says 'LGPL-2.1', but source disagrees. I'll include 'as-is'
  # to force reading files from the source (which states some parts are as-is).
  # FIXME? Should the ebuild license be updated to match xz-4.999.9beta/COPYING?
  'app-arch/xz-utils': [ 'public-domain', 'as-is', 'LGPL-2.1', 'GPL-2' ],

  # These packages are not in Alex, check and remove later (might be used in
  # other platforms).
  #'media-libs/freeimage': ['GPL-2'],
  #'sys-libs/talloc': ['LGPL-3'],  # ebuild incorrectly says GPL-3
  #'app-crypt/nss': ['MPL-1.1'],
  #'media-libs/jpeg': ['jpeg'],
  #'app-editors/gentoo-editor': ['MIT-gentoo-editor'],
  #
  # 'media-fonts/font-util': ['font-util'],  # COPYING file from git repo
  # 'net-wireless/iwl1000-ucode': ['Intel-iwl1000'],
  # 'sys-process/vixie-cron': ['vixie-cron'],
}

# FIXME(merlin): remove Old-MIT from the licenses shipped with this script
# and portage-stable/licenses/... should be resynced against
# http://git.chromium.org/gitweb/?p=chromiumos/overlays/portage.git;a=summary

# Any license listed list here found in the ebuild will make the code look for
# license files inside the package source code in order to get copyright
# attribution from them.
COPYRIGHT_ATTRIBUTION_LICENSES = [
  'BSD',    # requires distribution of copyright notice
  'BSD-2',  # so does BSD-2 http://opensource.org/licenses/BSD-2-Clause
  'BSD-3',  # and BSD-3? http://opensource.org/licenses/BSD-3-Clause
  'BSD-4',  # and 4?
  'BSD-with-attribution',
  'Old-MIT',
  'MIT',
  'MIT-with-advertising',
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
# these licenses, but it's simple and reliable to do it, so go for it.
  'BZIP2',     # Single use license, do copyright attribution because it's easy.
  'OFL',       # Almost single use license, do copyright attribution.
  'OFL-1.1',   # Almost single use license, do copyright attribution.
  'UoI-NCSA',  # Only used by NSCA, might as well show their custom copyright.
]

PACKAGE_HOMEPAGES = {
  'app-editors/vim': ['http://www.vim.org/'],
  'x11-proto/glproto': ['http://www.x.org/'],
}

# These are tokens found in LICENSE= in an ebuild that aren't licenses we
# can actually read from disk.
# You should not use this to blacklist real licenses.
LICENCES_IGNORE = [
  ')',            # Ignore OR tokens from LICENSE="|| ( LGPL-2.1 MPL-1.1 )"
  '(',
  '||',
  'International', # Workaround for LICENSE="Marvell International Ltd."
  'Ltd.',          # Find Marvell and ignore the other 2 tokens (FIXME upstream)
]

TEMPLATE_FILE = 'about_credits.tmpl'
ENTRY_TEMPLATE_FILE = 'about_credits_entry.tmpl'

class PackageInfo:
  def __init__(self, category=None, name=None, version=None, revision=None):
    self.category = category
    self.name = name
    self.version = version
    if revision is not None:
      revision = str(revision).lstrip('r')
      if revision == '0':
        revision = None
    self.revision = revision
    self.description = None
    self.homepages = []
    self.license_names = []
    # Made up from stock license(s).
    self.license_text_stock = ""
    # Read from the source code if any.
    self.license_text_scanned = ""
    # Shows source scanned licenses first, and then stock ones.
    self.license_text = None
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

  def _RunEbuildPhases(self, path, *phases, **kwargs):
    """Receives something like:
      path = /mnt/host/source/src/
                 third_party/portage-stable/net-misc/rsync/rsync-3.0.8.ebuild
      phases = ['clean', 'fetch'] or ['unpack']."""

    #logging.debug('ebuild-%s | %s | %s', board, path, str(list(phases)))
    subprocess.check_call(
      ['ebuild-%s' % board, path] + list(phases), **kwargs)

  def ExtractLicenses(self):
    """Try to get licenses from the package by unpacking it with ebuild
    and looking for license files in the unpacked tree.
    This is only called if we couldn't get usable licenses from the ebuild,
    or one of them is BSD/MIT like, and forces us to look for a file with
    copyright attribution in the source code itself.
    """

    path = GetEbuildPath(board, self.fullnamerev)
    self._RunEbuildPhases(
      path, 'clean', 'fetch',
      stdout=open('/dev/null', 'wb'),
      stderr=subprocess.STDOUT)
    self._RunEbuildPhases(path, 'unpack')
    try:
      self._ExtractLicense()
    finally:
      if not debug:
        # In debug mode, leave unpacked trees so that we can look for files
        # inside them.
        self._RunEbuildPhases(path, 'clean')

  def _ExtractLicense(self):
    """Scan the unpacked source code for what looks like license files
    as defined in LICENSE_FILENAMES.
    """
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
    licenses = []
    for name in files:
      if os.path.basename(name).lower() in LICENSE_FILENAMES:
        has_skipped_component = False
        # FIXME: Should we really exclude third_party?
        # (someone coded it that way with no comments as to why).
        for comp in SKIPPED_LICENSE_FILENAME_COMPONENTS:
          if comp in name:
            has_skipped_component = True
            break
        if not has_skipped_component:
          licenses.append(name)

    if not licenses:
      if self.need_copyright_attribution:
        OUTPUT_INCOMPLETE.append(self.fullname)
        logging.error("%s used license with copyright attribution, but "
                      "couldn't find license file in %s",
                      self.fullnamerev, workdir)
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
    logging.info('License(s) for %s: %s', self.fullnamerev, ' '.join(licenses))
    for license_file in licenses:
      logging.debug("Adding license %s:", os.path.join(workdir, license_file))
      self.license_text_scanned += "Source license %s:\n\n" % license_file
      self.license_text_scanned += open(os.path.join(workdir,
                                        license_file)).read()
      self.license_text_scanned += "\n\n"

  # Read stock license files specified in an ebuild.
  def ReadStockLicense(self):
    logging.info('%s: using stock license(s) %s',
                 self.fullnamerev, ','.join(self.license_names))

    license_texts = []
    for license_name in self.license_names:
      logging.debug("looking for license %s for %s", license_name,
                    self.fullnamerev)
      license_path = None
      for directory in STOCK_LICENSE_DIRS:
        path = '%s/%s' % (directory, license_name)
        if os.access(path, os.F_OK):
          license_path = path
          break
      if license_path:
        logging.info('%s: reading license %s', self.fullnamerev, license_path)
        license_texts.append("Gentoo Package Provided Stock License %s:" %
                             license_name)
        license_texts.append(open(license_path).read())
        license_texts.append("\n\n")
      else:
        logging.error('%s: stock license %s could not be found in %s',
                      self.fullnamerev, license_name,
                      '\n'.join(STOCK_LICENSE_DIRS))
        OUTPUT_INCOMPLETE.append(self.fullname)

    if license_texts:
      self.license_text_stock += '\n'.join(license_texts)
    else:
      logging.error('%s: couldn\'t find any stock licenses', self.fullnamerev)


def ListInstalledPackages(board):
  """Return a list of all packages installed for a particular board."""
  # FIXME(merlin): davidjames pointed out that this is
  # not the right way to get the package list as it does not apply
  # filters. This should change to ~/trunk/src/scripts/get_package_list
  args = [EQUERY_BASE % board, 'list', '*']
  p = subprocess.Popen(args, stdout=subprocess.PIPE)
  return [s.strip() for s in p.stdout.readlines()]


def BuildMetaPackages():
  pkgs = []

  pkg = PackageInfo('x11-base', 'X.Org', '1.9.3')
  pkg.homepages = [ 'http://www.x.org/' ]
  pkg.license_names = [ 'X' ]
  pkgs.append(pkg)

  pkg = PackageInfo('sys-kernel', 'Linux', '2.6')
  pkg.homepages = [ 'http://www.kernel.org/' ]
  pkg.license_names = [ 'GPL-2' ]
  pkgs.append(pkg)

  for pkg in pkgs:
    pkg.ReadStockLicense()
  return pkgs


def GetEbuildPath(board, name):
  """Turns (x86-alex, net-misc/wget-1.12) into
  /mnt/host/source/src/third_party/portage-stable/net-misc/wget/wget-1.12.ebuild
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


def GetPackageInfo(fullnamewithrev):
  """Create a PackageInfo object and populate its license, homepage and
  description if they are valid.
  Returns a package info object if the package isn't in a skip list.
  A package without license is returned with incomplete data, a return of
  None actually means we don't want to keep track of this package."""

  #                 (category, name, version, revision)
  info = PackageInfo(*portage.versions.catpkgsplit(fullnamewithrev))
  # The above will error if portage returns Null because you fed a bad package
  # name, or forgot to append the version number:
  # TypeError: PackageInfo constructor argument after * must be a sequence,
  #   not NoneType


  if info.category in SKIPPED_CATEGORIES:
    logging.info("%s in SKIPPED_CATEGORIES, skip info object creation",
                 info.fullname)
    return None

  if info.fullname in SKIPPED_PACKAGES:
    logging.info("%s in SKIPPED_PACKAGES, skip info object creation",
                 info.fullname)
    return None

  ebuild = GetEbuildPath(board, info.fullnamerev)

  if not os.access(ebuild, os.F_OK):
    logging.error("Can't access %s", ebuild)
    OUTPUT_INCOMPLETE.append(info.fullname)
    return info

  cmd = [
    'portageq',
    'metadata',
    '/build/%s' % board,
    'ebuild',
    info.fullnamerev,
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

  (info.homepages,    licenses,         info.description) = (
    lines[0].split(), lines[1].split(), lines[2:])

  # NOTE: the ebuild license field can look like:
  # LICENSE="GPL-3 LGPL-3 Apache-2.0" (this means AND, as in all 3)
  # for third_party/portage-stable/app-admin/rsyslog/rsyslog-5.8.11.ebuild
  # LICENSE="|| ( LGPL-2.1 MPL-1.1 )"
  # for third_party/portage-stable/x11-libs/cairo/cairo-1.8.8.ebuild
  # LICENSE="Marvell International Ltd."
  # for net-wireless/marvell_sd8787/marvell_sd8787-14.64.2.47-r16.ebuild

  # The parser does not know the || ( X Y ) OR logic
  # It partially ignores the AND logic by skipping BSD licenses
  # And it craps out on the Marvel license trying to look for
  # 'Marvel' (found), 'International' (not found), 'Ltd' (not found).
  # This blows...

  # Solution: show all licenses listed and ignore AND/OR.
  # Later down, we ignore some license tokens like these (LICENCES_IGNORE):
  # warnings '||' '(' ')' 'International' 'Ltd.'

  if info.fullname in PACKAGE_HOMEPAGES:
    info.homepages = PACKAGE_HOMEPAGES[info.fullname]

  # Packages with missing licenses or licenses that need mapping (like BSD/MIT)
  # are hardcoded here:
  if info.fullname in PACKAGE_LICENSES:
    licenses = PACKAGE_LICENSES[info.fullname]
    logging.debug("Static license mapping for %s: %s", info.fullnamerev,
                  ",".join(licenses))
  else:
    logging.debug("Read licenses from ebuild for %s: %s", info.fullnamerev,
                  ",".join(licenses))

  if not licenses:
    logging.error("%s: no license found in ebuild. FIXME!", info.fullnamerev)
    # In a bind, you could comment this out. I'm making the output fail to get
    # your attention since this error really should be fixed, but if you comment
    # out the next line, the script will try to find a license inside the source
    OUTPUT_INCOMPLETE.append(info.fullname)

  if "||" in licenses[1:]:
    raise AssertionError('Cannot parse || in the middle of a license for %s: %s'
      % (info.fullnamerev, ' '.join(licenses)))

  info.license_names = []

  or_licenses_and_one_is_no_attribution = False
  for license_name in [ x for x in licenses if x not in LICENCES_IGNORE ]:
    # Here we have an OR case, and one license that we can use stock, so
    # we remember that in order to be able to skip license attributions if
    # any were in the OR.
    if (licenses[0] == "||" and
        license_name not in COPYRIGHT_ATTRIBUTION_LICENSES):
      or_licenses_and_one_is_no_attribution = True

  for license_name in [ x for x in licenses if x not in LICENCES_IGNORE ]:
    # Licenses like BSD or MIT can't be used as is because they do not contain
    # copyright info. They have to be replaced by copyright file given in the
    # source code, or manually mapped by us in PACKAGE_LICENSES
    if license_name in COPYRIGHT_ATTRIBUTION_LICENSES:
      # To limit needless efforts, if a package is BSD or GPL, we ignore BSD and
      # use GPL to avoid scanning the package, but we can only do this if
      # or_licenses_and_one_is_no_attribution has been set above.
      # This ensures that if we have License: || (BSD3 BSD4), we will
      # look in the source.
      if or_licenses_and_one_is_no_attribution:
        logging.info("%s: ignore license %s because ebuild LICENSES had %s",
                     info.fullnamerev, license_name, ' '.join(licenses))
      else:
        logging.info("%s: can't use %s, will scan source code for copyright...",
                     info.fullnamerev, license_name)
        info.need_copyright_attribution = True
        info.scan_source_for_licenses = True
    else:
      info.license_names.append(license_name)

    if license_name in LOOK_IN_SOURCE_LICENSES:
      logging.info("%s: Got %s, will try to find better license in source...",
                   info.fullnamerev, license_name)
      info.scan_source_for_licenses = True

  return info


def EvaluateTemplate(template, env, escape=True):
  """Expand a template with variables like {{foo}} using a
  dictionary of expansions."""
  for key, val in env.iteritems():
    if escape:
      val = cgi.escape(val)
    template = template.replace('{{%s}}' % key, val)
  return template

def ProcessPkg(package):
  # First, we try to retrieve package and license data from the ebuild, and
  # return this in a created info object.
  # This will also set static license mappings stored in this script.
  info = GetPackageInfo(package)
  # None is returned if the package is in a skip list, not if the license is
  # invalid or missing.
  if not info:
    return None

  if info.license_names:
    info.ReadStockLicense()
  else:
    logging.warning('%s: no usable licenses in ebuild', info.fullnamerev)

  # If the license(s) could not be found, or one requires copyright attribution,
  # dig in the source code for license files:
  # For instance:
  # Read licenses from ebuild for net-dialup/ppp-2.4.5-r3: BSD,GPL-2
  # We need to both get the substitution file for BSD and add it to the GPL
  # license retrieved by ReadStockLicense.
  if not info.license_names or info.scan_source_for_licenses:
    info.ExtractLicenses()

  # Show scanned licenses first, they're usually better.
  info.license_text = info.license_text_scanned + info.license_text_stock

  if not info.license_text:
    logging.error("""
%s: unable to find usable license.
Typically this will happen because the ebuild says it's MIT or BSD, but there
was no license file that this script could find to include along with a
copyright attribution (required for BSD/MIT).
Go investigate the unpacked source in /tmp/boardname/tmp/portage/..., and
find which license to assign. Once you found it, add a static mapping to the
PACKAGE_LICENSES dict.
If there was a usable license file, you may also want to teach this script to
find it if you have time.""",
    info.fullname)
    OUTPUT_INCOMPLETE.append(info.fullname)

  return info


if __name__ == '__main__':
  debug = False
  if len(sys.argv) > 1 and sys.argv[1] == "--debug":
    debug = True
    sys.argv.pop(0)
    logging.basicConfig(format='%(levelname)s:%(message)s', level=logging.DEBUG)
  else:
    logging.basicConfig(format='%(levelname)s:%(message)s', level=logging.INFO)


  if len(sys.argv) != 3:
    print >> sys.stderr, (__doc__)
    sys.exit(1)

  entry_template = open(ENTRY_TEMPLATE_FILE, 'rb').read()

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

  # FIXME(merlin): we should have proper command line parsing and allow passing
  # a single package to generate a license for, as an argument.
  board = sys.argv[1]
  packages = ListInstalledPackages(board)
  # If the caller forgets to set $board, it'll default to beaglebone, and return
  # no packages. Catch this and give a hint that the wrong board was given.
  if not packages:
    raise AssertionError('FATAL: Could not get any packages for board %s' %
                         board)
  # For temporary single package debugging (make sure to include trailing -ver):
  # packages = [ "dev-libs/libatomic_ops-7.2d" ]
  logging.debug("Package list to work through:")
  logging.debug('\n'.join(packages))
  logging.debug("Will skip these packages:")
  logging.debug('\n'.join(SKIPPED_PACKAGES))

  infos = filter(None, (ProcessPkg(x) for x in packages))
  infos += BuildMetaPackages()
  infos.sort(key=lambda x: (x.name, x.version, x.revision))

  entries = []
  seen_package_names = set()
  for info in infos:
    if info.name in seen_package_names:
      continue
    seen_package_names.add(info.name)
    env = {
      'name': info.name,
      'url': info.homepages[0] if info.homepages else '',
      'license': info.license_text or '',
    }
    entries.append(EvaluateTemplate(entry_template, env))

  file_template = open(TEMPLATE_FILE, 'rb').read()
  out_file = open(sys.argv[2], "w")
  out_file.write(EvaluateTemplate(file_template,
                                  { 'entries': '\n'.join(entries) },
                                  escape=False))

  if OUTPUT_INCOMPLETE:
    raise AssertionError("""
DO NOT USE OUTPUT!!!
Some packages are missing due to errors, please look at errors generated during
this run.
List of packages with errors:
%s
""" % '\n'.join(OUTPUT_INCOMPLETE))
