# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common_util
import emulation
import sandwich_runner
import task_manager


def NetworkSimulationTransformer(network_condition):
  """Creates a function that accepts a SandwichRunner as a parameter and sets
  network emulation options on it.

  Args:
    network_condition: The network condition to apply to the sandwich runner.

  Returns:
    A callback transforming the SandwichRunner given in argument accordingly
  """
  assert network_condition in emulation.NETWORK_CONDITIONS
  def Transformer(runner):
    assert isinstance(runner, sandwich_runner.SandwichRunner)
    runner.network_condition = network_condition
  return Transformer


class SandwichCommonBuilder(task_manager.Builder):
  """A builder for a graph of tasks, each prepares or invokes a SandwichRunner.
  """

  def __init__(self, android_device, url, output_directory,
               output_subdirectory):
    """Constructor.

    Args:
      android_device: The android DeviceUtils to run sandwich on or None to run
        it locally.
      url: URL to benchmark.
      output_directory: As in task_manager.Builder.__init__
      output_subdirectory: As in task_manager.Builder.__init__
    """
    task_manager.Builder.__init__(self, output_directory, output_subdirectory)
    self._android_device = android_device
    self._url = url
    self.default_final_tasks = []

    self.original_wpr_task = None

  def CreateSandwichRunner(self):
    """Create a runner for non benchmark purposes."""
    runner = sandwich_runner.SandwichRunner()
    runner.url = self._url
    runner.android_device = self._android_device
    return runner

  def OverridePathToWprArchive(self, original_wpr_path):
    """Sets the original WPR archive path's to be used.

    Args:
      original_wpr_path: Path of the original WPR archive to be used.
    """
    self.original_wpr_task = \
        self.CreateStaticTask('common/webpages.wpr', original_wpr_path)

  def PopulateWprRecordingTask(self):
    """Records the original WPR archive."""
    @self.RegisterTask('common/webpages.wpr')
    def BuildOriginalWpr():
      common_util.EnsureParentDirectoryExists(BuildOriginalWpr.path)
      runner = self.CreateSandwichRunner()
      runner.wpr_archive_path = BuildOriginalWpr.path
      runner.wpr_record = True
      runner.output_dir = BuildOriginalWpr.path[:-4] + '-run'
      runner.Run()

    self.original_wpr_task = BuildOriginalWpr
