#!/usr/bin/python
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# TODO?: recursively look in packages to see if they have license files not
# at their top level.

"""Script that attempts to generate an HTML file containing license
information and homepage links for all installed packages.

Usage:
  license.py $board license-out.html

WARNING: this script in its current form is not finished or considered
production quality/code style compliant. This is an intermediate checkin
to allow for incremental cleanups and improvements that will make it
production quality.
"""

import cgi
import multiprocessing
import os
import portage
import subprocess
import sys

EQUERY_BASE = '/usr/local/bin/equery-%s'

PROCESS_EBUILD_SCRIPT = './process_ebuild.sh'

DISTFILES_DIR = '/var/lib/portage/distfiles'

STOCK_LICENSE_DIRS = [
  os.path.expanduser('~/trunk/src/third_party/portage/licenses'),
  os.path.expanduser('~/trunk/src/third_party/portage-stable/licenses'),
  os.path.abspath(os.path.join(os.path.dirname(__file__), 'licenses')),
]

# Virtual packages don't need to have a license and often don't, so we skip them
# chromeos-base contains google platforms packages that are covered by the
# general license at top of tree, so we skip those too.
SKIPPED_CATEGORIES = [
  'chromeos-base', # TODO: this shouldn't be excluded.
  'virtual',
]

SKIPPED_PACKAGES = [
  # Fix these packages by adding a real license in the code.
  'chromeos-base/madison-cromo-plugin-0.1-r40',
  'dev-util/perf',

  # These are Chrome-OS-specific packages.
  'dev-util/hdctools',
  'media-libs/libresample',
  'sys-apps/rootdev',
  'sys-kernel/chromeos-kernel',  # already manually credit Linux
  'sys-apps/flashmap',

  'www-plugins/adobe-flash',

  # These have been split across several packages, so we skip listing the
  # individual components (and just list the main package instead).
  'app-editors/vim-core',
  'x11-apps/mesa-progs',

  # Portage metapackage.
  'x11-base/xorg-drivers',

  # Written by google;
  'dev-db/leveldb',

  # These are covered by app-i18n/ibus-mozc (BSD, copyright Google).
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
  'third_party'
]

