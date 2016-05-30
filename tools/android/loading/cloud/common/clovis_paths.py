# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# BigQuery path constants.

# Name of the dataset.
BIGQUERY_DATASET = 'clovis_dataset'
# Name of the table used as a template for new tables.
BIGQUERY_TABLE_TEMPLATE = 'report'


# Trace path constants.

# Prefix for the loading trace database files.
TRACE_DATABASE_PREFIX = 'trace_database'


def GetBigQueryTableID(tag):
  """Returns the ID of the BigQuery table associated with tag. This ID is
  appended at the end of the table name.
  """
  # BigQuery table names can contain only alpha numeric characters and
  # underscores.
  return ''.join(c for c in tag if c.isalnum() or c == '_')


def GetBigQueryTableURL(project_name, tag):
  """Returns the full URL for the BigQuery table associated with tag."""
  table_id = GetBigQueryTableID(tag)
  table_name = BIGQUERY_DATASET + '.' + BIGQUERY_TABLE_TEMPLATE + '_' + table_id
  return 'https://bigquery.cloud.google.com/table/%s:%s' % (project_name,
                                                            table_name)
