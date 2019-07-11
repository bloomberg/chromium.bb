# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from cli_tools.soundwave.studies import health_study
from cli_tools.soundwave.studies import v8_study
from core.external_modules import pandas


_STUDIES = {'health': health_study, 'v8': v8_study}

NAMES = sorted(_STUDIES)


def GetStudy(study):
  return _STUDIES[study]


def PostProcess(df):
  # Snap stories on the same test run to the same timestamp.
  df['timestamp'] = df.groupby(
      ['test_suite', 'bot', 'point_id'])['timestamp'].transform('min')

  # We use all runs on the latest day for each quarter as reference.
  df['quarter'] = df['timestamp'].dt.to_period('Q')
  df['reference'] = df['timestamp'].dt.date == df.groupby(
      'quarter')['timestamp'].transform('max').dt.date

  # Change units for values in ms to seconds, and percent values.
  is_ms_unit = df['units'].str.startswith('ms_')
  df.loc[is_ms_unit, 'value'] = df['value'] / 1000

  is_percentage = df['units'].str.startswith('n%_')
  df.loc[is_percentage, 'value'] = df['value'] * 100

  return df
