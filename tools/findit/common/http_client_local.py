# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
A http client with support for https connections with certificate verification.

The verification is based on http://tools.ietf.org/html/rfc6125#section-6.4.3
and the code is from Lib/ssl.py in python3:
  http://hg.python.org/cpython/file/4dac45f88d45/Lib/ssl.py

One use case is to download Chromium DEPS file in a secure way:
  https://src.chromium.org/chrome/trunk/src/DEPS

Notice: python 2.7 or newer is required.
"""

import cookielib
import httplib
import os
import re
import socket
import ssl
import urllib
import urllib2

import http_client


_SCRIPT_DIR = os.path.dirname(__file__)
_TRUSTED_ROOT_CERTS = os.path.join(_SCRIPT_DIR, 'cacert.pem')


class CertificateError(ValueError):
  pass


def _DNSNameMatch(dn, hostname, max_wildcards=1):
  """Matching according to RFC 6125, section 6.4.3

  http://tools.ietf.org/html/rfc6125#section-6.4.3
  """
  pats = []
  if not dn:
    return False

  parts = dn.split(r'.')
  leftmost = parts[0]
  remainder = parts[1:]

  wildcards = leftmost.count('*')
  if wildcards > max_wildcards:
    # Issue #17980: avoid denials of service by refusing more
    # than one wildcard per fragment.  A survery of established
    # policy among SSL implementations showed it to be a
    # reasonable choice.
    raise CertificateError(
        'too many wildcards in certificate DNS name: ' + repr(dn))

  # speed up common case w/o wildcards
  if not wildcards:
    return dn.lower() == hostname.lower()

  # RFC 6125, section 6.4.3, subitem 1.
  # The client SHOULD NOT attempt to match a presented identifier in which
  # the wildcard character comprises a label other than the left-most label.
  if leftmost == '*':
    # When '*' is a fragment by itself, it matches a non-empty dotless
    # fragment.
    pats.append('[^.]+')
  elif leftmost.startswith('xn--') or hostname.startswith('xn--'):
    # RFC 6125, section 6.4.3, subitem 3.
    # The client SHOULD NOT attempt to match a presented identifier
    # where the wildcard character is embedded within an A-label or
    # U-label of an internationalized domain name.
    pats.append(re.escape(leftmost))
  else:
    # Otherwise, '*' matches any dotless string, e.g. www*
    pats.append(re.escape(leftmost).replace(r'\*', '[^.]*'))

  # add the remaining fragments, ignore any wildcards
  for frag in remainder:
    pats.append(re.escape(frag))

  pat = re.compile(r'\A' + r'\.'.join(pats) + r'\Z', re.IGNORECASE)
  return pat.match(hostname)


def _MatchHostname(cert, hostname):
  """Verify that *cert* (in decoded format as returned by
  SSLSocket.getpeercert()) matches the *hostname*.  RFC 2818 and RFC 6125
  rules are followed, but IP addresses are not accepted for *hostname*.

  CertificateError is raised on failure. On success, the function
  returns nothing.
  """
  if not cert:
    raise ValueError('empty or no certificate, match_hostname needs a '
                     'SSL socket or SSL context with either '
                     'CERT_OPTIONAL or CERT_REQUIRED')
  dnsnames = []
  san = cert.get('subjectAltName', ())
  for key, value in san:
    if key == 'DNS':
      if _DNSNameMatch(value, hostname):
        return
      dnsnames.append(value)
  if not dnsnames:
    # The subject is only checked when there is no dNSName entry
    # in subjectAltName
    for sub in cert.get('subject', ()):
      for key, value in sub:
        # XXX according to RFC 2818, the most specific Common Name
        # must be used.
        if key == 'commonName':
          if _DNSNameMatch(value, hostname):
            return
          dnsnames.append(value)
  if len(dnsnames) > 1:
    raise CertificateError('hostname %r doesn\'t match either of %s'
                           % (hostname, ', '.join(map(repr, dnsnames))))
  elif len(dnsnames) == 1:
    raise CertificateError('hostname %r doesn\'t match %r'
                           % (hostname, dnsnames[0]))
  else:
    raise CertificateError('no appropriate commonName or '
                           'subjectAltName fields were found')


class HTTPSConnection(httplib.HTTPSConnection):

  def __init__(self, host, root_certs=_TRUSTED_ROOT_CERTS, **kwargs):
    self.root_certs = root_certs
    httplib.HTTPSConnection.__init__(self, host, **kwargs)

  def connect(self):
    # Overrides for certificate verification.
    args = [(self.host, self.port), self.timeout,]
    if self.source_address:
      args.append(self.source_address)
    sock = socket.create_connection(*args)

    if self._tunnel_host:
      self.sock = sock
      self._tunnel()

    # Wrap the socket for verification with the root certs.
    kwargs = {}
    if self.root_certs is not None:
      kwargs.update(cert_reqs=ssl.CERT_REQUIRED, ca_certs=self.root_certs)
    self.sock = ssl.wrap_socket(sock, **kwargs)

    # Check hostname.
    try:
      _MatchHostname(self.sock.getpeercert(), self.host)
    except CertificateError:
      self.sock.shutdown(socket.SHUT_RDWR)
      self.sock.close()
      raise


class HTTPSHandler(urllib2.HTTPSHandler):

  def __init__(self, root_certs=_TRUSTED_ROOT_CERTS):
    urllib2.HTTPSHandler.__init__(self)
    self.root_certs = root_certs

  def https_open(self, req):
    # Pass a reference to the function below so that verification against
    # trusted root certs could be injected.
    return self.do_open(self.GetConnection, req)

  def GetConnection(self, host, **kwargs):
    params = dict(root_certs=self.root_certs)
    params.update(kwargs)
    return HTTPSConnection(host, **params)


def _SendRequest(url, timeout=None):
  """Send request to the given https url, and return the server response.

  Args:
    url: The https url to send request to.

  Returns:
    An integer: http code of the response.
    A string: content of the response.

  Raises:
    CertificateError: Certificate verification fails.
  """
  if not url:
    return None, None

  handlers = []
  if url.startswith('https://'):
    # HTTPSHandler has to go first, because we don't want to send secure cookies
    # to a man in the middle.
    handlers.append(HTTPSHandler())


  cookie_file = os.environ.get('COOKIE_FILE')
  if cookie_file and os.path.exists(cookie_file):
    handlers.append(
        urllib2.HTTPCookieProcessor(cookielib.MozillaCookieJar(cookie_file)))

  url_opener = urllib2.build_opener(*handlers)
  if timeout is not None:
    response = url_opener.open(url, timeout=timeout)
  else:
    response = url_opener.open(url)
  return response.code, response.read()


class HttpClientLocal(http_client.HttpClient):
  """This http client is used locally in a workstation, GCE VMs, etc."""

  @staticmethod
  def Get(url, params={}, timeout=None):
    if params:
      url = '%s?%s' % (url, urllib.urlencode(params))
    return _SendRequest(url, timeout=timeout)