PACKAGE_LICENSES = {
  'app-admin/eselect-opengl': ['GPL-2'],
  'app-crypt/nss': ['MPL-1.1'],
  'app-editors/gentoo-editor': ['MIT-gentoo-editor'],
  'app-editors/vim': ['vim'],
  'app-i18n/ibus-mozc': ['BSD-Google'],
  'app-i18n/ibus-mozc-pinyin': ['BSD-google'],
  'dev-db/sqlite': ['sqlite'],
  'dev-libs/libevent': ['BSD-libevent'],
  'dev-libs/nspr': ['GPL-2'],
  'dev-libs/nss': ['GPL-2'],
  'dev-libs/protobuf': ['BSD-Google'],
  'dev-python/netifaces': ['netiface'],
  'dev-util/bsdiff': ['BSD-bsdiff'],
  'dev-util/quipper': ['BSD-Google'],
  'media-fonts/font-util': ['font-util'],  # COPYING file from git repo
  'media-libs/freeimage': ['GPL-2'],
  'media-libs/jpeg': ['jpeg'],
  'media-plugins/o3d': ['BSD-Google'],
  'net-dialup/ppp': ['ppp-2.4.4'],
  'net-dns/c-ares': ['MIT-MIT'],
  'net-misc/dhcpcd': ['BSD-dhcpcd'],
  'net-misc/iputils': ['BSD-iputils'],
  'net-wireless/wpa_supplicant': ['GPL-2'],
  'net-wireless/iwl1000-ucode': ['Intel-iwl1000'],
  'net-wireless/marvell_sd8787': ['Marvell'],
  'sys-apps/less': ['BSD-less'],
  'sys-libs/ncurses': ['ncurses'],
  'sys-libs/talloc': ['LGPL-3'],  # ebuild incorrectly says GPL-3
  'sys-libs/timezone-data': ['public-domain'],
  'sys-process/vixie-cron': ['vixie-cron'],
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

  # TODO, what is this doing?
  @property
  def cpvr(self):
    s = '%s-%s' % (self.fullname, self.version)
    if self.revision:
      s += '-r%s' % self.revision
    return s

  @property
  def fullname(self):
    return '%s/%s' % (self.category, self.name)

  def _RunEbuildPhases(self, path, *phases, **kwargs):
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

    path = GetEbuildPath(board, self.cpvr)
    self._RunEbuildPhases(
      path, 'clean', 'fetch',
      stdout=open('/dev/null', 'wb'),
      stderr=subprocess.STDOUT)
    self._RunEbuildPhases(path, 'unpack')
    try:
      return self._ExtractLicense()
    finally:
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
    workdir = os.path.join(tmpdir, 'portage', self.cpvr, 'work')

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
        for comp in SKIPPED_LICENSE_FILENAME_COMPONENTS:
          if comp in name:
            has_skipped_component = True
            break
        if not has_skipped_component:
          licenses.append(name)

    if not licenses:
      print "%s: couldn't find license file in %s" % \
        (self.fullname, workdir)
      return False

    print '%s: using %s' % (self.fullname, ' '.join(licenses))
    license_file = licenses[0]
    self.license_text = open(os.path.join(workdir, license_file)).read()
    return True

  def GetStockLicense(self):
    if not self.license_names:
      print '%s: no stock licenses from ebuild' % self.fullname
      return False

    print '%s: using stock license %s' % \
      (self.fullname, ','.join(self.license_names))

    license_texts = []
    for license_name in self.license_names:
      license_path = None
      for directory in STOCK_LICENSE_DIRS:
        path = '%s/%s' % (directory, license_name)
        if os.access(path, os.F_OK):
          license_path = path
          break
      if license_path:
        license_texts.append(open(license_path).read())
      else:
        # TODO: We should probably report failure if we're unable to
        # find one of the licenses from a dual-licensed package.
        print '%s: stock license %s does not exist' % \
          (self.fullname, license_name)

    if not license_texts:
      print '%s: couldn\'t find any stock licenses' % self.fullname
      return False

    self.license_text = '\n'.join(license_texts)
    return True


def ListInstalledPackages(board):
  """Return a list of all packages installed for a particular board."""
  args = [ EQUERY_BASE % board, 'list', '*' ]
  p = subprocess.Popen(args, stdout=subprocess.PIPE)
  return [s.strip() for s in p.stdout.readlines()]


def GetFakePackages():
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
  p = subprocess.Popen(
    ['equery-%s' % board, 'which', name], stdout=subprocess.PIPE)
  stdout = p.communicate()[0]
  p.wait()
  return stdout.strip()


def GetPackageInfo(fullname):
  info = PackageInfo(*portage.versions.catpkgsplit(fullname))

  if info.category in SKIPPED_CATEGORIES or info.fullname in SKIPPED_PACKAGES:
    return None

  ebuild = GetEbuildPath(board, fullname)

  if not os.access(ebuild, os.F_OK):
    return info

  args = [
    'portageq',
    'metadata',
    '/build/%s' % board,
    'ebuild',
    fullname,
    'HOMEPAGE', 'LICENSE', 'DESCRIPTION',
  ]
  p = subprocess.Popen(args, stdout=subprocess.PIPE)
  lines = [s.strip() for s in p.stdout.readlines()]
  p.wait()
  if p.returncode != 0:
    return info

  if PACKAGE_HOMEPAGES.has_key(info.fullname):
    info.homepages = PACKAGE_HOMEPAGES[info.fullname]
  else:
    info.homepages = lines[0].split()

  if PACKAGE_LICENSES.has_key(info.fullname):
    licenses = PACKAGE_LICENSES[info.fullname]
  else:
    licenses = lines[1].split()

  info.license_names = []
  for license in licenses:
    if license in INVALID_STOCK_LICENSES:
      print '%s: skipping invalid stock license %s' % \
            (info.fullname, license)
    else:
      info.license_names.append(license)

  info.description = '\n'.join(lines[2:])

  return info


def EvaluateTemplate(template, env, escape=True):
  """Expand a template with variables like {{foo}} using a
  dictionary of expansions."""
  for key, val in env.items():
    if escape:
      val = cgi.escape(val)
    template = template.replace('{{%s}}' % key, val)
  return template

def ProcessPkg(package):
  info = GetPackageInfo(package)
  if not info:
    return (package, None)
  if not info.ExtractLicense() and not info.GetStockLicense():
    raise RuntimeError("%s: unable to find license" % info.cpvr)
  return (package, info)


if __name__ == '__main__':
  singlethreaded = False
  debug = False
  if len(sys.argv) > 1 and sys.argv[1] == "--debug":
    singlethreaded = True
    debug = True
    sys.argv.pop(0)
    print >> sys.stderr, '>>> Debug enabled, using single threading <<<\n'

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

  board = sys.argv[1]
  packages = ListInstalledPackages(board)
  if debug:
    print "Package list to work through:"
    print "\n".join(packages)
    print "\n\nWill skip these packages:"
    print "\n".join(SKIPPED_PACKAGES)

  if singlethreaded:
    data = [ProcessPkg(x) for x in packages]
  else:
    data = multiprocessing.Pool().map(ProcessPkg, packages, 1)

  infos = [x[1] for x in data if x[1] is not None]

  infos += GetFakePackages()
  infos.sort(key=lambda x:(x.name, x.version, x.revision))

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
