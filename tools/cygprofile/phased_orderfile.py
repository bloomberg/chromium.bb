#!/usr/bin/env vpython
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for creating a phased orderfile.

The profile dump format is described in process_profiles.py. These tools assume
profiling has been done with two phases.

The first phase, labeled 0 in the filename, is called "startup" and the second,
labeled 1, is called "interaction". These two phases are used to create an
orderfile with three parts: the code touched only in startup, the code
touched only during interaction, and code common to the two phases. We refer to
these parts as the orderfile phases.

Example invocation, with PROFILE_DIR the location of the profile data pulled
from a device and LIBTYPE either monochrome or chrome as appropriate.
./tools/cygprofile/phased_orderfile.py \
    --profile-directory=PROFILE_DIR \
    --instrumented-build-dir=out-android/Orderfile/ \
    --library-name=libLIBTYPE.so --offset-output-base=PROFILE_DIR/offset
"""

import argparse
import collections
import glob
import itertools
import logging
import os.path

import process_profiles


# Files matched when using this script to analyze directly (see main()).
PROFILE_GLOB = 'profile-hitmap-*.txt_*'


OrderfilePhaseOffsets = collections.namedtuple(
    'OrderfilePhaseOffsets', ('startup', 'common', 'interaction'))


class PhasedAnalyzer(object):
  """A class which collects analysis around phased orderfiles.

  It maintains common data such as symbol table information to make analysis
  more convenient.
  """
  # The process name of the browser as used in the profile dumps.
  BROWSER = 'browser'

  def __init__(self, profiles, processor):
    """Intialize.

    Args:
      profiles (ProfileManager) Manager of the profile dump files.
      processor (SymbolOffsetProcessor) Symbol table processor for the dumps.
    """
    self._profiles = profiles
    self._processor = processor

    # These members cache various computed values.
    self._phase_offsets = None
    self._annotated_offsets = None
    self._process_list = None

  def GetOffsetsForMemoryFootprint(self):
    """Get offsets organized to minimize the memory footprint.

    The startup, common and interaction offsets are computed for each
    process. Any symbols used by one process in startup or interaction that are
    used in a different phase by another process are moved to the common
    section. This should minimize the memory footprint by keeping startup- or
    interaction-only pages clean, at the possibly expense of startup time, as
    more of the common section will need to be loaded. To mitigate that effect,
    symbols moved from startup are placed at the beginning of the common
    section, and those moved from interaction are placed at the end.

    Browser startup symbols are placed at the beginning of the startup section
    in the hope of working out with native library prefetching to minimize
    startup time.

    Returns:
      OrdrerfilePhaseOffsets as described above.
    """
    startup = []
    common_head = []
    common = []
    common_tail = []
    interaction = []

    process_offsets = {p: self._GetCombinedProcessOffsets(p)
                       for p in self._GetProcessList()}
    assert self.BROWSER in process_offsets.keys()

    any_startup = set()
    any_interaction = set()
    any_common = set()
    for offsets in process_offsets.itervalues():
      any_startup |= set(offsets.startup)
      any_interaction |= set(offsets.interaction)
      any_common |= set(offsets.common)

    already_added = set()
    # This helper function splits |offsets|, adding to |alternate| all offsets
    # that are in |interfering| or are already known to be common, and otherwise
    # adding to |target|.
    def add_process_offsets(offsets, interfering, target, alternate):
      for o in offsets:
        if o in already_added:
          continue
        if o in interfering or o in any_common:
          alternate.append(o)
        else:
          target.append(o)
        already_added.add(o)

    # This helper updates |common| with new members of |offsets|.
    def add_common_offsets(offsets):
      for o in offsets:
        if o not in already_added:
          common.append(o)
          already_added.add(o)

    add_process_offsets(process_offsets[self.BROWSER].startup,
                        any_interaction, startup, common_head)
    add_process_offsets(process_offsets[self.BROWSER].interaction,
                        any_startup, interaction, common_tail)
    add_common_offsets(process_offsets[self.BROWSER].common)

    for p in process_offsets:
      if p == self.BROWSER:
        continue
      add_process_offsets(process_offsets[p].startup,
                          any_interaction, startup, common_head)
      add_process_offsets(process_offsets[p].interaction,
                          any_startup, interaction, common_tail)
      add_common_offsets(process_offsets[p].common)

    return OrderfilePhaseOffsets(
        startup=startup,
        common=(common_head + common + common_tail),
        interaction=interaction)

  def GetOffsetsForStartup(self):
    """Get offsets organized to minimize startup time.

    The startup, common and interaction offsets are computed for each
    process. Any symbol used by one process in interaction that appears in a
    different phase in another process is moved to common, but any symbol that
    appears in startup for *any* process stays in startup.

    This should maximize startup performance at the expense of increasing the
    memory footprint, as some startup symbols will not be able to page out.

    The startup symbols in the browser process appear first in the hope of
    working out with native library prefetching to minimize startup time.
    """
    startup = []
    common = []
    interaction = []
    already_added = set()

    process_offsets = {p: self._GetCombinedProcessOffsets(p)
                       for p in self._GetProcessList()}
    startup.extend(process_offsets[self.BROWSER].startup)
    already_added |= set(process_offsets[self.BROWSER].startup)
    common.extend(process_offsets[self.BROWSER].common)
    already_added |= set(process_offsets[self.BROWSER].common)
    interaction.extend(process_offsets[self.BROWSER].interaction)
    already_added |= set(process_offsets[self.BROWSER].interaction)

    for process, offsets in process_offsets.iteritems():
      if process == self.BROWSER:
        continue
      startup.extend(o for o in offsets.startup
                     if o not in already_added)
      already_added |= set(offsets.startup)
      common.extend(o for o in offsets.common
                     if o not in already_added)
      already_added |= set(offsets.common)
      interaction.extend(o for o in offsets.interaction
                     if o not in already_added)
      already_added |= set(offsets.interaction)

    return OrderfilePhaseOffsets(
        startup=startup, common=common, interaction=interaction)

  def _GetCombinedProcessOffsets(self, process):
    """Combine offsets across runs for a particular process.

    Args:
      process (str) The process to combine.

    Returns:
      OrderfilePhaseOffsets, the startup, common and interaction offsets for the
      process in question. The offsets are sorted arbitrarily.
    """
    (startup, common, interaction) = ([], [], [])
    assert self._profiles.GetPhases() == set([0,1]), (
        'Unexpected phases {}'.format(self._profiles.GetPhases()))
    for o in self._GetAnnotatedOffsets():
      startup_count = o.Count(0, process)
      interaction_count = o.Count(1, process)
      if not startup_count and not interaction_count:
        continue
      if startup_count and interaction_count:
        common.append(o.Offset())
      elif startup_count:
        startup.append(o.Offset())
      else:
        interaction.append(o.Offset())
    return OrderfilePhaseOffsets(
        startup=startup, common=common, interaction=interaction)

  def _GetAnnotatedOffsets(self):
    if self._annotated_offsets is None:
      self._annotated_offsets = self._profiles.GetAnnotatedOffsets()
      self._processor.TranslateAnnotatedSymbolOffsets(self._annotated_offsets)
      # A warning for missing offsets has already been emitted in
      # TranslateAnnotatedSymbolOffsets.
      self._annotated_offsets = filter(
          lambda offset: offset.Offset() is not None,
          self._annotated_offsets)
    return self._annotated_offsets

  def _GetProcessList(self):
    if self._process_list is None:
      self._process_list = set()
      for o in self._GetAnnotatedOffsets():
        self._process_list.update(o.Processes())
    return self._process_list

  def _GetOrderfilePhaseOffsets(self):
    """Compute the phase offsets for each run.

    Returns:
      [OrderfilePhaseOffsets] Each run corresponds to an OrderfilePhaseOffsets,
          which groups the symbol offsets discovered in the runs.
    """
    if self._phase_offsets is not None:
      return self._phase_offsets

    assert self._profiles.GetPhases() == set([0, 1]), (
        'Unexpected phases {}'.format(self._profiles.GetPhases()))
    self._phase_offsets = []
    for first, second in zip(self._profiles.GetRunGroupOffsets(phase=0),
                             self._profiles.GetRunGroupOffsets(phase=1)):
      all_first_offsets = self._processor.GetReachedOffsetsFromDump(first)
      all_second_offsets = self._processor.GetReachedOffsetsFromDump(second)
      first_offsets_set = set(all_first_offsets)
      second_offsets_set = set(all_second_offsets)
      common_offsets_set = first_offsets_set & second_offsets_set
      first_offsets_set -= common_offsets_set
      second_offsets_set -= common_offsets_set

      startup = [x for x in all_first_offsets
                 if x in first_offsets_set]

      interaction = [x for x in all_second_offsets
                     if x in second_offsets_set]

      common_seen = set()
      common = []
      for x in itertools.chain(all_first_offsets, all_second_offsets):
        if x in common_offsets_set and x not in common_seen:
          common_seen.add(x)
          common.append(x)

      self._phase_offsets.append(OrderfilePhaseOffsets(
          startup=startup,
          interaction=interaction,
          common=common))

    return self._phase_offsets


def _CreateArgumentParser():
  parser = argparse.ArgumentParser(
      description='Compute statistics on phased orderfiles')
  parser.add_argument('--profile-directory', type=str, required=True,
                      help=('Directory containing profile runs. Files '
                            'matching {} are used.'.format(PROFILE_GLOB)))
  parser.add_argument('--instrumented-build-dir', type=str,
                      help='Path to the instrumented build (eg, out/Orderfile)',
                      required=True)
  parser.add_argument('--library-name', default='libchrome.so',
                      help=('Chrome shared library name (usually libchrome.so '
                            'or libmonochrome.so'))
  parser.add_argument('--offset-output-base', default=None, type=str,
                      help=('If present, a base name to output offsets to. '
                            'No offsets are output if this is missing. The '
                            'base name is suffixed with _for_memory and '
                            '_for_startup, corresponding to the two sets of '
                            'offsets produced.'))
  return parser


def main():
  logging.basicConfig(level=logging.INFO)
  parser = _CreateArgumentParser()
  args = parser.parse_args()
  profiles = process_profiles.ProfileManager(itertools.chain.from_iterable(
      glob.glob(os.path.join(d, PROFILE_GLOB))
      for d in args.profile_directory.split(',')))
  processor = process_profiles.SymbolOffsetProcessor(os.path.join(
      args.instrumented_build_dir, 'lib.unstripped', args.library_name))
  phaser = PhasedAnalyzer(profiles, processor)
  for name, offsets in (
      ('_for_memory', phaser.GetOffsetsForMemoryFootprint()),
      ('_for_startup', phaser.GetOffsetsForStartup())):
    logging.info('%s Offset sizes (KiB):\n'
                 '%s startup\n%s common\n%s interaction',
                 name, processor.OffsetsPrimarySize(offsets.startup) / 1024,
                 processor.OffsetsPrimarySize(offsets.common) / 1024,
                 processor.OffsetsPrimarySize(offsets.interaction) / 1024)
    if args.offset_output_base is not None:
      with file(args.offset_output_base + name, 'w') as output:
        output.write('\n'.join(
            str(i) for i in (offsets.startup + offsets.common +
                             offsets.interaction)))
        output.write('\n')


if __name__ == '__main__':
  main()
