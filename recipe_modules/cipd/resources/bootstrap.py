# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import errno
import hashlib
import json
import os
import sys
import tempfile
import time
import traceback
import urllib
import urllib2


# Default package repository URL.
CIPD_BACKEND_URL = 'https://chrome-infra-packages.appspot.com'

# ./cipd resolve \
#     infra/tools/cipd/ \
#     -version=git_revision:768586a9aa72fe6a41a8a0205ed56ceb1495625c
CLIENT_VERSIONS = {
  'linux-386': '3f5d6d3906c3b2c6a057d1ab1634ac06aa418708',
  'linux-amd64': '1ea0b6b254ad3f546c826dd3e437798ace2c2480',
  'mac-amd64': 'c111be343c692e5285113a6b1c999887adbb268e',
  'windows-386': 'dc3d1bd5b4b93945640bac4bb047c333a8fa12fd',
  'windows-amd64': 'bccdb9a605037e3dd2a8a64e79e08f691a6f159d',
}


class CipdBootstrapError(Exception):
  """Raised by install_cipd_client on fatal error."""

def install_cipd_client(path, package, version):
  """Installs CIPD client to <path>/cipd.

  Args:
    path: root directory to install CIPD client into.
    package: cipd client package name, e.g. infra/tools/cipd/linux-amd64.
    version: version of the package to install.

  Returns:
    Absolute path to CIPD executable.
  """
  print 'Ensuring CIPD client is up-to-date'
  version_file = os.path.join(path, 'VERSION')
  bin_file = os.path.join(path, 'cipd')

  # Resolve version to concrete instance ID, e.g "live" -> "abcdef0123....".
  instance_id = call_cipd_api(
      'repo/v1/instance/resolve',
      {'package_name': package, 'version': version})['instance_id']
  print 'CIPD client %s => %s' % (version, instance_id)

  # Already installed?
  installed_instance_id = (read_file(version_file) or '').strip()
  if installed_instance_id == instance_id and os.path.exists(bin_file):
    return bin_file, instance_id

  # Resolve instance ID to an URL to fetch client binary from.
  client_info = call_cipd_api(
      'repo/v1/client',
      {'package_name': package, 'instance_id': instance_id})
  print 'CIPD client binary info:\n%s' % dump_json(client_info)

  # Fetch the client. It is ~10 MB, so don't bother and fetch it into memory.
  status, raw_client_bin = fetch_url(client_info['client_binary']['fetch_url'])
  if status != 200:
    print 'Failed to fetch client binary, HTTP %d' % status
    raise CipdBootstrapError('Failed to fetch client binary, HTTP %d' % status)
  digest = hashlib.sha1(raw_client_bin).hexdigest()
  if digest != client_info['client_binary']['sha1']:
    raise CipdBootstrapError('Client SHA1 mismatch')

  # Success.
  print 'Fetched CIPD client %s:%s at %s' % (package, instance_id, bin_file)
  write_file(bin_file, raw_client_bin)
  os.chmod(bin_file, 0755)
  write_file(version_file, instance_id + '\n')
  return bin_file, instance_id


def call_cipd_api(endpoint, query):
  """Sends GET request to CIPD backend, parses JSON response."""
  url = '%s/_ah/api/%s' % (CIPD_BACKEND_URL, endpoint)
  if query:
    url += '?' + urllib.urlencode(query)
  status, body = fetch_url(url)
  if status != 200:
    raise CipdBootstrapError('Server replied with HTTP %d' % status)
  try:
    body = json.loads(body)
  except ValueError:
    raise CipdBootstrapError('Server returned invalid JSON')
  status = body.get('status')
  if status != 'SUCCESS':
    m = body.get('error_message') or '<no error message>'
    raise CipdBootstrapError('Server replied with error %s: %s' % (status, m))
  return body


def fetch_url(url, headers=None):
  """Sends GET request (with retries).

  Args:
    url: URL to fetch.
    headers: dict with request headers.

  Returns:
    (200, reply body) on success.
    (HTTP code, None) on HTTP 401, 403, or 404 reply.

  Raises:
    Whatever urllib2 raises.
  """
  req = urllib2.Request(url)
  req.add_header('User-Agent', 'cipd recipe bootstrap.py')
  for k, v in (headers or {}).iteritems():
    req.add_header(str(k), str(v))
  i = 0
  while True:
    i += 1
    try:
      print 'GET %s' % url
      return 200, urllib2.urlopen(req, timeout=60).read()
    except Exception as e:
      if isinstance(e, urllib2.HTTPError):
        print 'Failed to fetch %s, server returned HTTP %d' % (url, e.code)
        if e.code in (401, 403, 404):
          return e.code, None
      else:
        print 'Failed to fetch %s' % url
      if i == 20:
        raise
    print 'Retrying in %d sec.' % i
    time.sleep(i)


def ensure_directory(path):
  """Creates a directory."""
  # Handle a case where a file is being converted into a directory.
  chunks = path.split(os.sep)
  for i in xrange(len(chunks)):
    p = os.sep.join(chunks[:i+1])
    if os.path.exists(p) and not os.path.isdir(p):
      os.remove(p)
      break
  try:
    os.makedirs(path)
  except OSError as e:
    if e.errno != errno.EEXIST:
      raise


def read_file(path):
  """Returns contents of a file or None if missing."""
  try:
    with open(path, 'r') as f:
      return f.read()
  except IOError as e:
    if e.errno == errno.ENOENT:
      return None
    raise


def write_file(path, data):
  """Puts a file on disk, atomically."""
  ensure_directory(os.path.dirname(path))
  fd, temp_file = tempfile.mkstemp(dir=os.path.dirname(path))
  with os.fdopen(fd, 'w') as f:
    f.write(data)
  if not sys.platform in ('linux2', 'darwin'):
    # On windows we should remove destination file if it exists.
    if os.path.exists(path):
      os.remove(path)
    # At this point we may crash, and it's OK, as next time we'll just
    # re-install CIPD from scratch.
  os.rename(temp_file, path)


def dump_json(obj):
  """Pretty-formats object to JSON."""
  return json.dumps(obj, indent=2, sort_keys=True, separators=(',',':'))


def main(args):
  parser = argparse.ArgumentParser('bootstrap cipd')
  parser.add_argument('--json-output', default=None)
  parser.add_argument('--version', default=None)
  parser.add_argument('--platform', required=True)
  parser.add_argument('--dest-directory', required=True)
  opts = parser.parse_args(args)

  package = "infra/tools/cipd/%s" % opts.platform
  version = opts.version or CLIENT_VERSIONS[opts.platform]

  try:
    exe_path, instance_id = install_cipd_client(opts.dest_directory,
                                                package, version)
    result = {
      'executable': exe_path,
      'instance_id': instance_id
    }
    if opts.json_output:
      with open(opts.json_output, 'w') as f:
        json.dump(result, f)
  except Exception as e:
    print 'Exception installing cipd: %s' % e
    _exc_type, _exc_value, exc_traceback = sys.exc_info()
    traceback.print_tb(exc_traceback)
    return 1

  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
