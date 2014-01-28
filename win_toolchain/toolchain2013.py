#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Extracts a Windows VS2013 toolchain from various downloadable pieces."""


import ctypes
import optparse
import os
import shutil
import subprocess
import sys
import tempfile
import urllib2


BASEDIR = os.path.dirname(os.path.abspath(__file__))
g_temp_dirs = []


sys.path.append(os.path.join(BASEDIR, '..'))
import download_from_google_storage


def GetLongPathName(path):
  """Converts any 8dot3 names in the path to the full name."""
  buf = ctypes.create_unicode_buffer(260)
  size = ctypes.windll.kernel32.GetLongPathNameW(unicode(path), buf, 260)
  if (size > 260):
    sys.exit('Long form of path longer than 260 chars: %s' % path)
  return buf.value


def RunOrDie(command):
  subprocess.check_call(command, shell=True)


def TempDir():
  """Generates a temporary directory (for downloading or extracting to) and keep
  track of the directory that's created for cleaning up later.
  """
  temp = tempfile.mkdtemp()
  g_temp_dirs.append(temp)
  return temp


def DeleteAllTempDirs():
  """Removes all temporary directories created by |TempDir()|."""
  global g_temp_dirs
  if g_temp_dirs:
    sys.stdout.write('Cleaning up temporaries...\n')
  for temp in g_temp_dirs:
    # shutil.rmtree errors out on read only attributes.
    RunOrDie('rmdir /s/q "%s"' % temp)
  g_temp_dirs = []


def GetIsoUrl(pro):
  """Gets the .iso URL.

  If |pro| is False, downloads the Express edition. If |CHROME_HEADLESS| is
  set in the environment, then we assume we're on an internal bot, and download
  from internal google storage instead.
  """
  prefix = 'http://download.microsoft.com/download/'
  if pro:
    return (prefix +
        'A/F/1/AF128362-A6A8-4DB3-A39A-C348086472CC/VS2013_RTM_PRO_ENU.iso')
  else:
    return (prefix +
        '7/2/E/72E0F986-D247-4289-B9DC-C4FB07374894/VS2013_RTM_DskExp_ENU.iso')


def Download(url, local_path):
  """Downloads a large-ish binary file and print some status information while
  doing so.
  """
  sys.stdout.write('Downloading %s...\n' % url)
  req = urllib2.urlopen(url)
  content_length = int(req.headers.get('Content-Length', 0))
  bytes_read = 0L
  terminator = '\r' if sys.stdout.isatty() else '\n'
  with open(local_path, 'wb') as file_handle:
    while True:
      chunk = req.read(1024 * 1024)
      if not chunk:
        break
      bytes_read += len(chunk)
      file_handle.write(chunk)
      sys.stdout.write('... %d/%d%s' % (bytes_read, content_length, terminator))
      sys.stdout.flush()
  sys.stdout.write('\n')
  if content_length and content_length != bytes_read:
    raise SystemExit('Got incorrect number of bytes downloading %s' % url)


def ExtractIso(iso_path):
  """Uses 7zip to extract the contents of the given .iso (or self-extracting
  .exe).
  """
  target_path = TempDir()
  sys.stdout.write('Extracting %s...\n' % iso_path)
  sys.stdout.flush()
  # TODO(scottmg): Do this (and exe) manually with python code.
  # Note that at the beginning of main() we set the working directory to 7z's
  # location so that 7z can find its codec dll.
  RunOrDie('7z x "%s" -y "-o%s" >nul' % (iso_path, target_path))
  return target_path


def ExtractMsi(msi_path):
  """Uses msiexec to extract the contents of the given .msi file."""
  sys.stdout.write('Extracting %s...\n' % msi_path)
  target_path = TempDir()
  RunOrDie('msiexec /a "%s" /qn TARGETDIR="%s"' % (msi_path, target_path))
  return target_path


def DownloadMainIso(url):
  temp_dir = TempDir()
  target_path = os.path.join(temp_dir, os.path.basename(url))
  Download(url, target_path)
  return target_path


def DownloadSDK8():
  """Downloads the Win8 SDK.

  This one is slightly different than the simpler direct downloads. There is
  no .ISO distribution for the Windows 8 SDK. Rather, a tool is provided that
  is a download manager. This is used to download the various .msi files to a
  target location. Unfortunately, this tool requires elevation for no obvious
  reason even when only downloading, so this function will trigger a UAC
  elevation if the script is not run from an elevated prompt. This is mostly
  grabbed for windbg and cdb (See http://crbug.com/321187) as most of the SDK
  is in VS2013, however we need a couple D3D related things from the SDK.
  """
  # Use the long path name here because because 8dot3 names don't seem to work.
  sdk_temp_dir = GetLongPathName(TempDir())
  target_path = os.path.join(sdk_temp_dir, 'sdksetup.exe')
  standalone_path = os.path.join(sdk_temp_dir, 'Standalone')
  Download(
      ('http://download.microsoft.com/download/'
       'F/1/3/F1300C9C-A120-4341-90DF-8A52509B23AC/standalonesdk/sdksetup.exe'),
      target_path)
  sys.stdout.write(
      'Running sdksetup.exe to download Win8 SDK (may request elevation)...\n')
  count = 0
  while count < 5:
    rc = os.system(target_path + ' /quiet '
                   '/features OptionId.WindowsDesktopDebuggers '
                   '/layout ' + standalone_path)
    if rc == 0:
      return standalone_path
    count += 1
    sys.stdout.write('Windows 8 SDK failed to download, retrying.\n')
  raise SystemExit("After multiple retries, couldn't download Win8 SDK")


