# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions shared between files in the chromite shell."""


# Python imports
import ConfigParser
import cPickle
import os
import tempfile


# Local imports
import chromite.lib.cros_build_lib as cros_lib
from chromite.lib import text_menu


# Find the Chromite root and Chromium OS root...  Note: in the chroot we may
# choose to install Chromite somewhere (/usr/lib/chromite?), so we use the
# environment variable to get the right place if it exists.
CHROMITE_PATH = os.path.join(os.path.dirname(os.path.realpath(__file__)), '..')
SRCROOT_PATH = os.environ.get('CROS_WORKON_SRCROOT',
                              os.path.realpath(os.path.join(CHROMITE_PATH,
                                                            '..')))


# Commands can take one of these two types of specs.  Note that if a command
# takes a build spec, we will find the associated chroot spec.  This should be
# a human-readable string.  It is printed and also is the name of the spec
# directory.
BUILD_SPEC_TYPE = 'build'
CHROOT_SPEC_TYPE = 'chroot'


# This is a special target that indicates that you want to do something just
# to the host.  This means different things to different commands.
# TODO(dianders): Good idea or bad idea?
_HOST_TARGET = 'HOST'


def GetBoardDir(build_config):
  """Returns the board directory (inside the chroot) given the name.

  Args:
    build_config: A SafeConfigParser representing the config that we're
        building.

  Returns:
    The absolute path to the board.
  """
  target_name = build_config.get('DEFAULT', 'target')

  # Extra checks on these, since we sometimes might do a rm -f on the board
  # directory and these could cause some really bad behavior.
  assert target_name, "Didn't expect blank target name."
  assert len(target_name.split()) == 1, 'Target name should have no whitespace.'

  return os.path.join('/', 'build', target_name)


def GetChrootAbsDir(chroot_config):
  """Returns the absolute chroot directory the chroot config.

  Args:
    chroot_config: A SafeConfigParser representing the config for the chroot.

  Returns:
    The chroot directory, always absolute.
  """
  chroot_dir = chroot_config.get('CHROOT', 'path')
  chroot_dir = os.path.join(SRCROOT_PATH, chroot_dir)

  return chroot_dir


def DoesChrootExist(chroot_config):
  """Return True if the chroot folder exists.

  Args:
    chroot_config: A SafeConfigParser representing the config for the chroot.

  Returns:
    True if the chroot folder exists.
  """
  chroot_dir = GetChrootAbsDir(chroot_config)
  return os.path.isdir(chroot_dir)


