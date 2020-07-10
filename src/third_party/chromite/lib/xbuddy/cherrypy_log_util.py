# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Logging via CherryPy."""

from __future__ import print_function

import re

# cherrypy may not be available outside the chroot.
try:
  import cherrypy  # pylint: disable=import-error
except ImportError:
  cherrypy = None

from chromite.lib import cros_logging as logging


class Loggable(object):
  """Provides a log method, with automatic log tag generation."""
  _CAMELCASE_RE = re.compile('(?<=.)([A-Z])')

  def _Log(self, message, *args):
    LogWithTag(
        self._CAMELCASE_RE.sub(r'_\1', self.__class__.__name__).upper(),
        message, *args)


def LogWithTag(tag, message, *args):
  # CherryPy log doesn't seem to take any optional args, so we just handle
  # args by formatting them into message.
  if cherrypy:
    cherrypy.log(message % args, context=tag)
  else:
    logging.info(message, *args)


def UpdateConfig(configs):
  """Updates the cherrypy config.

  Args:
    configs: A dictionary with all cherrypy configs.
  """
  if cherrypy:
    cherrypy.config.update(configs)
