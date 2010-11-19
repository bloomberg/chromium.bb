#!/usr/bin/python

# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

"""Multipy - Runs multiple scripts with platform independant arguments.

 The multipy module provides a mechanism for running multiple python
 scripts consecutively.  When run as:

 multipy.py <opts> <script> [<script>...] [<key=val> ...]

 multipy will load one or more scripts consisting of LF delimited python
 script invocations.  Each line of the script is run individually, by
 first replacing all variables in the form %(var)s with the values provided
 on the multipy command line, with the appropriate value.  In addition,
 arguments in the script will have their Linux style seperators replaced
 with platform appropriate seperators.  This script will return zero if all
 commands executed return 0, otherwise it will return 1.

 -h, --help : Display usage.
 -i, --ignore : Ignore non-zero results and continue processing.
 -v, --verbose : Dump verbose information.
"""

import getopt
import os
import sys

class MultiPy(object):
  def __init__(self, ignore = False, output = False, verbose = False):
    self.commands = []
    self.scripts = []
    self.lib_map = {}
    self.var_map = {}
    self.verbose = verbose
    self.ignore = ignore
    self.output = output


  def __GetLib(self, libname):
    """ Load or reload a library before calling its main. """

    # If we already have the module, reload it, to reset its state
    if libname in self.lib_map:
      self.__Log("Reload %s." % libname)
      mod = self.lib_map[libname]
      reload(mod)
      return mod

    # Split the library into a path plus library name
    # removing the '.py' if needed
    path, lib = os.path.split(libname)
    if lib[-3:] == '.py':
      lib = lib[:-3]

    # Import the library
    # Since this modules is called from within the build system
    # and we can not predict what directory is used, or which
    # paths are set, we append this path just in case.
    if not path in sys.path:
      self.__Log("Add path %s." % path)
      sys.path.append(path)

    self.__Log("Load %s." % libname)
    mod = __import__(lib)
    self.lib_map[libname] = mod
    return mod

  def __Log(self, msg, otherCheck=False):
    """ Print the message if verbosity is on, or the extra test is true."""
    if self.verbose or otherCheck:
      print msg

  def AddScript(self, script):
    """ Load the script and add its lines to the list of commands. """
    src = open(script, 'r')
    lines = src.readlines()
    src.close()

    # Iterate through lines, removing comments and empties
    for line in lines:
      line = line.strip()
      if not line or line[0] == '#':
        continue
      # Add valid lines to the list of commands
      self.commands.append(line)


  def AddVarMap(self, key, value):
    """ Add a mapping between a variable and a value. """
    self.var_map[key] = value
    self.__Log("Add map of %s to %s." % (key, value))


  def ClearCommands(self):
    """ Clear all commands. """
    self.commands = []


  def RemapCommand(self, command):
    """ Remap variables and path seperators in the specified command. """

    # Replace all variables in the command
    command = str(command) % self.var_map

    # Split into arguments
    commandarray = command.split()

    # For each argument, rejoin with directory sep with platform specific sep
    for index in range(len(commandarray)):
      token = commandarray[index]
      commandarray[index] = os.sep.join(token.split('/'))
    command = ' '.join(commandarray)
    return command


  def Run(self):
    """ Run all of the loaded scripts.

    Run all of the loaded scripts in order.  If any script returns non-zero
    return False, otherwise return True.
    """
    did_pass = True
    for command in self.commands:
      val = self.RunCommand(command)
      if val != 0:
        did_pass = False
        if not self.ignore:
          self.__Log("  Command failed!" , True)
          return did_pass

    self.__Log("Success = %s" % str(did_pass))
    return did_pass


  def RunCommand(self, command):
    """ Run a single command, converting paths and variables.

    Split the command string into an argument list where arg[0] is the
    name of the script to be executed.  Convert paths and variables in
    each argument, then load and execute the main within that script by
    passing in the argument list.
    """

    # Clean the command, replacing variables, etc...
    command = self.RemapCommand(command)

    # Split the command into a standard arugment list
    commandarray = command.split()

    # Get the module which is the first token in the command
    mod = self.__GetLib(commandarray[0])

    # Run the commands
    self.__Log(str(command), self.output)
    val = mod.main(commandarray)
    self.__Log("  Returned %d." % val)
    return val


def usage():
  """ Print the usage information. """
  print __doc__
  sys.exit(1)

def main(argv):
  # All arguments after location of python script are variable declaractions
  # in the form of <Name>=<Value>.  They will be found in the command strings
  # as $Name
  ignore = False
  verbose = False
  output = False

  # Parse command-line arguments
  long_opts = ['help','ignore','output','verbose']
  opts, args = getopt.getopt(argv[1:], 'hiov', long_opts)

  # Process options
  for k,v in opts:
    if k == '-h' or k == '--help':
      usage()
    if k == '-i' or k == '--ignore':
      ignore = True
    if k == '-o' or k == '--output':
      output = True
    if k == '-v' or k == '--verbose':
      verbose = True

  mpy = MultiPy(ignore=ignore, output=output, verbose=verbose)

  # Process sources and key/value pairs
  for arg in args:
    keyval = arg.split('=')

    # If this is a file name, it must be a script to load
    if len(keyval) == 1:
      mpy.AddScript(arg)

    # If this contains an '=', it must be key=val
    elif len(keyval) == 2:
      mpy.AddVarMap(keyval[0], keyval[1])
    else:
      print "Unknown argument:", arg
      usage()

  if mpy.Run():
    return 0
  else:
    return 1

if __name__ == '__main__':
  sys.exit(main(sys.argv))
