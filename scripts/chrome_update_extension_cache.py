#!/usr/bin/python
# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate and upload tarballs for default apps cache.

Run inside the 'files' dir containing 'external_extensions.json' file:
$ chromite/bin/chrome_update_extension_cache --create --upload \\
    chromeos-default-apps-1.0.0

Always increment the version when you update an existing package.
If no new files are added, increment the third version number.
  e.g. 1.0.0 -> 1.0.1
If you change list of default extensions, increment the second version number.
  e.g. 1.0.0 -> 1.1.0

Also you need to regenerate the Manifest with the new tarball digest.
Run inside the chroot:
$ ebuild chromeos-default-apps-1.0.0.ebuild manifest --force
"""

import json
import os
import urllib
import xml.dom.minidom

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import gs
from chromite.lib import osutils


EXTENSIONS_CACHE_PREFIX = '/usr/share/google-chrome/extensions'
UPLOAD_URL_BASE = 'gs://chromeos-localmirror-private/distfiles'


def DownloadCrx(ext, extension, outputdir):
  """Download .crx file from WebStore and update entry."""
  cros_build_lib.Info('Extension "%s"(%s)...', extension['name'], ext)

  update_url = '%s?x=id%%3D%s%%26uc' % (extension['external_update_url'], ext)
  response = urllib.urlopen(update_url)
  if response.getcode() != 200:
    cros_build_lib.Error('Cannot get update response, URL: %s, error: %d',
                         update_url, response.getcode())
    return False

  dom = xml.dom.minidom.parse(response)
  status = dom.getElementsByTagName('app')[0].getAttribute('status')
  if status != 'ok':
    cros_build_lib.Error('Cannot fetch extension, status: %s', status)
    return False

  node = dom.getElementsByTagName('updatecheck')[0]
  url = node.getAttribute('codebase')
  version = node.getAttribute('version')
  filename = '%s-%s.crx' % (ext, version)
  response = urllib.urlopen(url)
  if response.getcode() != 200:
    cros_build_lib.Error('Cannot download extension, URL: %s, error: %d',
                         url, response.getcode())
    return False

  osutils.WriteFile(os.path.join(outputdir, 'extensions', filename),
                    response.read())

  # Has to delete because only one of 'external_crx' or
  # 'external_update_url' should present for the extension.
  del extension['external_update_url']

  extension['external_crx'] = os.path.join(EXTENSIONS_CACHE_PREFIX, filename)
  extension['external_version'] = version

  cros_build_lib.Info('Downloaded, current version %s', version)
  return True


def CreateCacheTarball(extensions, outputdir, tarball):
  """Cache |extensions| in |outputdir| and pack them in |tarball|."""
  osutils.SafeMakedirs(os.path.join(outputdir, 'extensions', 'managed_users'))
  was_errors = False
  for ext in extensions:
    managed_users = extensions[ext].get('managed_users', 'no')
    cache_crx = extensions[ext].get('cache_crx', 'yes')

    # Remove fields that shouldn't be in the output file.
    for key in ('cache_crx', 'managed_users'):
      extensions[ext].pop(key, None)

    if cache_crx == 'yes':
      if not DownloadCrx(ext, extensions[ext], outputdir):
        was_errors = True
    elif cache_crx == 'no':
      pass
    else:
      cros_build_lib.Die('Unknown value for "cache_crx" %s for %s',
                         cache_crx, ext)

    if managed_users == 'yes':
      json_file = os.path.join(outputdir,
          'extensions/managed_users/%s.json' % ext)
      json.dump(extensions[ext],
                open(json_file, 'w'),
                sort_keys=True,
                indent=2,
                separators=(',', ': '))

    if managed_users != 'only':
      json_file = os.path.join(outputdir, 'extensions/%s.json' % ext)
      json.dump(extensions[ext],
                open(json_file, 'w'),
                sort_keys=True,
                indent=2,
                separators=(',', ': '))

  if was_errors:
    cros_build_lib.Die('FAIL to download some extensions')

  cros_build_lib.CreateTarball(tarball, outputdir)
  cros_build_lib.Info('Tarball created %s', tarball)


def main(argv):
  parser = commandline.ArgumentParser(
      '%%(prog)s [options] <version>\n\n%s' % __doc__)
  parser.add_argument('version', nargs=1)
  parser.add_argument('--path', default=None, type='path',
                      help='Path of files dir with external_extensions.json')
  parser.add_argument('--create', default=False, action='store_true',
                      help='Create cache tarball with specified name')
  parser.add_argument('--upload', default=False, action='store_true',
                      help='Upload cache tarball with specified name')
  options = parser.parse_args(argv)

  if options.path:
    os.chdir(options.path)

  if not (options.create or options.upload):
    cros_build_lib.Die('Need at least --create or --upload args')

  if not os.path.exists('external_extensions.json'):
    cros_build_lib.Die('No external_extensions.json in %s. Did you forget the '
                       '--path option?', os.getcwd())

  tarball = '%s.tar.xz' % options.version[0]
  if options.create:
    extensions = json.load(open('external_extensions.json', 'r'))
    with osutils.TempDir() as tempdir:
      CreateCacheTarball(extensions, tempdir, os.path.abspath(tarball))

  if options.upload:
    ctx = gs.GSContext()
    url = os.path.join(UPLOAD_URL_BASE, tarball)
    if ctx.Exists(url):
      cros_build_lib.Die('This version already exists on Google Storage (%s)!\n'
                         'NEVER REWRITE EXISTING FILE. IT WILL BREAK CHROME OS '
                         'BUILD!!!', url)
    ctx.Copy(os.path.abspath(tarball), url, acl='project-private')
    cros_build_lib.Info('Tarball uploaded %s', url)
    osutils.SafeUnlink(os.path.abspath(tarball))
