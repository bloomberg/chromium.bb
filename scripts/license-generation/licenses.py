#!/usr/bin/python
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# TODO?: recursively look in packages to see if they have license files not
# at their top level.
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
  board=x86-alex
  cros_sdk
  sudo rm -rf /build/$board
  setup_board --board=$board
  build_packages --board=$board --nowithautotest --nowithtest --nowithdev
  cd ~/trunk/chromite/scripts/license-generation
  ./licenses.py --debug $board out.html | tee output.sav

The script is still experimental. Review at least ERROR output from it.

The output file is meant to update
http://src.chromium.org/viewvc/chrome/trunk/src/chrome/browser/resources/ +
  chromeos/about_os_credits.html?view=log
(gclient config svn://svn.chromium.org/chrome/trunk/src)
For an example CL, see https://codereview.chromium.org/13496002/

UPDATE: gcl will probably fail now, because the file is too big. Before it
gets moved somewhere else, you should just use svn diff and svn commit.

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
  'chromeos-base', # TODO: this shouldn't be excluded.
  'virtual',
]

SKIPPED_PACKAGES = [
  # Fix these packages by adding a real license in the code.
  'dev-python/unittest2', # BSD

  # These are Chrome-OS-specific packages, copyright BSD-Google
  'dev-util/hdctools',
  'media-libs/libresample',
  'sys-apps/rootdev',
  'sys-kernel/chromeos-kernel',  # already manually credit Linux
  'sys-apps/flashmap',

  # These have been split across several packages, so we skip listing the
  # individual components (and just list the main package instead).
  'app-editors/vim-core',
  'x11-apps/mesa-progs',

  # Portage metapackage.
  'x11-base/xorg-drivers',

  # Written by google;
  'dev-db/leveldb',

  # These are covered by app-i18n/ibus-mozc (BSD, copyright Google).
  # TODO(merlin): These should not be exceptions, but under the BSD-Google
  # umbrella.
  'app-i18n/ibus-mozc',
  'app-i18n/ibus-mozc-chewing',
  'app-i18n/ibus-mozc-hangul',
  'app-i18n/ibus-mozc-pinyin',

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

ARCHIVE_SUFFIXES = [ '.tar.gz', '.tgz', '.tar.bz2', '.tbz2' ]

LICENSE_FILENAMES = [
  'COPYING',
  'COPYRIGHT',    # used by strace
  'LICENCE',      # used by openssh
  'LICENSE',
  'LICENSE.txt',  # used by NumPy, glew
  'LICENSE.TXT',  # used by hdparm
  'License',      # used by libcap
  'copyright',
  'IPA_Font_License_Agreement_v1.0.txt',  # used by ja-ipafonts
]

SKIPPED_LICENSE_FILENAME_COMPONENTS = [
  # FIXME: check whether this should be excluded.
  'third_party'
]

PACKAGE_LICENSES = {
  # The licenses should be set in the ebuild, but are not. Look into
  # why and fix upstream as appropriate.
  'app-admin/eselect-opengl': ['GPL-2'],
  'dev-libs/nspr': ['GPL-2'],
  'dev-libs/nss': ['GPL-2'],
  'media-libs/freeimage': ['GPL-2'],
  'net-wireless/wpa_supplicant': ['GPL-2'],
  'sys-libs/talloc': ['LGPL-3'],  # ebuild incorrectly says GPL-3
  'app-crypt/nss': ['MPL-1.1'],

  # One off licenses. We should check in a custom LICENSE file for these:
  'dev-db/sqlite': ['sqlite'],
  'dev-python/netifaces': ['netiface'],
  'media-libs/jpeg': ['jpeg'],
  'net-dialup/ppp': ['ppp-2.4.4'],
  'net-wireless/marvell_sd8787': ['Marvell'],
  'sys-libs/ncurses': ['ncurses'],

  'app-editors/vim': ['vim'],
  'sys-libs/timezone-data': ['public-domain'],

  # These packages are not in Alex, check and remove.
  # 'media-fonts/font-util': ['font-util'],  # COPYING file from git repo
  # 'net-wireless/iwl1000-ucode': ['Intel-iwl1000'],
  # 'sys-process/vixie-cron': ['vixie-cron'],

  # BSD and MIT license authorship mapping.
  # Ideally we should have a custom LICENSE file in the upstream source.
  'dev-libs/libevent': ['BSD-libevent'],
  'dev-util/bsdiff': ['BSD-bsdiff'],
  'net-misc/dhcpcd': ['BSD-dhcpcd'],
  'net-misc/iputils': ['BSD-iputils'],
  'sys-apps/less': ['BSD-less'],
  'app-editors/gentoo-editor': ['MIT-gentoo-editor'],
  'net-dns/c-ares': ['MIT-MIT'],

  'chromeos-base/madison-cromo-plugin-0.1-r40': ['BSD-Google'],
  'app-i18n/input-tools': ['BSD-Google'],
  'app-i18n/nacl-mozc': ['BSD-Google'],
  'app-i18n/ibus-mozc': ['BSD-Google'],
  'app-i18n/ibus-mozc-pinyin': ['BSD-Google'],
  'dev-libs/protobuf': ['BSD-Google'],
  'dev-util/quipper': ['BSD-Google'],
  'media-plugins/o3d': ['BSD-Google'],
  'x11-drivers/xf86-input-cmt': ['BSD-Google'],
}

INVALID_STOCK_LICENSES = [
  'BSD',  # requires distribution of copyright notice
  'MIT',  # requires distribution of copyright notice
]

PACKAGE_HOMEPAGES = {
  'app-editors/vim': ['http://www.vim.org/'],
  'x11-proto/glproto': ['http://www.x.org/'],
}

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
    self.license_text = None

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

  def ExtractLicense(self):
    """Try to get a license from the package by unpacking it with ebuild
    and looking for license files in the unpacked tree.
    """

    # Some packages have hardcoded licenses and are generated in
    # GetPackageInfo, so we skip the license extraction and exit early.
    if self.fullname in PACKAGE_LICENSES:
      return False

    path = GetEbuildPath(board, self.fullnamerev)
    self._RunEbuildPhases(
      path, 'clean', 'fetch',
      stdout=open('/dev/null', 'wb'),
      stderr=subprocess.STDOUT)
    self._RunEbuildPhases(path, 'unpack')
    try:
      return self._ExtractLicense()
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

    args = ['find', workdir + '/', '-maxdepth', '3',
            '-mindepth', '1', '-type', 'f']
    p = subprocess.Popen(args, stdout=subprocess.PIPE)
    files = p.communicate()[0].splitlines()
    ret = p.wait()
    if ret != 0:
      raise AssertionError('exit code was not 0: got %s' % ret)

    files = [x[len(workdir):].lstrip('/') for x in files]
    licenses = []
    for name in files:
      if os.path.basename(name) in LICENSE_FILENAMES and \
        (name.count('/') == 1 or (name.count('/') == 2 and
         name.split('/')[1] == 'doc')):
        has_skipped_component = False
        # FIXME: Should we really exclude third_party?
        for comp in SKIPPED_LICENSE_FILENAME_COMPONENTS:
          if comp in name:
            has_skipped_component = True
            break
        if not has_skipped_component:
          licenses.append(name)

    if not licenses:
      logging.warn("%s: couldn't find license file in %s",
          self.fullnamerev, workdir)
      return False

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
    self.license_text = ""
    for license_file in licenses:
      logging.debug("Adding license %s:", os.path.join(workdir, license_file))
      self.license_text += "Source license %s:\n\n" % license_file
      self.license_text += open(os.path.join(workdir, license_file)).read()
      self.license_text += "\n\n"
    return True

  # See if the ebuild file itself contains a license, in case there is no text
  # license in the source code.
  def GetStockLicense(self):
    if not self.license_names:
      logging.error('%s: no stock licenses from ebuild', self.fullnamerev)
      return False

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
        # If a package with multiple stock licenses has one that we don't have,
        # we report this, but it's ok to continue since we only have to honor/
        # repeat one of the licenses. Still, worth looking into just in case.
        # sys-apps/hwids currently has a LICENSE field that triggers this:
        # LICENSE="|| ( GPL-2 BSD )"
        logging.error('%s: stock license %s could not be found in %s',
            self.fullnamerev, license_name, 'n'.join(STOCK_LICENSE_DIRS))

    if not license_texts:
      logging.error('%s: couldn\'t find any stock licenses', self.fullnamerev)
      return False

    self.license_text = '\n'.join(license_texts)
    return True


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
    pkg.GetStockLicense()
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

  # FIXME(merlin): Is it ok to just return an unprocessed object if the
  # ebuild can't be found? I think not. Consider dying here.
  if not os.access(ebuild, os.F_OK):
    logging.error("Can't access %s", ebuild)
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
  info.license_names = []
  for license_name in licenses:
    # Licenses like BSD or MIT can't be used as it because they do not contain
    # copyright info. They have to be replaced by a custom file generated by us.
    if license_name in INVALID_STOCK_LICENSES:
      logging.warning('%s: cannot use stock license %s, skipping...',
                   info.fullnamerev, license_name)
    else:
      info.license_names.append(license_name)

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

  # From that info object, either get a mapped license file as per the
  # PACKAGE_LICENSES dict, or retrieve/unpack the source to look for license
  # files (scanning recursively by file name).
  # Note that finding a license file OVERRIDES any license specified in
  # the ebuild.
  if (not info.ExtractLicense() and
      not info.GetStockLicense()):
      # ^^^^^^^^^^^^^^^^^^^^^^
      # If no license looking file is found in source archive, use our own copy
      # of a stock license file matching what's defined in the ebuild (if any):
    raise AssertionError("""
%s: unable to find usable license.
Typically this will happen because the ebuild says it's MIT or BSD, but there
was no license file that this script could find to include along with a
copyright attribution (required for BSD/MIT).
Go investigate the unpacked source in /tmp/boardname/tmp/portage/..., and
find which license to assign. Once you found it, add a static mapping to the
PACKAGE_LICENSES dict.
If there was a usable license file, you may also want to teach this script to
find it if you have time."""
      % info.fullnamerev)

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
  # For temporary single package debugging (make sure to include trailing -ver):
  #packages = [ "dev-python/unittest2-0.5.1" ]
  logging.debug("Package list to work through:")
  logging.debug("\n".join(packages))
  logging.debug("Will skip these packages:")
  logging.debug("\n".join(SKIPPED_PACKAGES))

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