def DownloadUsingGsutil(filename):
  """Downloads the given file from Google Storage chrome-wintoolchain bucket."""
  temp_dir = TempDir()
  assert os.path.basename(filename) == filename
  target_path = os.path.join(temp_dir, filename)
  gsutil = download_from_google_storage.Gsutil(
      download_from_google_storage.GSUTIL_DEFAULT_PATH, boto_path=None)
  code = gsutil.call('cp', 'gs://chrome-wintoolchain/' + filename, target_path)
  if code != 0:
    raise SystemExit('gsutil failed')
  return target_path


def GetVSInternal():
  """Uses gsutil to pull the toolchain from internal Google Storage bucket."""
  return DownloadUsingGsutil('VS2013_RTM_PRO_ENU.iso')


def GetSDKInternal():
  """Downloads a zipped copy of the SDK from internal Google Storage bucket,
  and extracts it."""
  zip_file = DownloadUsingGsutil('Standalone.zip')
  return ExtractIso(zip_file)


class SourceImages(object):
  def __init__(self, vs_path, sdk8_path):
    self.vs_path = vs_path
    self.sdk8_path = sdk8_path


def GetSourceImages(local_dir, pro):
  url = GetIsoUrl(pro)
  if os.environ.get('CHROME_HEADLESS'):
    return SourceImages(GetVSInternal(), GetSDKInternal())
  elif local_dir:
    return SourceImages(os.path.join(local_dir, os.path.basename(url)),
                        os.path.join(local_dir, 'Standalone'))
  else:
    # Note that we do the SDK first, as it might cause an elevation prompt.
    sdk8_path = DownloadSDK8()
    vs_path = DownloadMainIso(url)
    return SourceImages(vs_path, sdk8_path)


def ExtractMsiList(root_dir, packages):
  """Extracts the contents of a list of .msi files from an already extracted
  .iso file.

  |packages| is a list of pairs (msi, required). If required is not True, the
  msi is optional (this is set for packages that are in Pro but not Express).
  """
  results = []
  for (package, required) in packages:
    path_to_package = os.path.join(root_dir, package)
    if not os.path.exists(path_to_package) and not required:
      continue
    results.append(ExtractMsi(path_to_package))
  return results


def ExtractComponents(image):
  vs_packages = [
      (r'vcRuntimeAdditional_amd64\vc_runtimeAdditional_x64.msi', True),
      (r'vcRuntimeAdditional_x86\vc_runtimeAdditional_x86.msi', True),
      (r'vcRuntimeDebug_amd64\vc_runtimeDebug_x64.msi', True),
      (r'vcRuntimeDebug_x86\vc_runtimeDebug_x86.msi', True),
      (r'vcRuntimeMinimum_amd64\vc_runtimeMinimum_x64.msi', True),
      (r'vcRuntimeMinimum_x86\vc_runtimeMinimum_x86.msi', True),
      (r'vc_compilerCore86\vc_compilerCore86.msi', True),
      (r'vc_compilerCore86res\vc_compilerCore86res.msi', True),
      (r'vc_compilerx64nat\vc_compilerx64nat.msi', False),
      (r'vc_compilerx64natres\vc_compilerx64natres.msi', False),
      (r'vc_compilerx64x86\vc_compilerx64x86.msi', False),
      (r'vc_compilerx64x86res\vc_compilerx64x86res.msi', False),
      (r'vc_librarycore86\vc_librarycore86.msi', True),
      (r'vc_libraryDesktop\x64\vc_LibraryDesktopX64.msi', True),
      (r'vc_libraryDesktop\x86\vc_LibraryDesktopX86.msi', True),
      (r'vc_libraryextended\vc_libraryextended.msi', False),
      (r'Windows_SDK\Windows Software Development Kit-x86_en-us.msi', True),
      ('Windows_SDK\\'
       r'Windows Software Development Kit for Metro style Apps-x86_en-us.msi',
          True),
    ]
  extracted_iso = ExtractIso(image.vs_path)
  result = ExtractMsiList(os.path.join(extracted_iso, 'packages'), vs_packages)

  sdk_packages = [
      (r'X86 Debuggers And Tools-x86_en-us.msi', True),
      (r'X64 Debuggers And Tools-x64_en-us.msi', True),
      (r'SDK Debuggers-x86_en-us.msi', True),
    ]
  result.extend(ExtractMsiList(os.path.join(image.sdk8_path, 'Installers'),
                               sdk_packages))

  return result


