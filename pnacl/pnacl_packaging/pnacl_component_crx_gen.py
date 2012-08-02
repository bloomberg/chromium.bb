#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
   This script packages the PNaCl translator files as a Chrome Extension (crx),
   which can be used as a straight-forward CRX, or used with the Chrome
   incremental installer (component updater).

   This script depends on and pulls in the translator nexes and libraries
   from the toolchain directory (so that must be downloaded first), fills
   in the CRX manifest files, and zips and signs the CRXes using
   chrome (which must also be downloaded first), etc.
"""

import glob
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
# Target arch and build arch junk to convert between all the
# silly conventions between SCons, Chrome and PNaCl.

# The version of the arch used by NaCl manifest files / scons placement
# of chromebinaries.  This is based on the machine "building" this extension.
# We also used this to identify the arch-specific different versions of
# this extension.
def GetBuildArch():
  arch = platform.machine()
  if arch in ('x86_64', 'amd64'):
    return 'x86-64'
  # TODO(jvoung): be more specific about the arm architecture version?
  if arch.startswith('armv7'):
    return 'arm'
  x86_32_re = re.compile('^i.86$')
  if x86_32_re.search(arch) or arch == 'x86_32' or arch == 'x86':
    return 'x86-32'

BUILD_ARCH = GetBuildArch()
ARCHES = ['x86-32', 'x86-64', 'arm']

def IsValidArch(arch):
  return arch in ARCHES

# The version of the arch used by configure and pnacl's build.sh.
def StandardArch(arch):
  return {'x86-32': 'i686',
          'x86-64': 'x86_64',
          'arm'   : 'armv7'}[arch]


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

# Normalize the platform name to be the way SCons finds chrome binaries.
# This is based on the platform "building" the extension.

def GetBuildPlatform():
  if sys.platform == 'darwin':
    platform = 'mac'
  elif sys.platform.startswith('linux'):
    platform = 'linux'
  elif sys.platform in ('cygwin', 'win32'):
    platform = 'windows'
  else:
    raise Exception('Unknown platform: %s' % sys.platform)
  return platform
BUILD_PLATFORM = GetBuildPlatform()

class CRXGen(object):
  """ Generate a CRX file. Can generate a fresh CRX and private key, or
  create a version of new CRX with the same AppID, using an existing
  private key.

  NOTE: We use the chrome binary to do CRX packing. There is also a bash
  script available at: http://code.google.com/chrome/extensions/crx.html
  but it is not featureful (doesn't know how to generate private keys).

  We should probably make a version of this that doesn't require chrome.
  """

  @staticmethod
  def ChromeBinaryName():
    if BUILD_PLATFORM == 'mac':
      return 'Chromium.app/Contents/MacOS/Chromium'
    elif BUILD_PLATFORM == 'windows':
      return 'chrome.exe'
    else:
      return 'chrome'

  @staticmethod
  def GetCRXGenPath():
    plat_arch = '%s_%s' % (BUILD_PLATFORM, BUILD_ARCH)
    return J(NACL_ROOT, 'chromebinaries', plat_arch, CRXGen.ChromeBinaryName())

  @staticmethod
  def RunCRXGen(manifest_dir, private_key=None):
    binary = CRXGen.GetCRXGenPath()
    if not os.path.isfile(binary):
      raise Exception('NaCl downloaded chrome binary not found: %s' % binary)
    cmdline = []
    if BUILD_PLATFORM == 'linux':
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
  # The extension system's manifest.json.
  manifest_template = J(package_base, 'pnacl_manifest_template.json')
  # Pnacl-specific info
  pnacl_template = J(package_base, 'pnacl_info_template.json')

  # Agreed-upon name for pnacl-specific info.
  pnacl_json = 'pnacl.json'

  @staticmethod
  def GenerateManifests(target_dir, version, arch, web_accessible,
                        all_host_permissions,
                        manifest_key=None):
    PnaclPackaging.GenerateExtensionManifest(target_dir, version,
                                             web_accessible,
                                             all_host_permissions,
                                             manifest_key)
    PnaclPackaging.GeneratePnaclInfo(target_dir, version, arch)

  @staticmethod
  def GenerateExtensionManifest(target_dir, version,
                                web_accessible, all_host_permissions,
                                manifest_key):
    manifest_template_fd = open(PnaclPackaging.manifest_template, 'r')
    manifest_template = manifest_template_fd.read()
    manifest_template_fd.close()
    output_fd = open(J(target_dir, 'manifest.json'), 'w')
    extra = ''
    if web_accessible != []:
      extra += '"web_accessible_resources": [\n%s],\n' % ',\n'.join(
          [ '    "%s"' % to_quote for to_quote in web_accessible ])
    if manifest_key is not None:
      extra += '  "key": "%s",\n' % manifest_key
    if all_host_permissions:
      extra += '  "permissions": ["http://*/"],\n'
    output_fd.write(manifest_template % { "version" : version,
                                          "extra" : extra, })
    output_fd.close()

  @staticmethod
  def GeneratePnaclInfo(target_dir, version, arch):
    pnacl_template_fd = open(PnaclPackaging.pnacl_template, 'r')
    pnacl_template = pnacl_template_fd.read()
    pnacl_template_fd.close()
    output_fd = open(J(target_dir, PnaclPackaging.pnacl_json), 'w')
    # For now, make the ABI version the same as pnacl-version.
    output_fd.write(pnacl_template % { "abi-version" : version,
                                       "arch" : arch, })
    output_fd.close()



######################################################################

class PnaclDirs(object):
  toolchain_dir = J(NACL_ROOT, 'toolchain')
  output_dir = J(toolchain_dir, 'pnacl-package')

  @staticmethod
  def TranslatorRoot():
    return J(PnaclDirs.toolchain_dir, 'pnacl_translator')

  @staticmethod
  def LibDir(target_arch):
    return J(PnaclDirs.TranslatorRoot(), 'lib-%s' % target_arch)

  @staticmethod
  def SandboxedCompilerDir(target_arch):
    return J(PnaclDirs.toolchain_dir,
             'pnacl_translator', StandardArch(target_arch), 'bin')

  @staticmethod
  def SetOutputDir(d):
    PnaclDirs.output_dir = d

  @staticmethod
  def OutputDir():
    return PnaclDirs.output_dir

  @staticmethod
  def OutputAllDir(version_quad):
    return J(PnaclDirs.OutputDir(), version_quad)

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
  for (root, dirs, files) in os.walk(base_dir, followlinks=True):
    for f in files:
      full_name = J(root, f)
      zipfile.write(full_name, os.path.relpath(full_name, base_dir))


def ListDirectoryRecursivelyAsURLs(base_dir):
  """ List all files that can be found from base_dir.  Return names as
  URLs relative to the base_dir.
  """
  file_list = []
  for (root, dirs, files) in os.walk(base_dir, followlinks=True):
    for f in files:
      full_name = J(root, f)
      if os.path.isfile(full_name):
        rel_name = os.path.relpath(full_name, base_dir)
        url = '/'.join(rel_name.split(os.path.sep))
        file_list.append(url)
  return file_list


def GetWebAccessibleResources(base_dir):
  ''' Return the default list of web_accessible_resources to allow us
  to do a CORS request to get extension files. '''
  resources = ListDirectoryRecursivelyAsURLs(base_dir)
  # Make sure that the pnacl.json file is accessible.
  resources.append(os.path.basename(PnaclPackaging.pnacl_json))
  return resources

def GeneratePrivateKey():
  """ Generate a dummy extension to generate a fresh private key. This will
  be left in the build dir, and the dummy extension will be cleaned up.
  """
  StepBanner('GEN PRIVATE KEY', 'Generating fresh private key')
  tempdir = tempfile.mkdtemp(dir=PnaclDirs.OutputDir())
  ext_dir = J(tempdir, 'dummy_extension')
  os.mkdir(ext_dir)
  PnaclPackaging.GenerateManifests(ext_dir,
                                   '0.0.0.0',
                                   'dummy_arch',
                                   [],
                                   False)
  CRXGen.RunCRXGen(ext_dir)
  shutil.copy2(J(tempdir, 'dummy_extension.pem'),
               PnaclDirs.OutputDir())
  shutil.rmtree(tempdir)
  print ('\n<<< Fresh key is now in %s/dummy_extension.pem >>>\n' %
         PnaclDirs.OutputDir())


def BuildArchCRXForComponentUpdater(version_quad, arch, lib_overrides, options):
  """ Build an architecture specific version for the chrome component
  install (an actual CRX, vs a zip file).  Though this is a CRX,
  it is not used as a chrome extension as the CWS and unpacked version.
  """
  parent_dir, target_dir = PnaclDirs.OutputArchDir(arch)

  StepBanner('BUILD ARCH CRX %s' % arch,
             'Packaging for arch %s in %s' % (arch, target_dir))

  # Copy llc and ld.
  copytree_existing(PnaclDirs.SandboxedCompilerDir(arch), target_dir)

  # Rename llc.nexe to llc, ld.nexe to ld
  for tool in ('llc', 'ld'):
    shutil.move(J(target_dir, '%s.nexe' % tool), J(target_dir, tool))

  # Copy native libraries.
  copytree_existing(PnaclDirs.LibDir(arch), target_dir)
  # Also copy files from the list of overrides.
  if arch in lib_overrides:
    for override in lib_overrides[arch]:
      print 'Copying override %s to %s' % (override, target_dir)
      shutil.copy2(override, target_dir)

  # Filter out native libraries related to glibc.
  patterns = ['*nonshared.a', '*.so', '*.so.*']
  for pat in patterns:
    for f in glob.glob(J(target_dir, pat)):
      print 'Filtering out glibc file: %s' % f
      os.remove(f)

  # Skip the CRX generation if we are only building the unpacked version
  # for commandline testing.
  if options.unpacked_only:
    return

  # Generate manifest one level up (to have layout look like the "all" package).
  # NOTE: this does not have 'web_accessible_resources' and does not have
  # the all_host_permissions, since it isn't used via chrome-extension://
  # URL requests.
  PnaclPackaging.GenerateManifests(parent_dir,
                                   version_quad,
                                   arch,
                                   [],
                                   False)
  CRXGen.RunCRXGen(parent_dir, options.prev_priv_key)


def LayoutAllDir(version_quad):
  StepBanner("Layout All Dir", "Copying Arch specific to Arch-independent.")
  target_dir = PnaclDirs.OutputAllDir(version_quad)
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
  target_dir = PnaclDirs.OutputAllDir(version_quad)

  web_accessible = GetWebAccessibleResources(target_dir)

  # Overwrite the arch-specific 'manifest.json' that was there.
  PnaclPackaging.GenerateManifests(target_dir,
                                   version_quad,
                                   'all',
                                   web_accessible,
                                   True)
  target_zip = J(PnaclDirs.OutputDir(), 'pnacl_all.zip')
  zipf = zipfile.ZipFile(target_zip, 'w', compression=zipfile.ZIP_DEFLATED)
  ZipDirectory(target_dir, zipf)
  zipf.close()


def BuildUnpacked(version_quad):
  """ Build an unpacked chrome extension with all files for commandline
  testing (load on chrome commandline).
  """
  StepBanner("UNPACKED CRX", "Making an unpacked CRX of all architectures.")

  target_dir = PnaclDirs.OutputAllDir(version_quad)
  web_accessible = GetWebAccessibleResources(target_dir)
  # Overwrite the manifest file (if there was one already).
  PnaclPackaging.GenerateManifests(target_dir,
                                   version_quad,
                                   'all',
                                   web_accessible,
                                   True,
                                   PnaclPackaging.WEBSTORE_PUBLIC_KEY)


def BuildAll(version_quad, lib_overrides, options):
  """ Package the pnacl components 3 ways.
  1) Arch-specific CRXes that can be queried by Omaha.
  2) A zip containing all arch files for the Chrome Webstore.
  3) An unpacked extension with all arch files for offline testing.
  """
  StepBanner("BUILD_ALL", "Packaging for version: %s" % version_quad)
  for arch in ARCHES:
    BuildArchCRXForComponentUpdater(version_quad, arch, lib_overrides, options)
  LayoutAllDir(version_quad)
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
  parser.add_option('-d', '--dest', dest='dest',
                    help='The destination root for laying out the extension')
  parser.add_option('-p', '--priv_key',
                    dest='prev_priv_key', default=None,
                    help='Specify the old private key')
  parser.add_option('-L', '--lib_override',
                    dest='lib_overrides', action='append', default=[],
                    help='Specify path to a fresher native library ' +
                    'that overrides the tarball library with ' +
                    '(arch:libfile) tuple.')
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
    arch, override_lib = o.split(',')
    if not IsValidArch(arch):
      raise Exception('Unknown arch for -L: %s (from %s)' % (arch, o))
    if not os.path.isfile(override_lib):
      raise Exception('Override native lib not a file for -L: %s (from %s)' %
                      (override_lib, o))
    override_list = lib_overrides.get(arch, [])
    override_list.append(override_lib)
    lib_overrides[arch] = override_list

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
