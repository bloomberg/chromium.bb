#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
   This script packages the PNaCl translator files as a Chrome Extension.
"""

import optparse
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

# shutil.copytree does not allow the target directory to exist.
# Borrow this copy_tree, which does allow it (overwrites conflicts?).
from distutils.dir_util import copy_tree as copytree_existing

J = os.path.join

######################################################################
# Normalize the platform name, just like SCons, etc.

def GetPlatform():
  if sys.platform == 'darwin':
    platform = 'mac'
  elif sys.platform.startswith('linux'):
    platform = 'linux'
  elif sys.platform in ('cygwin', 'win32'):
    platform = 'windows'
  else:
    raise Exception('Unknown platform: %s' % sys.platform)
  return platform

def PnaclPlatform(platform):
  if platform == 'mac':
    return 'darwin'
  return platform

PLATFORM = GetPlatform()
PNACL_BUILD_PLATFORM = PnaclPlatform(GetPlatform())

######################################################################
# Target arch and build arch junk to convert between all the
# silly conventions between SCons, Chrome and PNaCl.

def GetArch():
  arch = platform.machine()
  if arch in ('x86_64', 'amd64'):
    return 'x86-64'
  if arch.startswith('arm'):
    return 'arm'
  x86_32_re = re.compile('^i.86$')
  if x86_32_re.search(arch):
    return 'x86-32'

ARCH = GetArch()

def PnaclBuildArch():
  if PLATFORM == 'windows':
    return 'i686'
  else:
    return platform.machine()

PNACL_BUILD_ARCH = PnaclBuildArch()

def DashFreeArch(arch):
  return arch.replace('-', '')

ARCHES=['x86-32', 'x86-64', 'arm']

def IsValidArch(arch):
  return arch in ARCHES

LIBMODES=['newlib', 'glibc']

def IsValidLibmode(libmode):
  return libmode in LIBMODES

######################################################################

def GetNaClRoot():
  """ Find the native_client path, relative to this script.
  """
  script_file = os.path.abspath(__file__)
  def SearchForNaCl(cur_dir):
    if cur_dir.endswith('native_client'):
      return cur_dir
    # Detect when we've the root (linux is /, but windows is not...)
    next_dir = os.path.dirname(cur_dir)
    if cur_dir == next_dir:
      raise Exception('Could not find native_client relative to %s' %
                      script_file)
    return SearchForNaCl(next_dir)

  return SearchForNaCl(script_file)


NACL_ROOT = GetNaClRoot()


######################################################################

class CRXGen(object):
  """ Generate a CRX file. Can generate a fresh CRX and private key, or
  create a version of new CRX with the same AppID, using an existing private
  key. NOTE: We use the chrome binary to do CRX packing. There is also a bash
  script available at: http://code.google.com/chrome/extensions/crx.html
  but it is not featureful (doesn't know how to generate private keys).
  """

  @staticmethod
  def ChromeBinaryName():
    if PLATFORM == 'mac':
      return 'Chromium.app/Contents/MacOS/Chromium'
    elif PLATFORM == 'windows':
      return 'chrome.exe'
    else:
      return 'chrome'

  @staticmethod
  def GetCRXGenPath():
    plat_arch = '%s_%s' % (PLATFORM, ARCH)
    return J(NACL_ROOT, 'chromebinaries', plat_arch, CRXGen.ChromeBinaryName())

  @staticmethod
  def RunCRXGen(manifest_dir, private_key=None):
    binary = CRXGen.GetCRXGenPath()
    if not os.path.isfile(binary):
      raise Exception('NaCl downloaded chrome binary not found: %s' % binary)
    cmdline = []
    if PLATFORM == 'linux':
      # In linux, run chrome in headless mode (even though crx-packing should
      # be headless, it's not quite with the zygote). This allows you to
      # run the tool under ssh or screen, etc.
      cmdline.append('xvfb-run')
    cmdline += [binary, '--pack-extension=%s' % manifest_dir]
    if private_key is not None:
      cmdline.append('--pack-extension-key=%s' % private_key)
    StepBanner('GEN CRX', str(cmdline))
    if subprocess.call(cmdline) != 0:
      raise Exception('Failed to RunCRXGen: %s' % (cmdline))


######################################################################

def IsValidVersion(version):
  """ Return true if the version is a valid ID (a quad like 0.0.0.0).
  """
  pat = re.compile('^\d+\.\d+\.\d+\.\d+$')
  return pat.search(version)


######################################################################

class PnaclPackaging(object):

  # For dogfooding, we also create a webstore extension.
  # See: https://chrome.google.com/webstore/a/google.com/detail/gcodniebolpnpaiggndmcmmfpldlknih
  # To test offline, we need to be able to load via the command line on chrome,
  # but we also need the AppID to remain the same. Thus we supply the
  # public key in the unpacked/offline extension manifest. See:
  # http://code.google.com/chrome/extensions/manifest.html#key
  # Summary:
  # 1) install the extension, then look for key in
  # 2) <profile>/Default/Extensions/<extensionId>/<versionString>/manifest.json
  # (Fret not -- this is not the private key, it's just a key stored in the
  # user's profile directory).
  WEBSTORE_PUBLIC_KEY="MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC7zhW8iytdYid7SXLokWfxNoz2Co9x2ItkVUS53Iq12xDLfcKkUZ2RNXQtua+yKgRTRMP0HigPtn2KZeeJYzvBYLP/kz62B3nM5nS8Mo0qQKEsJiNgTf1uOgYGPyrE6GrFBFolLGstnZ1msVgNHEv2dZruC2XewOJihvmeQsOjjwIDAQAB"

  package_base = os.path.dirname(__file__)
  manifest_template = J(package_base, 'pnacl_manifest_template.json')

  @staticmethod
  def GenerateManifest(output, version, arch,
                       web_accessible_resources,
                       manifest_key=None):
    template_fd = open(PnaclPackaging.manifest_template, 'r')
    template = template_fd.read()
    template_fd.close()
    output_fd = open(output, 'w')
    extra = ''
    if web_accessible_resources != []:
      extra += '"web_accessible_resources": [%s],\n' % ',\n'.join(
          [ '"' + to_quote + '"' for to_quote in web_accessible_resources ])
    if manifest_key is not None:
      extra += '"key": "%s",\n' % manifest_key
    output_fd.write(template % { "version" : version,
                                 "extra" : extra,
                                 "arch" : arch, })
    output_fd.close()

######################################################################

class PnaclDirs(object):

  output_dir = J(NACL_ROOT, 'toolchain', 'pnacl-package')

  @staticmethod
  def BaseDir():
    pnacl_dir = 'pnacl_%s_%s' % (PNACL_BUILD_PLATFORM,
                                 PNACL_BUILD_ARCH)
    return J(NACL_ROOT, 'toolchain', pnacl_dir)

  @staticmethod
  def LibDir(target_arch):
    return J(PnaclDirs.BaseDir(), 'lib-%s' % target_arch)

  @staticmethod
  def SandboxedCompilerDir(target_arch):
    # Choose newlib's LLC and LD to simplify startup of those nexes.
    return J(PnaclDirs.BaseDir(),
             'tools-sb',
             DashFreeArch(target_arch),
             'srpc',
             'bin')

  @staticmethod
  def SetOutputDir(d):
    PnaclDirs.output_dir = d

  @staticmethod
  def OutputDir():
    return PnaclDirs.output_dir

  @staticmethod
  def OutputAllDir():
    return J(PnaclDirs.OutputDir(), 'pnacl_all')

  @staticmethod
  def OutputArchBase(arch):
    return '%s' % arch

  @staticmethod
  def OutputArchDir(arch):
    # Nest this in another directory so that the layout will be the same
    # as the "all"/universal version.
    parent_dir = J(PnaclDirs.OutputDir(), PnaclDirs.OutputArchBase(arch))
    return (parent_dir, J(parent_dir, PnaclDirs.OutputArchBase(arch)))


######################################################################

def StepBanner(short_desc, long_desc):
  print "**** %s\t%s" % (short_desc, long_desc)


def Clean():
  out_dir = PnaclDirs.OutputDir()
  StepBanner('CLEAN', 'Cleaning out old packaging: %s' % out_dir)
  if os.path.isdir(out_dir):
    shutil.rmtree(out_dir)
  else:
    print 'Clean skipped -- no previous output directory!'


def ZipDirectory(base_dir, zipfile):
  """ Zip all the files in base_dir into the given opened zipfile object.
  """
  def visit(zipfile, dirname, names):
    for name in names:
      full_name = J(dirname, name)
      zipfile.write(full_name, os.path.relpath(full_name, base_dir))

  os.path.walk(base_dir, visit, zipfile)


def ListDirectoryRecursivelyAsURLs(base_dir):
  """ List all files that can be found from base_dir.  Return names as
  URLs relative to the base_dir.
  """
  file_list = []
  def visit(accum, dirname, names):
    for name in names:
      full_name = J(dirname, name)
      if os.path.isfile(full_name):
        rel_name = os.path.relpath(full_name, base_dir)
        url = '/'.join(rel_name.split(os.path.sep))
        accum.append(url)

  os.path.walk(base_dir, visit, file_list)
  return file_list

def GeneratePrivateKey():
  """ Generate a dummy extension to generate a fresh private key. This will
  be left in the build dir, and the dummy extension will be cleaned up.
  """
  StepBanner('GEN PRIVATE KEY', 'Generating fresh private key')
  tempdir = tempfile.mkdtemp(dir=PnaclDirs.OutputDir())
  ext_dir = J(tempdir, 'dummy_extension')
  os.mkdir(ext_dir)
  PnaclPackaging.GenerateManifest(J(ext_dir, 'manifest.json'),
                                  '0.0.0.0',
                                  'dummy_arch',
                                  [])
  CRXGen.RunCRXGen(ext_dir)
  shutil.copy2(J(tempdir, 'dummy_extension.pem'),
               PnaclDirs.OutputDir())
  shutil.rmtree(tempdir)
  print ('\n<<< Fresh key is now in %s/dummy_extension.pem >>>\n' %
         PnaclDirs.OutputDir())


def BuildArchCRX(version_quad, arch, lib_overrides, options):
  """ Build an architecture specific version for the chrome component
  install (an actual CRX, vs a zip file).
  """
  parent_dir, target_dir = PnaclDirs.OutputArchDir(arch)

  StepBanner('BUILD ARCH CRX %s' % arch,
             'Packaging for arch %s in %s' % (arch, target_dir))

  # Copy llc and ld.
  copytree_existing(PnaclDirs.SandboxedCompilerDir(arch),
                    target_dir)

  # Copy newlib deps, and glibc deps.
  # Put newlib at the top level, and glibc on a separate subdir
  # for now, until all the native link line looks the same, or
  # at least there are no conflicting overlaps.
  # E.g., libgcc_eh, libgcc are still not quite the same and overlap.
  for lib_mode in LIBMODES:
    if lib_mode == 'glibc' and options.skip_glibc:
      print 'Skip copying glibc libs for arch %s' % arch
      continue

    if lib_mode == 'newlib':
      lib_dir = target_dir
    else:
      lib_dir = J(target_dir, lib_mode)
    copytree_existing(PnaclDirs.LibDir(arch), lib_dir)
    if (arch, lib_mode) in lib_overrides:
      # Also copy files from the list of overrides.
      for override in lib_overrides[(arch, lib_mode)]:
        print 'Copying override %s to %s' % (override, lib_dir)
        shutil.copy2(override, lib_dir)


  # Skip the CRX generation if we are only building the unpacked version
  # for commandline testing.
  if options.unpacked_only:
    return

  web_accessible_files = ListDirectoryRecursivelyAsURLs(parent_dir)

  # Generate manifest one level up (to have layout look like the "all" package).
  PnaclPackaging.GenerateManifest(J(parent_dir, 'manifest.json'),
                                  version_quad,
                                  arch,
                                  web_accessible_files)
  CRXGen.RunCRXGen(parent_dir, options.prev_priv_key)


def LayoutAllDir():
  StepBanner("Layout All Dir", "Copying Arch specific to Arch-independent.")
  target_dir = PnaclDirs.OutputAllDir()
  for arch in ARCHES:
    arch_parent, arch_dir = PnaclDirs.OutputArchDir(arch)
    # NOTE: The arch_parent contains the arch-specific manifest.json files.
    # We carefully avoid copying those to the "all dir" since having
    # more than one manifest.json will confuse the CRX tools (e.g., you will
    # get a mysterious failure when uploading to the webstore).
    copytree_existing(arch_dir,
                      J(target_dir, PnaclDirs.OutputArchBase(arch)))


def BuildCWSZip(version_quad):
  """ Build a 'universal' chrome extension zipfile for webstore use (where the
  installer doesn't know the target arch). Assumes the individual arch
  versions were built.
  """
  StepBanner("CWS ZIP", "Making a zip with all architectures.")
  target_dir = PnaclDirs.OutputAllDir()

  web_accessible_files = ListDirectoryRecursivelyAsURLs(target_dir)

  # Overwrite the arch-specific 'manifest.json' that was there.
  PnaclPackaging.GenerateManifest(J(target_dir, 'manifest.json'),
                                  version_quad,
                                  'all',
                                  web_accessible_files)
  target_zip = J(PnaclDirs.OutputDir(), 'pnacl_all.zip')
  zipf = zipfile.ZipFile(target_zip, 'w', compression=zipfile.ZIP_DEFLATED)
  ZipDirectory(target_dir, zipf)
  zipf.close()


def BuildUnpacked(version_quad):
  """ Build an unpacked chrome extension with all files for commandline
  testing (load on chrome commandline).
  """
  StepBanner("UNPACKED CRX", "Making an unpacked CRX of all architectures.")

  target_dir = PnaclDirs.OutputAllDir()
  web_accessible_files = ListDirectoryRecursivelyAsURLs(target_dir)
  # Overwrite the manifest file (if there was one already).
  PnaclPackaging.GenerateManifest(J(target_dir, 'manifest.json'),
                                  version_quad,
                                  'all',
                                  web_accessible_files,
                                  PnaclPackaging.WEBSTORE_PUBLIC_KEY)


def BuildAll(version_quad, lib_overrides, options):
  """ Package the pnacl components 3 ways.
  1) Arch-specific CRXes that can be queried by Omaha.
  2) A zip containing all arch files for the Chrome Webstore.
  3) An unpacked extesion with all arch files for offline testing.
  """
  StepBanner("BUILD_ALL", "Packaging for version: %s" % version_quad)
  for arch in ARCHES:
    BuildArchCRX(version_quad, arch, lib_overrides, options)
  LayoutAllDir()
  if not options.unpacked_only:
    BuildCWSZip(version_quad)
  BuildUnpacked(version_quad)


def Main():
  usage = 'usage: %prog [options] version_arg'
  parser = optparse.OptionParser(usage)
  # We may want to accept a target directory to dump it in the usual
  # output directory (e.g., scons-out).
  parser.add_option('-c', '--clean', dest='clean',
                    action='store_true', default=False,
                    help='Clean out destination directory first.')
  parser.add_option('-u', '--unpacked_only', action='store_true',
                    dest='unpacked_only', default=False,
                    help='Only generate the unpacked version')
  parser.add_option('-s', '--skip_glibc', dest='skip_glibc',
                    default=False, action='store_true',
                    help='Skip copying the glibc libraries.')
  parser.add_option('-d', '--dest', dest='dest',
                    help='The destination root for laying out the extension')
  parser.add_option('-p', '--priv_key',
                    dest='prev_priv_key', default=None,
                    help='Specify the old private key')
  parser.add_option('-L', '--lib_override',
                    dest='lib_overrides', action='append', default=[],
                    help='Specify path to a fresher native library ' +
                    'that overrides the tarball library with ' +
                    '(arch:libmode:libdir) tuple.')
  parser.add_option('-g', '--generate_key',
                    action='store_true', dest='gen_key',
                    help='Generate a fresh private key, and exit.')

  (options, args) = parser.parse_args()

  # Set destination directory before doing any cleaning, etc.
  if options.dest:
    PnaclDirs.SetOutputDir(options.dest)

  if options.clean:
    Clean()

  if options.gen_key:
    GeneratePrivateKey()
    return 0

  lib_overrides = {}
  for o in options.lib_overrides:
    arch, libmode, override_lib = o.split(',')
    if not IsValidArch(arch):
      raise Exception('Unknown arch for -L: %s (from %s)' % (arch, o))
    if not IsValidLibmode(libmode):
      raise Exception('Unknown libmode for -L: %s (from %s)' % (libmode, o))
    if not os.path.isfile(override_lib):
      raise Exception('Override native lib not a file for -L: %s (from %s)' %
                      (override_lib, o))
    override_list = lib_overrides.get((arch, libmode), [])
    override_list.append(override_lib)
    lib_overrides[(arch, libmode)] = override_list

  if len(args) != 1:
    parser.print_help()
    parser.error('Incorrect number of arguments')

  version_quad = args[0]
  if not IsValidVersion(version_quad):
    print 'Invalid version format: %s\n' % version_quad
    return 1

  BuildAll(version_quad, lib_overrides, options)
  return 0


if __name__ == '__main__':
  sys.exit(Main())
