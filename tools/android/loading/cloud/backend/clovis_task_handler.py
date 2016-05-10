# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from cloud.common.clovis_task import ClovisTask
from failure_database import FailureDatabase
from trace_task_handler import TraceTaskHandler


class ClovisTaskHandler(object):
  """Handles all the supported clovis tasks."""

  def __init__(self, base_path, failure_database, google_storage_accessor,
               binaries_path, logger, instance_name=None):
    """Creates a ClovisTaskHandler.

    Args:
      base_path(str): Base path where results are written.
      binaries_path(str): Path to the directory where Chrome executables are.
      instance_name(str, optional): Name of the ComputeEngine instance.
    """
    self._failure_database = failure_database
    self._handlers = {
        'trace': TraceTaskHandler(
            os.path.join(base_path, 'trace'), failure_database,
            google_storage_accessor, binaries_path, logger, instance_name)}

  def Run(self, clovis_task):
    """Runs a clovis_task.

    Args:
      clovis_task(ClovisTask): The task to run.

    Returns:
      boolean: True in case of success, False if a failure has been added to the
               failure database.
    """
    handler = self._handlers.get(clovis_task.Action())
    if not handler:
      self._logger.error('Unsupported task action: %s' % clovis_task.Action())
      self._failure_database.AddFailure('unsupported_action',
                                        clovis_task.Action())
      return False
    return handler.Run(clovis_task)