def CopyToFinalLocation(extracted_dirs, target_dir):
  sys.stdout.write('Copying to final location...\n')
  mappings = {
      'Program Files\\Microsoft Visual Studio 12.0\\': '.\\',
      'System64\\': 'sys64\\',
      'System\\': 'sys32\\',
      'Windows Kits\\8.0\\': 'win8sdk\\',
  }
  matches = []
  for extracted_dir in extracted_dirs:
    for root, _, filenames in os.walk(extracted_dir):
      for filename in filenames:
        matches.append((extracted_dir, os.path.join(root, filename)))

  copies = []
  for prefix, full_path in matches:
    # +1 for trailing \.
    partial_path = full_path[len(prefix) + 1:]
    for map_from, map_to in mappings.iteritems():
      if partial_path.startswith(map_from):
        target_path = os.path.join(map_to, partial_path[len(map_from):])
        copies.append((full_path, os.path.join(target_dir, target_path)))

  for full_source, full_target in copies:
    target_dir = os.path.dirname(full_target)
    if not os.path.isdir(target_dir):
      os.makedirs(target_dir)
    shutil.copy2(full_source, full_target)


def GenerateSetEnvCmd(target_dir, pro):
  """Generate a batch file that gyp expects to exist to set up the compiler
  environment.

  This is normally generated by a full install of the SDK, but we
  do it here manually since we do not do a full install."""
  with open(os.path.join(
        target_dir, r'win8sdk\bin\SetEnv.cmd'), 'w') as f:
    f.write('@echo off\n'
            ':: Generated by win_toolchain\\toolchain2013.py.\n'
            # Common to x86 and x64
            'set PATH=%~dp0..\\..\\Common7\\IDE;%PATH%\n'
            'set INCLUDE=%~dp0..\\..\\win8sdk\\Include\\um;'
               '%~dp0..\\..\\win8sdk\\Include\\shared;'
               '%~dp0..\\..\\VC\\include;'
               '%~dp0..\\..\\VC\\atlmfc\\include\n'
            'if "%1"=="/x64" goto x64\n')

    # x86. If we're Pro, then use the amd64_x86 cross (we don't support x86
    # host at all).
    if pro:
      f.write('set PATH=%~dp0..\\..\\win8sdk\\bin\\x86;'
                '%~dp0..\\..\\VC\\bin\\amd64_x86;'
                '%~dp0..\\..\\VC\\bin\\amd64;'  # Needed for mspdb120.dll.
                '%PATH%\n')
    else:
      f.write('set PATH=%~dp0..\\..\\win8sdk\\bin\\x86;'
                '%~dp0..\\..\\VC\\bin;%PATH%\n')
    f.write('set LIB=%~dp0..\\..\\VC\\lib;'
               '%~dp0..\\..\\win8sdk\\Lib\\win8\\um\\x86;'
               '%~dp0..\\..\\VC\\atlmfc\\lib\n'
            'goto :EOF\n')

    # Express does not include a native 64 bit compiler, so we have to use
    # the x86->x64 cross.
    if not pro:
      # x86->x64 cross.
      f.write(':x64\n'
              'set PATH=%~dp0..\\..\\win8sdk\\bin\\x64;'
                 '%~dp0..\\..\\VC\\bin\\x86_amd64;'
                 '%PATH%\n')
    else:
      # x64 native.
      f.write(':x64\n'
              'set PATH=%~dp0..\\..\\win8sdk\\bin\\x64;'
                 '%~dp0..\\..\\VC\\bin\\amd64;'
                 '%PATH%\n')
    f.write('set LIB=%~dp0..\\..\\VC\\lib\\amd64;'
               '%~dp0..\\..\\win8sdk\\Lib\\win8\\um\\x64;'
               '%~dp0..\\..\\VC\\atlmfc\\lib\\amd64\n')


def main():
  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  parser.add_option('--targetdir', metavar='DIR',
                    help='put toolchain into DIR',
                    default=os.path.join(BASEDIR, 'win_toolchain_2013'))
  parser.add_option('--noclean', action='store_false', dest='clean',
                    help='do not remove temp files',
                    default=True)
  parser.add_option('--local', metavar='DIR',
                    help='use downloaded files from DIR')
  parser.add_option('--express',
                    help='use VS Express instead of Pro', action='store_true')
  options, _ = parser.parse_args()
  try:
    target_dir = os.path.abspath(options.targetdir)
    if os.path.exists(target_dir):
      parser.error('%s already exists. Please [re]move it or use '
                   '--targetdir to select a different target.\n' %
                   target_dir)
    # Set the working directory to 7z subdirectory. 7-zip doesn't find its
    # codec dll very well, so this is the simplest way to make sure it runs
    # correctly, as we don't otherwise care about working directory.
    os.chdir(os.path.join(BASEDIR, '7z'))
    images = GetSourceImages(options.local, not options.express)
    extracted = ExtractComponents(images)
    CopyToFinalLocation(extracted, target_dir)

    GenerateSetEnvCmd(target_dir, not options.express)
  finally:
    if options.clean:
      DeleteAllTempDirs()


if __name__ == '__main__':
  sys.exit(main())
