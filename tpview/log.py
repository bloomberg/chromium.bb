# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gzip import GzipFile
from StringIO import StringIO
import base64
import bz2
import cookielib
import getpass
import os
import os.path
import re
import subprocess
import sys
import tarfile
import tempfile
import urllib2


# path to current script directory
script_dir = os.path.dirname(os.path.realpath(__file__))

# path to the cookies file used for storing the login cookies.
cookies_file = os.path.join(script_dir, "tptool.cookies")


def Log(source, options):
  """
    Returns a log object containg activity, evdev and system logs.
    source can be either an ip address to a device from which to
    download, a url pointing to a feedback report to pull or a filename.
  """
  if os.path.exists(source):
    return FileLog(source, options)
  else:
    match = re.search("Report/([0-9]+)", source)
    if match:
      return FeedbackLog(match.group(1), options)
    else:
      # todo add regex and error message
      return DeviceLog(source, options)


class AbstractLog(object):
  """
    A log file consists of the activity log, the evdev log and
    a system log which can be saved to disk all together.
  """
  def SaveAs(self, filename):
    open(filename, "w").write(self.activity)
    if self.evdev:
      open(filename + ".evdev", "w").write(self.evdev)
    if self.system:
      open(filename + ".system", "w").write(self.system)


class FileLog(AbstractLog):
  """
    Loads log from file. Does not contain evdev or system logs.
  """
  def __init__(self, filename, options):
    self.activity = open(filename).read()
    self.evdev = ""
    self.system = ""


class FeedbackLog(AbstractLog):
  """
    Downloads logs from a feedback id.
  """
  def __init__(self, id, options):
    # setup cookies file and url opener
    self.cookies = cookielib.MozillaCookieJar(cookies_file)
    if os.path.exists(cookies_file):
      self.cookies.load()
    cookie_processor = urllib2.HTTPCookieProcessor(self.cookies)
    opener = urllib2.build_opener(cookie_processor)

    self._DownloadSystemLog(id, opener)
    self._ExtractLogFiles()

  def _Login(self, opener):
    print "Login to corp.google.com:"
    username = getpass.getuser()
    password = getpass.getpass()
    sys.stdout.write("OTP: ")
    otp = sys.stdin.readline().strip()

    # execute login by posting userdata to login.corp.google.com
    url = "https://login.corp.google.com/login?ssoformat=CORP_SSO"
    data = "u=%s&pw=%s&otp=%s" % (username, password, otp)
    result = opener.open(url, data).read()

    # check if the result displays error
    if result.find("error") >= 0:
      print "Login failed"
      exit(-1)

    # login was successful. save cookies to file to be reused later.
    self.cookies.save()

  def _GetLatestFile(self, match, tar):
    # find file name containing match with latest timestamp
    names = filter(lambda n: n.find(match) >= 0, tar.getnames())
    names.sort()
    return tar.extractfile(names[-1])

  def _DownloadSystemLog(self, id, opener):
    # First download the report.bz2 file
    logfile = "report-%s-system_logs.bz2" % id
    url = ("https://feedback.corp.googleusercontent.com/binarydata/"+
           "%s?id=%s&logIndex=0") % (logfile, id)
    report_bz2 = opener.open(url).read()

    if report_bz2[0:2] != "BZ":
      # not a bzip file. Log in and try again.
      self._Login(opener)
      report_bz2 = opener.open(url).read()

    # unpack bzip file
    self.system = bz2.decompress(report_bz2)

  def _ExtractLogFiles(self):
    # find embedded and uuencoded activity.tar in system log
    log_start = ("hack-33025-touchpad_activity=\"\"\"\n" +
                 "begin-base64 644 touchpad_activity_log.tar\n")
    log_start_index = self.system.index(log_start) + len(log_start)
    log_end_index = self.system.index("\"\"\"", log_start_index)
    activity_tar_enc = self.system[log_start_index:log_end_index]

    # base64 decode
    activity_tar_data = base64.b64decode(activity_tar_enc)

    # untar
    activity_tar_file = tarfile.open(fileobj=StringIO(activity_tar_data))

    # find latest log files
    activity_gz = self._GetLatestFile("touchpad_activity", activity_tar_file)
    evdev_gz = self._GetLatestFile("cmt_input_events", activity_tar_file)

    # decompress log files
    self.activity = GzipFile(fileobj=activity_gz).read()
    self.evdev = GzipFile(fileobj=evdev_gz).read()


identity_message = """\
In order to access devices in TPTool, you need to have password-less
auth for chromebooks set up.
Would you like tptool to run the following command for you?
$ %s
Yes/No? (Default: No)"""

class DeviceLog(AbstractLog):
  """
    Downloads logs from a running chromebook via scp.
  """
  def __init__(self, ip, options):
    self.id_path = os.path.expanduser("~/.ssh/identity")
    if not os.path.exists(self.id_path):
      self._SetupIdentity()

    if options.new:
      self._GenerateLogs(ip)

    self.activity = self._Download(ip, "touchpad_activity_log.txt")
    self.evdev = self._Download(ip, "cmt_input_events.dat")
    self.system = ""

  def _GenerateLogs(self, ip):
    cmd = ("ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no " +
           "-q root@%s /opt/google/touchpad/tpcontrol log") % ip
    process = subprocess.Popen(cmd.split())
    process.wait()

  def _Download(self, ip, filename):
    temp = tempfile.NamedTemporaryFile("r")
    cmd = ("scp -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no " +
           "-q root@%s:/var/log/%s %s")
    cmd = cmd % (ip, filename, temp.name)
    process = subprocess.Popen(cmd.split(), stdout=subprocess.PIPE)
    process.wait()
    temp.seek(0)
    return temp.read()

  def _SetupIdentity(self):
    source = os.path.expanduser("~/trunk/chromite/ssh_keys/testing_rsa")
    command = "cp %s %s && chmod 600 %s" % (source, self.id_path, self.id_path)
    print identity_message % command
    response = sys.stdin.readline().strip()
    if response in ("Yes", "yes", "Y", "y"):
      print command
      os.system(command)
