#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Package Management

This module is used to bring in external Python dependencies via eggs from the
PyPi repository.

The approach is to create a site directory in depot_tools root which will
contain those packages that are listed as dependencies in the module variable
PACKAGES. Since we can't guarantee that setuptools is available in all
distributions this module also contains the ability to bootstrap the site
directory by manually downloading and installing setuptools. Once setuptools is
available it uses that to install the other packages in the traditional
manner.

Use is simple:

  import package_management

  # Before any imports from the site directory, call this. This only needs
  # to be called in one place near the beginning of the program.
  package_management.SetupSiteDirectory()

  # If 'SetupSiteDirectory' fails it will complain with an error message but
  # continue happily. Expect ImportErrors when trying to import any third
  # party modules from the site directory.

  import some_third_party_module

  ... etc ...
"""

import cStringIO
import os
import re
import shutil
import site
import subprocess
import sys
import tempfile
import urllib2


# This is the version of setuptools that we will download if the local
# python distribution does not include one.
SETUPTOOLS = ('setuptools', '0.6c11')

# These are the packages that are to be installed in the site directory.
# easy_install makes it so that the most recently installed version of a
# package is the one that takes precedence, even if a newer version exists
# in the site directory. This allows us to blindly install these one on top
# of the other without worrying about whats already installed.
#
# NOTE: If we are often rolling these dependencies then users' site
#     directories will grow monotonically. We could be purging any orphaned
#     packages using the tools provided by pkg_resources.
PACKAGES = (('logilab-common', '0.57.1'),
            ('logilab-astng', '0.23.1'),
            ('pylint', '0.25.1'))


# The Python version suffix used in generating the site directory and in
# requesting packages from PyPi.
VERSION_SUFFIX = "%d.%d" % sys.version_info[0:2]

# This is the root directory of the depot_tools installation.
ROOT_DIR = os.path.abspath(os.path.dirname(__file__))

# This is the path of the site directory we will use. We make this
# python version specific so that this will continue to work even if the
# python version is rolled.
SITE_DIR = os.path.join(ROOT_DIR, 'site-packages-py%s' % VERSION_SUFFIX)

# This status file is created the last time PACKAGES were rolled successfully.
# It is used to determine if packages need to be rolled by comparing against
# the age of __file__.
LAST_ROLLED = os.path.join(SITE_DIR, 'last_rolled.txt')


class Error(Exception):
  """The base class for all module errors."""
  pass


class InstallError(Error):
  """Thrown if an installation is unable to complete."""
  pass


class Package(object):
  """A package represents a release of a project.

  We use this as a lightweight version of pkg_resources, allowing us to
  perform an end-run around setuptools for the purpose of bootstrapping. Its
  functionality is very limited.

  Attributes:
    name: the name of the package.
    version: the version of the package.
    safe_name: the safe name of the package.
    safe_version: the safe version string of the package.
    file_name: the filename-safe name of the package.
    file_version: the filename-safe version string of the package.
  """

  def __init__(self, name, version):
    """Initialize this package.

    Args:
      name: the name of the package.
      version: the version of the package.
    """
    self.name = name
    self.version = version
    self.safe_name = Package._MakeSafeName(self.name)
    self.safe_version = Package._MakeSafeVersion(self.version)
    self.file_name = Package._MakeSafeForFilename(self.safe_name)
    self.file_version = Package._MakeSafeForFilename(self.safe_version)

  @staticmethod
  def _MakeSafeName(name):
    """Makes a safe package name, as per pkg_resources."""
    return re.sub('[^A-Za-z0-9]+', '-', name)

  @staticmethod
  def _MakeSafeVersion(version):
    """Makes a safe package version string, as per pkg_resources."""
    version = re.sub('\s+', '.', version)
    return re.sub('[^A-Za-z0-9\.]+', '-', version)

  @staticmethod
  def _MakeSafeForFilename(safe_name_or_version):
    """Makes a safe name or safe version safe to use in a file name.
    |safe_name_or_version| must be a safe name or version string as returned
    by GetSafeName or GetSafeVersion.
    """
    return re.sub('-', '_', safe_name_or_version)

  def GetAsRequirementString(self):
    """Builds an easy_install requirements string representing this package."""
    return '%s==%s' % (self.name, self.version)

  def GetFilename(self, extension=None):
    """Builds a filename for this package using the setuptools convention.
    If |extensions| is provided it will be appended to the generated filename,
    otherwise only the basename is returned.

    The following url discusses the filename format:

      http://svn.python.org/projects/sandbox/trunk/setuptools/doc/formats.txt
    """
    filename = '%s-%s-py%s' % (self.file_name, self.file_version,
                               VERSION_SUFFIX)

    if extension:
      if not extension.startswith('.'):
        filename += '.'
      filename += extension

    return filename

  def GetPyPiUrl(self, extension):
    """Returns the URL where this package is hosted on PyPi."""
    return 'http://pypi.python.org/packages/%s/%c/%s/%s' % (VERSION_SUFFIX,
        self.file_name[0], self.file_name, self.GetFilename(extension))

  def DownloadEgg(self, dest_dir, overwrite=False):
    """Downloads the EGG for this URL.

    Args:
      dest_dir: The directory where the EGG should be written. If the EGG
          has already been downloaded and cached the returned path may not
          be in this directory.
      overwite: If True the destination path will be overwritten even if
          it already exists. Defaults to False.

    Returns:
      The path to the written EGG.

    Raises:
      Error: if dest_dir doesn't exist, the EGG is unable to be written,
          the URL doesn't exist, or the server returned an error, or the
          transmission was interrupted.
    """
    if not os.path.exists(dest_dir):
      raise Error('Path does not exist: %s' % dest_dir)

    if not os.path.isdir(dest_dir):
      raise Error('Path is not a directory: %s' % dest_dir)

    filename = os.path.abspath(os.path.join(dest_dir, self.GetFilename('egg')))
    if os.path.exists(filename):
      if os.path.isdir(filename):
        raise Error('Path is a directory: %s' % filename)
      if not overwrite:
        return filename

    url = self.GetPyPiUrl('egg')

    try:
      url_stream = urllib2.urlopen(url)
      local_file = open(filename, 'wb')
      local_file.write(url_stream.read())
      local_file.close()
      return filename
    except (IOError, urllib2.HTTPError, urllib2.URLError):
      # Reraise with a new error type, keeping the original message and
      # traceback.
      raise Error, sys.exc_info()[1], sys.exc_info()[2]


def AddToPythonPath(path):
  """Adds the provided path to the head of PYTHONPATH and sys.path."""
  path = os.path.abspath(path)
  if path not in sys.path:
    sys.path.insert(0, path)

  paths = os.environ.get('PYTHONPATH', '').split(os.pathsep)
  if path not in paths:
    paths.insert(0, path)
    os.environ['PYTHONPATH'] = os.pathsep.join(paths)


def AddSiteDirectory(path):
  """Adds the provided path to the runtime as a site directory.

  Any modules that are in the site directory will be available for importing
  after this returns. If modules are added or deleted this must be called
  again for the changes to be reflected in the runtime.

  This calls both AddToPythonPath and site.addsitedir. Both are needed to
  convince easy_install to treat |path| as a site directory.
  """
  AddToPythonPath(path)
  site.addsitedir(path)  # pylint: disable=E1101

def EnsureSiteDirectory(path):
  """Creates and/or adds the provided path to the runtime as a site directory.

  This works like AddSiteDirectory but it will create the directory if it
  does not yet exist.

  Raise:
    Error: if the site directory is unable to be created, or if it exists and
        is not a directory.
  """
  if os.path.exists(path):
    if not os.path.isdir(path):
      raise Error('Path is not a directory: %s' % path)
  else:
    try:
      os.mkdir(path)
    except IOError:
      raise Error('Unable to create directory: %s' % path)

  AddSiteDirectory(path)


def ModuleIsFromPackage(module, package_path):
  """Determines if a module has been imported from a given package.

  Args:
    module: the module to test.
    package_path: the path to the package to test.

  Returns:
    True if |module| has been imported from |package_path|, False otherwise.
  """
  try:
    m = os.path.abspath(module.__file__)
    p = os.path.abspath(package_path)
    if len(m) <= len(p):
      return False
    if m[0:len(p)] != p:
      return False
    return m[len(p)] == os.sep
  except AttributeError:
    return False


def _CaptureStdStreams(function, *args, **kwargs):
  """Captures stdout and stderr while running the provided function.

  This only works if |function| only accesses sys.stdout and sys.stderr. If
  we need more than this we'll have to use subprocess.Popen.

  Args:
    function: the function to be called.
    args: the arguments to pass to |function|.
    kwargs: the keyword arguments to pass to |function|.
  """
  orig_stdout = sys.stdout
  orig_stderr = sys.stderr
  sys.stdout = cStringIO.StringIO()
  sys.stderr = cStringIO.StringIO()
  try:
    return function(*args, **kwargs)
  finally:
    sys.stdout = orig_stdout
    sys.stderr = orig_stderr


def InstallPackage(url_or_req, site_dir):
  """Installs a package to a site directory.

  |site_dir| must exist and already be an active site directory. setuptools
  must in the path. Uses easy_install which may involve a download from
  pypi.python.org, so this also requires network access.

  Args:
    url_or_req: the package to install, expressed as an URL (may be local),
        or a requirement string.
    site_dir: the site directory in which to install it.

  Raises:
    InstallError: if installation fails for any reason.
  """
  args = ['--quiet', '--install-dir', site_dir, '--exclude-scripts',
          '--always-unzip', '--no-deps', url_or_req]

  # The easy_install script only calls SystemExit if something goes wrong.
  # Otherwise, it falls through returning None.
  try:
    import setuptools.command.easy_install
    _CaptureStdStreams(setuptools.command.easy_install.main, args)
  except (ImportError, SystemExit):
    # Re-raise the error, preserving the stack trace and message.
    raise InstallError, sys.exc_info()[1], sys.exc_info()[2]


def _RunInSubprocess(pycode):
  """Launches a python subprocess with the provided code.

  The subprocess will be launched with the same stdout and stderr. The
  subprocess will use the same instance of python as is currently running,
  passing |pycode| as arguments to this script. |pycode| will be interpreted
  as python code in the context of this module.

  Returns:
    True if the subprocess returned 0, False if it returned an error.
  """
  return not subprocess.call([sys.executable, __file__, pycode])


def _LoadSetupToolsFromEggAndInstall(egg_path):
  """Loads setuptools from the provided egg |egg_path|, and installs it to
  SITE_DIR.

  This is intended to be run from a subprocess as it pollutes the running
  instance of Python by importing a module and then forcibly deleting its
  source.

  Returns:
    True on success, False on failure.
  """
  AddToPythonPath(egg_path)

  try:
    # Import setuptools and ensure it comes from the EGG.
    import setuptools
    if not ModuleIsFromPackage(setuptools, egg_path):
      raise ImportError()
  except ImportError:
    print '  Unable to import downloaded package!'
    return False

  try:
    print '  Using setuptools to install itself ...'
    InstallPackage(egg_path, SITE_DIR)
  except InstallError:
    print '  Unable to install setuptools!'
    return False

  return True


def BootstrapSetupTools():
  """Bootstraps the runtime with setuptools.

  Will try to import setuptools directly. If not found it will attempt to
  download it and load it from there. If the download is successful it will
  then use setuptools to install itself in the site directory.

  This is meant to be run from a child process as it modifies the running
  instance of Python by importing modules and then physically deleting them
  from disk.

  Returns:
    Returns True if 'import setuptools' will succeed, False otherwise.
  """
  AddSiteDirectory(SITE_DIR)

  # Check if setuptools is already available. If so, we're done.
  try:
    import setuptools  # pylint: disable=W0612
    return True
  except ImportError:
    pass

  print 'Bootstrapping setuptools ...'

  EnsureSiteDirectory(SITE_DIR)

  # Download the egg to a temp directory.
  dest_dir = tempfile.mkdtemp('depot_tools')
  path = None
  try:
    package = Package(*SETUPTOOLS)
    print '  Downloading %s ...' % package.GetFilename()
    path = package.DownloadEgg(dest_dir)
  except Error:
    print '  Download failed!'
    shutil.rmtree(dest_dir)
    return False

  try:
    # Load the downloaded egg, and install it to the site directory. Do this
    # in a subprocess so as not to pollute this runtime.
    pycode = '_LoadSetupToolsFromEggAndInstall(%s)' % repr(path)
    if not _RunInSubprocess(pycode):
      raise Error()

    # Reload our site directory, which should now contain setuptools.
    AddSiteDirectory(SITE_DIR)

    # Try to import setuptools
    import setuptools
  except ImportError:
    print '  Unable to import setuptools!'
    return False
  except Error:
    # This happens if RunInSubProcess fails, and the appropriate error has
    # already been written to stdout.
    return False
  finally:
    # Delete the temp directory.
    shutil.rmtree(dest_dir)

  return True


def _GetModTime(path):
  """Gets the last modification time associated with |path| in seconds since
  epoch, returning 0 if |path| does not exist.
  """
  try:
    return os.stat(path).st_mtime
  except:  # pylint: disable=W0702
    # This error is different depending on the OS, hence no specified type.
    return 0


def _SiteDirectoryIsUpToDate():
  return _GetModTime(LAST_ROLLED) > _GetModTime(__file__)


def UpdateSiteDirectory():
  """Installs the packages from PACKAGES if they are not already installed.
  At this point we must have setuptools in the site directory.

  This is intended to be run in a subprocess *prior* to the site directory
  having been added to the parent process as it may cause packages to be
  added and/or removed.

  Returns:
    True on success, False otherwise.
  """
  if _SiteDirectoryIsUpToDate():
    return True

  try:
    AddSiteDirectory(SITE_DIR)
    import pkg_resources

    # Determine if any packages actually need installing.
    missing_packages = []
    for package in [SETUPTOOLS] + list(PACKAGES):
      pkg = Package(*package)
      req = pkg.GetAsRequirementString()

      # It may be that this package is already available in the site
      # directory. If so, we can skip past it without trying to install it.
      pkg_req = pkg_resources.Requirement.parse(req)
      try:
        dist = pkg_resources.working_set.find(pkg_req)
        if dist:
          continue
      except pkg_resources.VersionConflict:
        # This happens if another version of the package is already
        # installed in another site directory (ie: the system site directory).
        pass

      missing_packages.append(pkg)

    # Install the missing packages.
    if missing_packages:
      print 'Updating python packages ...'
      for pkg in missing_packages:
        print '  Installing %s ...' % pkg.GetFilename()
        InstallPackage(pkg.GetAsRequirementString(), SITE_DIR)

    # Touch the status file so we know that we're up to date next time.
    open(LAST_ROLLED, 'wb')
  except InstallError, e:
    print '  Installation failed: %s' % str(e)
    return False

  return True


def SetupSiteDirectory():
  """Sets up the site directory, bootstrapping setuptools if necessary.

  If this finishes successfully then SITE_DIR will exist and will contain
  the appropriate version of setuptools and all of the packages listed in
  PACKAGES.

  This is the main workhorse of this module. Calling this will do everything
  necessary to ensure that you have the desired packages installed in the
  site directory, and the site directory enabled in this process.

  Returns:
    True on success, False on failure.
  """
  if _SiteDirectoryIsUpToDate():
    AddSiteDirectory(SITE_DIR)
    return True

  if not _RunInSubprocess('BootstrapSetupTools()'):
    return False

  if not _RunInSubprocess('UpdateSiteDirectory()'):
    return False

  # Process the site directory so that the packages within it are available
  # for import.
  AddSiteDirectory(SITE_DIR)

  return True


def CanImportFromSiteDirectory(package_name):
  """Determines if the given package can be imported from the site directory.

  Args:
    package_name: the name of the package to import.

  Returns:
    True if 'import package_name' will succeed and return a module from the
    site directory, False otherwise.
  """
  try:
    return ModuleIsFromPackage(__import__(package_name), SITE_DIR)
  except ImportError:
    return False


def Test():
  """Runs SetupSiteDirectory and then tries to load pylint, ensuring that it
  comes from the site directory just created. This is an end-to-end unittest
  and allows for simple testing from the command-line by running

    ./package_management.py 'Test()'
  """
  print 'Testing package_management.'
  if not SetupSiteDirectory():
    print 'SetupSiteDirectory failed.'
    return False
  if not CanImportFromSiteDirectory('pylint'):
    print 'CanImportFromSiteDirectory failed.'
    return False
  print 'Success!'
  return True


def Main():
  """The main entry for the package management script.

  If no arguments are provided simply runs SetupSiteDirectory. If arguments
  have been passed we execute the first argument as python code in the
  context of this module. This mechanism is used during the bootstrap
  process so that the main instance of Python does not have its runtime
  polluted by various intermediate packages and imports.

  Returns:
    0 on success, 1 otherwise.
  """
  if len(sys.argv) == 2:
    result = False
    exec('result = %s' % sys.argv[1])

    # Translate the success state to a return code.
    return not result
  else:
    return not SetupSiteDirectory()


if __name__ == '__main__':
  sys.exit(Main())
