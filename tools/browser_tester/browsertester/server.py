import BaseHTTPServer
import cgi
import os.path
import posixpath
import SimpleHTTPServer
import SocketServer
import threading
import urllib
import urlparse


class RequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):

  def NormalizePath(self, path):
    path = path.split('?', 1)[0]
    path = path.split('#', 1)[0]
    path = posixpath.normpath(urllib.unquote(path))
    words = path.split('/')

    bad = set((os.curdir, os.pardir, ''))
    words = [word for word in words if word not in bad]
    return os.sep.join(words)

  def translate_path(self, path):
    path = self.NormalizePath(path)
    if path in self.server.file_mapping:
      return self.server.file_mapping[path]
    elif not path.endswith('favicon.ico') and not self.server.allow_404:
      self.server.listener.ServerError('Cannot find file \'%s\'' % path)
    return path

  def SendRPCResponse(self, response):
    self.send_response(200)
    self.send_header("Content-type", "text/plain")
    self.send_header("Content-length", str(len(response)))
    self.end_headers()
    self.wfile.write(response)

    # shut down the connection
    self.wfile.flush()
    self.connection.shutdown(1)

  def HandleRPC(self, name, query):
    kargs = {}
    for k, v in query.iteritems():
      assert len(v) == 1, k
      kargs[k] = v[0]

    l = self.server.listener
    try:
      response = getattr(l, name)(**kargs)
    except Exception, e:
      self.SendRPCResponse('%r' % (e,))
      raise
    else:
      self.SendRPCResponse(response)

  def do_GET(self):
    self.server.ResetTimeout()

    # Backwards compatible - treat result as tuple without named fields.
    _, _, path, _, query, _ = urlparse.urlparse(self.path)

    tester = '/TESTER/'
    if path.startswith(tester):
      # If the path starts with '/TESTER/', the GET is an RPC call.
      name = path[len(tester):]
      # Supporting Python 2.5 prevents us from using urlparse.parse_qs
      query = cgi.parse_qs(query, True)

      self.server.rpc_lock.acquire()
      try:
        self.HandleRPC(name, query)
      finally:
        self.server.rpc_lock.release()
    else:
      # A normal GET request for transferring files, etc.
      f = self.send_head()
      if f:
        self.copyfile(f, self.wfile)
        f.close()
      self.server.listener.Log('GET %s' % (self.path,))

  # Disable the built-in logging
  def log_message(self, format, *args):
    pass


# NOTE: SocketServer.ThreadingMixIn seems to cause stability problems
# when using older versions of Python.
class Server(BaseHTTPServer.HTTPServer):

  def Configure(self, file_mapping, allow_404, listener):
    self.file_mapping = file_mapping
    self.allow_404 = allow_404
    self.listener = listener
    self.rpc_lock = threading.Lock()

  def TestingBegun(self, timeout):
    self.test_in_progress = True
    self.timeout = timeout
    self.ResetTimeout()

  def ResetTimeout(self):
    self.timeout_count = 0

  def TestingEnded(self):
    self.test_in_progress = False

  def handle_timeout(self):
    self.timeout_count += 1

  def TimedOut(self, total_time):
    return (total_time >= 0.0 and
            self.timeout_count * self.timeout >= total_time)


def Create(host, port):
  return Server((host, port), RequestHandler)