def FindSpec(spec_name, spec_type=BUILD_SPEC_TYPE, can_show_ui=True):
  """Find the spec with the given name.

  This tries to be smart about helping the user to find the right spec.  See
  the spec_name parameter for details.

  Args:
    spec_name: Can be any of the following:
        1. A full path to a spec file (including the .spec suffix).  This is
           checked first (i.e. if os.path.isfile(spec_name), we assume we've
           got this case).
        2. The full name of a spec file somewhere in the spec search path
           (not including the .spec suffix).  This is checked second.  Putting
           this check second means that if one spec name is a substring of
           another, you can still specify the shorter spec name and know you
           won't get a menu (the exact match prevents the menu).
        3. A substring that will be used to pare-down a menu of spec files
           found in the spec search path.  Can be the empty string to show a
           menu of all spec files in the spec path.  NOTE: Only possible if
           can_show_ui is True.
    spec_type: The type of spec this is: 'chroot' or 'build'.
    can_show_ui: If True, enables the spec name to be a substring since we can
        then show a menu if the substring matches more than one thing.

  Returns:
    A path to the spec file.
  """
  spec_suffix = '.spec'

  # Handle 'HOST' for spec name w/ no searching, so it counts as an exact match.
  if spec_type == BUILD_SPEC_TYPE and spec_name == _HOST_TARGET.lower():
    return _HOST_TARGET

  # If we have an exact path name, that's it.  No searching.
  if os.path.isfile(spec_name):
    return spec_name

  # Figure out what our search path should be.
  # ...make these lists in anticipation of the need to support specs that live
  # in private overlays.
  # TODO(dianders): Should specs be part of the shell, or part of the main
  # chromite?
  search_path = [
      os.path.join(CHROMITE_PATH, 'specs', spec_type),
  ]

  # Look for an exact match of a spec name.  An exact match will go through with
  # no menu.
  if spec_name:
    for dir_path in search_path:
      spec_path = os.path.join(dir_path, spec_name + spec_suffix)
      if os.path.isfile(spec_path):
        return spec_path

  # cros_lib.Die right away if we can't show UI and didn't have an exact match.
  if not can_show_ui:
    cros_lib.Die("Couldn't find %s spec: %s" % (spec_type, spec_name))

  # No full path and no exact match.  Move onto a menu.
  # First step is to construct the options.  We'll store in a dict keyed by
  # spec name.
  options = {}
  for dir_path in search_path:
    for file_name in os.listdir(dir_path):
      # Find any files that end with ".spec" in a case-insensitive manner.
      if not file_name.lower().endswith(spec_suffix):
        continue

      this_spec_name, _ = os.path.splitext(file_name)

      # Skip if this spec file doesn't contain our substring.  We are _always_
      # case insensitive here to be helpful to the user.
      if spec_name.lower() not in this_spec_name.lower():
        continue

      # Disallow the spec to appear twice in the search path.  This is the
      # safest thing for now.  We could revisit it later if we ever found a
      # good reason (and if we ever have more than one directory in the
      # search path).
      if this_spec_name in options:
        cros_lib.Die('Spec %s was found in two places in the search path' %
            this_spec_name)

      # Combine to get a full path.
      full_path = os.path.join(dir_path, file_name)

      # Ignore directories or anything else that isn't a file.
      if not os.path.isfile(full_path):
        continue

      # OK, it's good.  Store the path.
      options[this_spec_name] = full_path

  # Add 'HOST'.  All caps so it sorts first.
  if (spec_type == BUILD_SPEC_TYPE and
      spec_name.lower() in _HOST_TARGET.lower()):
    options[_HOST_TARGET] = _HOST_TARGET

  # If no match, die.
  if not options:
    cros_lib.Die("Couldn't find any matching %s specs for: %s" % (spec_type,
                                                                  spec_name))

  # Avoid showing the user a menu if the user's search string matched exactly
  # one item.
  if spec_name and len(options) == 1:
    _, spec_path = options.popitem()
    return spec_path

  # If more than one, show a menu...
  option_keys = sorted(options.iterkeys())
  choice = text_menu.TextMenu(option_keys, 'Choose a %s spec' % spec_type)

  if choice is None:
    cros_lib.Die('OK, cancelling...')
  else:
    return options[option_keys[choice]]


def ReadConfig(spec_path):
  """Read the a build config or chroot config from a spec file.

  This includes adding thue proper _default stuff in.

  Args:
    spec_path: The path to the build or chroot spec.

  Returns:
    config: A SafeConfigParser representing the config.
  """
  spec_name, _ = os.path.splitext(os.path.basename(spec_path))
  spec_dir = os.path.dirname(spec_path)

  config = ConfigParser.SafeConfigParser({'name': spec_name})
  config.read(os.path.join(spec_dir, '_defaults'))
  config.read(spec_path)

  return config


def GetBuildConfigFromArgs(argv):
  """Helper for commands that take a build config in the arg list.

  This function can cros_lib.Die() in some instances.

  Args:
    argv: A list of command line arguments.  If non-empty, [0] should be the
        build spec.  These will not be modified.

  Returns:
    argv: The argv with the build spec stripped off.  This might be argv[1:] or
        just argv.  Not guaranteed to be new memory.
    build_config: The SafeConfigParser for the build config; might be None if
        this is a host config.  TODO(dianders): Should there be a build spec for
        the host?
  """
  # The spec name is optional.  If no arguments, we'll show a menu...
  # Note that if there are arguments, but the first argument is a flag, we'll
  # assume that we got called before OptionParser.  In that case, they might
  # have specified options but still want the board menu.
  if argv and not argv[0].startswith('-'):
    spec_name = argv[0]
    argv = argv[1:]
  else:
    spec_name = ''

  spec_path = FindSpec(spec_name)

  if spec_path == _HOST_TARGET:
    return argv, None

  build_config = ReadConfig(spec_path)

  # TODO(dianders): Add a config checker class that makes sure that the
  # target is not a blank string.  Might also be good to make sure that the
  # target has no whitespace (so we don't screw up a subcommand invoked
  # through a shell).

  return argv, build_config


