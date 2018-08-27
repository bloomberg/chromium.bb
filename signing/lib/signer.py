# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""ChromeOS image signer logic

Terminology:
  Keyset:     Set of keys used for signing, similar to a keyring
  Signer:     Object able to sign a type of image: bios, ec, kernel, etc...
  Image:      Fle that can be signed by a Signer

Signing requires an image to sign and its required keys.

A Signer is expected to understand how to take an input and output signed
artifacts using the stored Keychain.

A Keyset consists of keys and signed objects called keyblocks.

Signing Flow:

 Key+------+->Keyset+---+->Signer+->Image Out
           |            |
 Keyblock+-+  Image In+-+
"""

from __future__ import print_function


import ConfigParser
import os
import re

from chromite.lib import cros_build_lib


class SignerOutputTemplateError(Exception):
  """Raise when there is an issue with filling a signer output template"""


class SignerInstructionConfig(object):
  """Signer Configuration based on ini file.

  See Signer Documentation - Instruction File Format:
  https://goto.google.com/cros-signer-instruction-file
  """
  DEFAULT_TEMPLATE = ('chromeos_@VERSION@_@BOARD@_@TYPE@_@CHANNEL@-channel_'
                      '@KEYSET@.bin')

  def __init__(self, archive='', board='', artifact_type='', version='',
               versionrev='', keyset='', channel='', input_files=(),
               output_files=()):
    """Initialize Configuration."""
    # [general] section
    self.archive = archive
    self.board = board
    self.artifact_type = artifact_type
    self.version = version
    self.versionrev = versionrev

    # [insns] section
    self.keyset = keyset
    self.channel = channel

    # Wrap files in tuple if given as a string
    self.input_files = ((input_files,) if isinstance(input_files, basestring)
                        else input_files)

    self.output_files = ((output_files,) if isinstance(output_files, basestring)
                         else output_files)

  def __eq__(self, other):
    return self.ToIniDict() == other.ToIniDict()

  def ToIniDict(self):
    """Return ini layout."""

    # [general] section
    general_dict = {}
    if self.archive:
      general_dict['archive'] = self.archive
    if self.board:
      general_dict['board'] = self.board
    if self.artifact_type:
      general_dict['type'] = self.artifact_type
    if self.version:
      general_dict['version'] = self.version
    if self.versionrev:
      general_dict['versionrev'] = self.versionrev

    # [insns] section
    insns_dict = {}
    if self.keyset:
      insns_dict['keyset'] = self.keyset
    if self.channel:
      insns_dict['channel'] = self.channel
    if self.input_files:
      insns_dict['input_files'] = ' '.join(self.input_files)
    if self.output_files:
      insns_dict['output_names'] = ' '.join(self.output_files)

    return {'general': general_dict,
            'insns': insns_dict}

  def ReadIniFile(self, fd):
    """Reads given file descriptor into configuration"""
    config = ConfigParser.ConfigParser(self.ToIniDict())
    config.readfp(fd)

    self.archive = config.get('general', 'archive')
    self.board = config.get('general', 'board')
    self.artifact_type = config.get('general', 'type')
    self.version = config.get('general', 'version')
    self.versionrev = config.get('general', 'versionrev')

    self.keyset = config.get('insns', 'keyset')
    self.channel = config.get('insns', 'channel')

    # Optional options
    if config.has_option('insns', 'input_files'):
      self.input_files = config.get('insns', 'input_files').split(' ')

    if config.has_option('insns', 'output_names'):
      self.output_files = config.get('insns', 'output_names').split(' ')

  def GetFilePairs(self):
    """Returns list of (input_file,output_file) tuples"""
    files = []

    if self.output_files:
      out_files = self.output_files
    else:
      out_files = [SignerInstructionConfig.DEFAULT_TEMPLATE]

    if len(out_files) == 1:
      out_file = out_files[0]

      # Check template generate unique output files
      if (len(self.input_files) > 1 and
          not re.search('(@BASENAME@)|(@ROOTNAME@)', out_file)):
        raise SignerOutputTemplateError('@BASENAME@ or @ROOTNAME@ required for'
                                        'templates with multiple input files')

      for in_file in self.input_files:
        files.append((in_file, self.FillTemplate(out_file, filename=in_file)))

    elif len(self.input_files) == len(out_files):
      for in_file, out_file in zip(self.input_files, out_files):
        files.append((in_file, self.FillTemplate(out_file, filename=in_file)))

    else:
      raise IndexError('Equal number of input_files and output_names required')


    return files

  def FillTemplate(self, template_str, filename=''):
    """Return string based on given template."""

    rep_dict = {'@BOARD@': self.board,
                '@CHANNEL@': self.channel,
                '@KEYSET@': self.keyset,
                '@TYPE@': self.artifact_type,
                '@VERSION@': self.version,
               }

    if filename:
      basename = os.path.basename(filename)
      rep_dict['@BASENAME@'] = basename
      rep_dict['@ROOTNAME@'] = os.path.splitext(basename)[0]

    return re.sub('@[A-Z]+@',
                  lambda x: rep_dict.get(x.group(0), x.group(0)),
                  template_str)


class BaseSigner(object):
  """Base Signer object."""

  # Override the following lists to enforce key requirements
  _required_keys = ()
  _required_keys_public = ()
  _required_keys_private = ()
  _required_keyblocks = ()

  def CheckKeyset(self, keyset):
    """Returns true if all required keys and keyblocks are in keyset."""
    for k in self._required_keys:
      if k not in keyset.keys:
        return False

    for k in self._required_keys_public:
      if not keyset.KeyExists(k, require_public=True):
        return False

    for k in self._required_keys_private:
      if not keyset.KeyExists(k, require_private=True):
        return False

    for kb in self._required_keyblocks:
      if not keyset.KeyblockExists(kb):
        return False

    return True

  def Sign(self, keyset, input_name, output_name):
    """Sign given input to output. Returns True if success."""
    raise NotImplementedError


class FutilitySigner(BaseSigner):
  """Base class for signers that use futility command."""

  def GetFutilityArgs(self, keyset, input_name, output_name):
    """Return list of arguments to use with futility."""
    raise NotImplementedError

  def Sign(self, keyset, input_name, output_name):
    if self.CheckKeyset(keyset):
      return RunFutility(self.GetFutilityArgs(keyset, input_name, output_name))
    return False


def RunFutility(args):
  """Runs futility with the given args, returns True if success"""
  cmd = ['futility']
  cmd += args
  return cros_build_lib.RunCommand(cmd, error_code_ok=True).returncode == 0
