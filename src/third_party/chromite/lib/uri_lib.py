# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Various URI related helpers."""

from __future__ import print_function

import os
import re

from six.moves import urllib


def _ExtractGobClAndSubpath(o):
  """Return a (CL, subpath) tuple if found in the URI parts, or None if not.

  Many forms are supported below.
  """
  path = o.path
  fragment = o.fragment

  # https://chromium-review.googlesource.com/#/662618
  # https://chromium-review.googlesource.com/#/662618/
  # https://chromium-review.googlesource.com/#/662618/////
  if path == '/':
    m = re.match(r'^/(?:c/)?([0-9]+)/*$', fragment)
    if m:
      return (m.group(1), '')

    # Any valid /c/ URI can also be in the fragment.
    if fragment.startswith('/c/'):
      # https://chrome-internal-review.googlesource.com/#/c/280497
      # https://chrome-internal-review.googlesource.com/#/c/280497/
      path = fragment
      fragment = ''
    else:
      return None

  # https://chromium-review.googlesource.com/662618
  # https://chromium-review.googlesource.com/662618/
  m = re.match(r'^/([0-9]+)/?$', path)
  if m:
    return (m.group(1), fragment)

  # https://chromium-review.googlesource.com/c/662618
  # https://chromium-review.googlesource.com/c/662618/
  # https://chromium-review.googlesource.com/c/662618/1/lib/gs.py
  m = re.match(r'^/c/([0-9]+)(/.*)?$', path)
  if m:
    return (m.group(1) + (m.group(2) or '').rstrip('/'), fragment)

  # https://chromium-review.googlesource.com/c/chromiumos/chromite/+/662618
  # https://chromium-review.googlesource.com/c/chromiumos/chromite/+/662618/
  # https://chromium-review.googlesource.com/c/chromiumos/chromite/+/662618/1/lib/gs.py
  m = re.match(r'^/c/.*/\+/([0-9]+)(/.*)?$', path)
  if m:
    return (m.group(1) + (m.group(2) or '').rstrip('/'), fragment)

  return None


def _ShortenGob(o):
  """Shorten a Gerrit-on-Borg URI.

  Args:
    o: The named tuple from a urllib.parse.urlsplit call.

  Returns:
    A new named tuple that can be passed to urllib.parse.urlunsplit.
  """
  parts = _ExtractGobClAndSubpath(o)
  if parts is None:
    return o

  # If we're using a Chromium GoB, use crrev.com.  Otherwise, we'll just
  # shorten the path but keep the original GoB host.
  if o.netloc.split('.')[0] in ('chromium-review', 'chrome-internal-review'):
    netloc = 'crrev.com'
    if o.netloc.startswith('chrome-internal'):
      sub = 'i'
    else:
      sub = 'c'
    path = '/%s/%s' % (sub, parts[0])
  else:
    netloc = o.netloc
    path = '/%s' % (parts[0],)

  return o._replace(scheme='https', netloc=netloc, path=path, fragment=parts[1])


def _ShortenCrosReview(o):
  """Shorten old review to new review hosts.

  Args:
    o: The named tuple from a urllib.parse.urlsplit call.

  Returns:
    A new named tuple that can be passed to urllib.parse.urlunsplit.
  """
  m = re.match(r'^/(i/)?([0-9]+)', o.path)
  if m:
    subpart = 'i' if m.group(1) else 'c'
    o = o._replace(netloc='crrev.com',
                   path='/%s/%s' % (subpart, m.group(2)))

  return o._replace(scheme='https')


def _ShortenRietveld(o):
  """Shorten a rietveld URI.

  Args:
    o: The named tuple from a urllib.parse.urlsplit call.

  Returns:
    A new named tuple that can be passed to urllib.parse.urlunsplit.
  """
  m = re.match(r'^/([0-9]+)', o.path)
  if m:
    o = o._replace(netloc='crrev.com',
                   path='/%s' % (m.group(1),))

  return o._replace(scheme='https')


def _ShortenBuganizer(o):
  """Shorten a buganizer URI.

  Args:
    o: The named tuple from a urllib.parse.urlsplit call.

  Returns:
    A new named tuple that can be passed to urllib.parse.urlunsplit.
  """
  if o.path == '/issue':
    # http://b.corp.google.com/issue?id=123
    qs = urllib.parse.parse_qs(o.query)
    if 'id' in qs:
      o = o._replace(path='/%s' % (qs['id'][0],), query='')
  elif o.path.startswith('/issues/'):
    # http://b.corp.google.com/issues/123
    o = o._replace(path=o.path[len('/issues'):])

  return o._replace(scheme='http', netloc='b')


def _ShortenChromiumBug(o):
  """Shorten a Chromium bug URI.

  Args:
    o: The named tuple from a urllib.parse.urlsplit call.

  Returns:
    A new named tuple that can be passed to urllib.parse.urlunsplit.
  """
  # https://bugs.chromium.org/p/chromium/issues/detail?id=123
  # https://bugs.chromium.org/p/google-breakpad/issues/list
  # These we don't actually shorten:
  # https://bugs.chromium.org/p/chromium/people
  m = re.match(r'/p/([^/]+)/issues(/detail)?', o.path)
  if m is None:
    return o

  if m.group(1) == 'chromium':
    path = ''
  else:
    path = '/%s' % (m.group(1),)

  if m.group(2):
    qs = urllib.parse.parse_qs(o.query)
    if 'id' in qs:
      path += '/%s' % (qs['id'][0],)
    o = o._replace(query='')

  return o._replace(scheme='https', netloc='crbug.com', path=path)


