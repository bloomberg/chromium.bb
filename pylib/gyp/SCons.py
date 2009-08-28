#!/usr/bin/env python

"""
SCons generator.

This contains class definitions and supporting functions for generating
pieces of SCons files for the different types of GYP targets.
"""

import os


def WriteList(fp, list, prefix='',
                        separator=',\n    ',
                        preamble=None,
                        postamble=None):
  fp.write(preamble or '')
  fp.write((separator or ' ').join([prefix + l for l in list]))
  fp.write(postamble or '')


class TargetBase(object):
  """
  Base class for a SCons representation of a GYP target.
  """
  is_ignored = False
  target_prefix = ''
  target_suffix = ''
  def __init__(self, spec):
    self.spec = spec
  def full_product_name(self):
    """
    Returns the full name of the product being built:

      * Uses 'product_name' if it's set, else 'target_name'.
      * Appends SCons prefix and suffix variables for the target type.
      * Prepends 'product_dir' if set.
    """
    name = self.spec.get('product_name') or self.spec['target_name']
    name = self.target_prefix + name + self.target_suffix
    product_dir = self.spec.get('product_dir')
    if product_dir:
      name = os.path.join(product_dir, name)
    return name

  def write_input_files(self, fp):
    """
    Writes the definition of the input files (sources).
    """
    sources = self.spec.get('sources')
    if not sources:
      fp.write('\ninput_files = []\n')
      return
    preamble = '\ninput_files = GypFileList([\n    '
    postamble = ',\n])\n'
    WriteList(fp, map(repr, sources), preamble=preamble, postamble=postamble)

  def builder_call(self):
    """
    Returns the actual SCons builder call to build this target.
    """
    name = self.full_product_name()
    return 'env.%s(%r, input_files)' % (self.builder_name, name)
  def write_target(self, fp, pre=''):
    """
    Writes the lines necessary to build this target.
    """
    fp.write('\n' + pre)
    fp.write('_outputs = %s\n' % self.builder_call())
    fp.write('target_files.extend(_outputs)\n')


class NoneTarget(TargetBase):
  """
  A GYP target type of 'none', implicitly or explicitly.
  """
  def write_target(self, fp, pre=''):
    fp.write('\ntarget_files.extend(input_files)\n')


class SettingsTarget(TargetBase):
  """
  A GYP target type of 'settings'.
  """
  is_ignored = True


class ProgramTarget(TargetBase):
  """
  A GYP target type of 'executable'.
  """
  builder_name = 'GypProgram'
  target_prefix = '${PROGPREFIX}'
  target_suffix = '${PROGSUFFIX}'

  # TODO:  remove these subclass methods by moving the env.File()
  # into the base class.
  def write_target(self, fp):
    fp.write('\n_program = env.File(%r)' % self.full_product_name())
    super(ProgramTarget, self).write_target(fp)
  def builder_call(self):
    return 'env.GypProgram(_program, input_files)'


class StaticLibraryTarget(TargetBase):
  """
  A GYP target type of 'static_library'.
  """
  builder_name = 'GypStaticLibrary'
  # TODO:  enable these
  #target_prefix = '${LIBPREFIX}'
  #target_suffix = '${LIBSUFFIX}'


class SharedLibraryTarget(TargetBase):
  """
  A GYP target type of 'shared_library'.
  """
  builder_name = 'GypSharedLibrary'
  # TODO:  enable these
  #target_prefix = '${SHLIBPREFIX}'
  #target_suffix = '${SHLIBSUFFIX}'


class LoadableModuleTarget(TargetBase):
  """
  A GYP target type of 'loadable_module'.
  """
  builder_name = 'GypLoadableModule'
  # TODO:  enable these
  #target_prefix = '${SHLIBPREFIX}'
  #target_suffix = '${SHLIBSUFFIX}'


TargetMap = {
  None : NoneTarget,
  'none' : NoneTarget,
  'settings' : SettingsTarget,
  'executable' : ProgramTarget,
  'static_library' : StaticLibraryTarget,
  'shared_library' : SharedLibraryTarget,
  'loadable_module' : LoadableModuleTarget,
}

def Target(spec):
  return TargetMap[spec.get('type')](spec)