def EnterChroot(chroot_config, func, *args, **kwargs):
  """Re-run the given function inside the chroot.

  When the function is run, it will be run in a SEPARATE INSTANCE of chromite,
  which will be run in the chroot.  This is a little weird.  Specifically:
  - When the callee executes, it will be a separate python instance.
    - Globals will be reset back to defaults.
    - A different version of python (with different modules) may end up running
      the script in the chroot.
  - All arguments are pickled up into a temp file, whose path is passed on the
    command line.
    - That means that args must be pickleable.
    - It also means that modifications to the parameters by the callee are not
      visible to the caller.
    - Even the function is "pickled".  The way the pickle works, I belive, is it
      just passes the name of the function.  If this name somehow resolves
      differently in the chroot, you may get weirdness.
  - Since we're in the chroot, obviously files may have different paths.  It's
    up to you to convert parameters if you need to.
  - The stdin, stdout, and stderr aren't touched.

  Args:
    chroot_config: A SafeConfigParser representing the config for the chroot.
    func: Either: a) the function to call or b) A tuple of an object and the
        name of the method to call.
    args: All other arguments will be passed to the function as is.
    kwargs: All other arguments will be passed to the function as is.
  """
  # Make sure that the chroot exists...
  chroot_dir = GetChrootAbsDir(chroot_config)
  if not DoesChrootExist(chroot_config):
    cros_lib.Die(
        'Chroot dir does not exist; try the "build host" command.\n  %s.' %
        chroot_dir)

  cros_lib.Info('ENTERING THE CHROOT')

  # Save state to a temp file (inside the chroot!) using pickle.
  tmp_dir = os.path.join(chroot_dir, 'tmp')
  state_file = tempfile.NamedTemporaryFile(prefix='chromite', dir=tmp_dir)
  try:
    cPickle.dump((func, args, kwargs), state_file, cPickle.HIGHEST_PROTOCOL)
    state_file.flush()

    # Translate temp file name into a chroot path...
    chroot_state_path = os.path.join('/tmp', os.path.basename(state_file.name))

    # Put together command.  We're going to force the shell to do all of the
    # splitting of arguments, since we're throwing all of the flags from the
    # config file in there.
    # TODO(dianders): It might be nice to run chromite as:
    #     python -m chromite.chromite_main
    #   ...but, at the moment, that fails if you're in src/scripts
    #   which already has a chromite folder.
    cmd = (
        './enter_chroot.sh --chroot="%s" %s --'
        ' python ../../chromite/shell/main.py --resume-state %s') % (
            chroot_dir,
            chroot_config.get('CHROOT', 'enter_chroot_flags'),
            chroot_state_path)

    # We'll put CWD as src/scripts when running the command.  Since everyone
    # running by hand has their cwd there, it is probably the safest.
    cwd = os.path.join(SRCROOT_PATH, 'src', 'scripts')

    # Run it.  We allow "error" so we don't print a confusing error message
    # filled with out resume-state garbage on control-C.
    cmd_result = cros_lib.RunCommand(cmd, shell=True, cwd=cwd, print_cmd=False,
                            exit_code=True, error_ok=True, ignore_sigint=True)

    if cmd_result.returncode:
      cros_lib.Die('Chroot exited with error code %d' % cmd_result.returncode)
  finally:
    # Make sure things get closed (and deleted), even upon exception.
    state_file.close()


def ResumeEnterChrootIfNeeded(argv):
  """Should be called as the first line in main() to support EnterChroot().

  We'll check whether the --resume-state argument is there.  If the argument is
  there, we'll use it and call the proper function (now that we're in the
  chroot).  We'll then return True to indicate to main() that it should exit
  immediately.

  If the --resume-state argument is not there, this function will return False
  without doing anything else.

  Args:
    argv: The value of sys.argv

  Returns:
    True if we resumed; indicates that main should return without doing any
    further work.
  """
  if argv[1:2] == ['--resume-state']:
    # Internal mechanism (not documented to users) to resume in the chroot.
    # ...actual resume state file is passed in argv[2] for simplicity...
    assert len(argv) == 3, 'Resume State not passed properly.'
    func, args, kwargs = cPickle.load(open(argv[2], 'rb'))

    # Handle calling a method in a class; that can't be directly pickled.
    if isinstance(func, tuple):
      obj, method = func
      func = getattr(obj, method)

    func(*args, **kwargs)  # pylint: disable=W0142

    # Return True to tell main() that it should exit.
    return True
  else:
    # Return False to tell main() that we didn't find the --resume-state
    # argument and that it should do normal arugment parsing.
    return False
