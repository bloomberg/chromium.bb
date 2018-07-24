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
  # These figures are taken from running memory and speedometer telemetry
  # benchmarks, and are still subject to change as of 2018-01-24.
  STARTUP_STABILITY_THRESHOLD = 1.5
  COMMON_STABILITY_THRESHOLD = 1.75
  INTERACTION_STABILITY_THRESHOLD = 2.5

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

  def IsStableProfile(self):
    """Verify that the profiling has been stable.

    See ComputeStability for details.

    Returns:
      True if the profile was stable as described above.
    """
    (startup_stability, common_stability,
     interaction_stability) = [s[0] for s in self.ComputeStability()]

    stable = True
    if startup_stability > self.STARTUP_STABILITY_THRESHOLD:
      logging.error('Startup unstable: %.3f', startup_stability)
      stable = False
    if common_stability > self.COMMON_STABILITY_THRESHOLD:
      logging.error('Common unstable: %.3f', common_stability)
      stable = False
    if interaction_stability > self.INTERACTION_STABILITY_THRESHOLD:
      logging.error('Interaction unstable: %.3f', interaction_stability)
      stable = False

    return stable

  def ComputeStability(self):
    """Compute heuristic phase stability metrics.

    This computes the ratio in size of symbols between the union and
    intersection of all orderfile phases. Intuitively if this ratio is not too
    large it means that the profiling phases are stable with respect to the code
    they cover.

    Returns:
      ((float, int), (float, int), (float, int)) A heuristic stability metric
          for startup, common and interaction orderfile phases,
          respectively. Each metric is a pair of the ratio of symbol sizes as
          described above, and the size of the intersection.
    """
    (startup_intersection, startup_union,
     common_intersection, common_union,
     interaction_intersection, interaction_union,
     _, _) = self.GetCombinedOffsets()
    startup_intersection_size = self._processor.OffsetsPrimarySize(
        startup_intersection)
    common_intersection_size = self._processor.OffsetsPrimarySize(
        common_intersection)
    interaction_intersection_size = self._processor.OffsetsPrimarySize(
        interaction_intersection)
    startup_stability = self._SafeDiv(
        self._processor.OffsetsPrimarySize(startup_union),
        startup_intersection_size)
    common_stability = self._SafeDiv(
        self._processor.OffsetsPrimarySize(common_union),
        common_intersection_size)
    interaction_stability = self._SafeDiv(
        self._processor.OffsetsPrimarySize(interaction_union),
        interaction_intersection_size)
    return ((startup_stability, startup_intersection_size),
            (common_stability, common_intersection_size),
            (interaction_stability, interaction_intersection_size))

  def GetCombinedOffsets(self):
    """Get offsets for the union and intersection of orderfile phases.

    Returns:
      ([int] * 8) For each of startup, common, interaction and all, respectively
          the intersection and union offsets, in that order.
    """
    phase_offsets = self._GetOrderfilePhaseOffsets()
    assert phase_offsets
    if len(phase_offsets) == 1:
      logging.error('Only one run set, the combined offset files will all be '
                    'identical')

    startup_union = set(phase_offsets[0].startup)
    startup_intersection = set(phase_offsets[0].startup)
    common_union = set(phase_offsets[0].common)
    common_intersection = set(phase_offsets[0].common)
    interaction_union = set(phase_offsets[0].interaction)
    interaction_intersection = set(phase_offsets[0].interaction)
    for offsets in phase_offsets[1:]:
      startup_union |= set(offsets.startup)
      startup_intersection &= set(offsets.startup)
      common_union |= set(offsets.common)
      common_intersection &= set(offsets.common)
      interaction_union |= set(offsets.interaction)
      interaction_intersection &= set(offsets.interaction)
    return (startup_intersection, startup_union,
            common_intersection, common_union,
            interaction_intersection, interaction_union,
            (startup_union & common_union & interaction_union),
            (startup_union | common_union | interaction_union))

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
    assert self._profiles.GetPhases() == set([0,1]), 'Unexpected phases'
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

  @classmethod
  def _SafeDiv(cls, a, b):
    if not b:
      return None
    return float(a) / b


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
  stability = phaser.ComputeStability()
  print 'Stability: {:.2} {:.2} {:.2}'.format(*[s[0] for s in stability])
  print 'Sizes: {} {} {}'.format(*[s[1] for s in stability])
  if args.offset_output_base is not None:
    for name, offsets in zip(
        ['_for_memory', '_for_startup'],
        [phaser.GetOffsetsForMemoryFootprint(),
         phaser.GetOffsetsForStartup()]):
      with file(args.offset_output_base + name, 'w') as output:
        output.write('\n'.join(
            str(i) for i in (offsets.startup + offsets.common +
                             offsets.interaction)))
        output.write('\n')


if __name__ == '__main__':
  main()