def _ShortenGutsTicket(o):
  """Shorten a Google GUTS ticket URI.

  Args:
    o: The named tuple from a urllib.parse.urlsplit call.

  Returns:
    A new named tuple that can be passed to urllib.parse.urlunsplit.
  """
  # https://gutsv3.corp.google.com/#ticket/123
  m = re.match(r'^ticket/([0-9]+)', o.fragment)
  if m:
    o = o._replace(path='/%s' % (m.group(1),), fragment='')

  return o._replace(scheme='http', netloc='t')


# Map sites to the shortener.  Use a tuple to keep rule ordering sane (even if
# we don't have rules atm relying on it).
_SHORTENERS = (
    (re.compile(r'^[a-z0-9-]+-review\.googlesource\.com$'), _ShortenGob),
    (re.compile(r'^crosreview\.com$'), _ShortenCrosReview),
    (re.compile(r'^codereview\.chromium\.org$'), _ShortenRietveld),
    (re.compile(r'^b\.corp\.google\.com$'), _ShortenBuganizer),
    (re.compile(r'^bugs\.chromium\.org$'), _ShortenChromiumBug),
    (re.compile(r'^gutsv\d\.corp\.google\.com$'), _ShortenGutsTicket),
)


def ShortenUri(uri, omit_scheme=False):
  """Attempt to shorten a URI for humans.

  If the URI can't be shortened, then we just return the original value.
  Thus this can be safely used as a pass-through for anything.

  Args:
    uri: Any valid URI.
    omit_scheme: Whether to include the scheme (e.g. http:// or https://).

  Returns:
    A (hopefully shorter) URI pointing to the same resource as |uri|.
  """
  o = urllib.parse.urlsplit(uri)

  # If the scheme & host are empty, assume it's because the URI we were given
  # lacked a http:// or https:// prefix, so blindly insert a http://.
  if not o.scheme and not o.netloc:
    o = urllib.parse.urlsplit('http://%s' % (uri,))

  for matcher, shortener in _SHORTENERS:
    if matcher.match(o.netloc):
      o = shortener(o)
      break
  else:
    return uri

  if omit_scheme:
    o = o._replace(scheme='')
    # Strip off the leading // due to blank scheme.
    return urllib.parse.urlunsplit(o)[2:]
  else:
    return urllib.parse.urlunsplit(o)


_LUCI_MILO_BUILDBOT_URL = 'https://luci-milo.appspot.com/buildbot'

_LOGDOG_URL = ('https://luci-logdog.appspot.com/v/'
               '?s=chromeos/buildbucket/cr-buildbucket.appspot.com/'
               '%s/%%2B/steps/%s/0/stdout')

# Will redirect:
#   https://ci.chromium.org/b/8914470887449121184
# to:
#   https://ci.chromium.org/p/chromeos/builds/b8914470887449121184
_MILO_BUILD_URL = 'https://ci.chromium.org/b/%(buildbucket_id)s'


def ConstructMiloBuildUri(buildbucket_id):
  """Return a Milo build URL.

  Args:
    buildbucket_id: Buildbucket id of the build to link.

  Returns:
    The fully formed URI.
  """
  # Only local tryjobs will not have a buildbucket_id but they also do not have
  # a web UI to point at. Generate a fake URL.
  buildbucket_id = buildbucket_id or 'fake_bb_id'
  return _MILO_BUILD_URL % {'buildbucket_id': buildbucket_id}


def ConstructDashboardUri(buildbot_master_name, builder_name, build_number):
  """Return the dashboard (luci-milo) URL for this run

  Args:
    buildbot_master_name: Name of buildbot master, e.g. chromeos
    builder_name: Builder name on buildbot dashboard.
    build_number: Build number for this validation attempt.

  Returns:
    The fully formed URI.
  """
  url_suffix = '%s/%s' % (builder_name, str(build_number))
  url_suffix = urllib.parse.quote(url_suffix)
  return os.path.join(
      _LUCI_MILO_BUILDBOT_URL, buildbot_master_name, url_suffix)


def ConstructLogDogUri(build_number, stage):
  """Return the logdog URL for the given build number and stage.

  Args:
    build_number: The build ID.
    stage: The name of the stage to view logs for.

  Returns:
    The fully formed URI.
  """
  return _LOGDOG_URL % (str(build_number), stage)


def ConstructViceroyBuildDetailsUri(build_id):
  """Return the dashboard (viceroy) URL for this run.

  Args:
    build_id: CIDB id for the master build.

  Returns:
    The fully formed URI.
  """
  _link = ('https://viceroy.corp.google.com/'
           'chromeos/build_details?build_id=%(build_id)s')
  return _link % {'build_id': build_id}


def ConstructGoldenEyeBuildDetailsUri(build_id):
  """Return the dashboard (goldeneye) URL for this run.

  Args:
    build_id: CIDB id for the build.

  Returns:
    The fully formed URI.
  """
  _link = ('http://go/goldeneye/'
           'chromeos/healthmonitoring/buildDetails?id=%(build_id)s')
  return _link % {'build_id': build_id}


def ConstructAnnotatorUri(build_id):
  """Return the build annotator URL for this run.

  Args:
    build_id: CIDB id for the master build.

  Returns:
    The fully formed URI.
  """
  _link = ('https://chromiumos-build-annotator.googleplex.com/'
           'build_annotations/edit_annotations/master-paladin/%(build_id)s/?')
  return _link % {'build_id': build_id}
