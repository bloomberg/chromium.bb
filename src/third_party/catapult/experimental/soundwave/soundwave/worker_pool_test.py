# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import pandas  # pylint: disable=import-error
import shutil
import sqlite3
import tempfile
import unittest

from soundwave import pandas_sqlite
from soundwave import worker_pool


def TestWorker(args):
  con = sqlite3.connect(args.database_file)

  def Process(item):
    # Add item to the database.
    df = pandas.DataFrame({'item': [item]})
    df.to_sql('items', con, index=False, if_exists='append')

  worker_pool.Process = Process


class TestWorkerPool(unittest.TestCase):
  def testWorkerPoolRun(self):
    tempdir = tempfile.mkdtemp()
    try:
      args = argparse.Namespace()
      args.database_file = os.path.join(tempdir, 'test.db')
      args.processes = 3
      schema = pandas_sqlite.DataFrame([('item', int)])
      items = range(20)  # We'll write these in the database.
      con = sqlite3.connect(args.database_file)
      try:
        pandas_sqlite.CreateTableIfNotExists(con, 'items', schema)
        with open(os.devnull, 'w') as devnull:
          worker_pool.Run(
              'Processing:', TestWorker, args, items, stream=devnull)
        df = pandas.read_sql('SELECT * FROM items', con)
        # Check all of our items were written.
        self.assertItemsEqual(df['item'], items)
      finally:
        con.close()
    finally:
      shutil.rmtree(tempdir)


if __name__ == '__main__':
  unittest.main()
