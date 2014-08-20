# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from datetime import datetime
import logging

from google.appengine.ext import db

from model.datastorefile import DataStoreFile


class TestFile(DataStoreFile):  # pylint: disable=W0232
  master = db.StringProperty()
  builder = db.StringProperty()
  test_type = db.StringProperty()
  build_number = db.IntegerProperty()

  @property
  def file_information(self):
    return ("master: %s, builder: %s, test_type: %s, build_number: %r, "
            "name: %s.") % (self.master, self.builder, self.test_type,
            self.build_number, self.name)

  @classmethod
  def delete_file(cls, key, master, builder, test_type, build_number, name,
      before, limit):
    if key:
      record = db.get(key)
      if not record:
        logging.warning("File not found, key: %s.", key)
        return 0

      record.delete_all()
      return 1

    files = cls.get_files(master, builder, test_type, build_number, name,
        before, load_data=False, limit=limit)
    if not files:
      logging.warning(("File not found, master: %s, builder: %s, test_type:%s, "
                       "name: %s, before: %s."), master, builder, test_type,
          name, before)
      return 0

    for record in files:
      record.delete_all()

    return len(files)

  @classmethod
  def get_files(cls, master, builder, test_type, build_number, name,
      before=None, load_data=True, limit=1):
    query = TestFile.all()
    if master:
      query = query.filter("master =", master)
    if builder:
      query = query.filter("builder =", builder)
    if test_type:
      query = query.filter("test_type =", test_type)
    if build_number == 'latest':
      query = query.sort('-build_number')
    elif build_number is not None and build_number != 'None':
      # TestFile objects that were added to the persistent DB before the
      # build_number
      # property was added will have a default build_number of None.  When
      # those files
      # appear in a list view generated from templates/showfilelist.html,
      # the URL
      # to get the individual file contents will have '&buildnumber=None' in it.
      # Hence, we ignore the string 'None' in build_number query parameter.
      try:
        query = query.filter("build_number =", int(build_number))
      except (TypeError, ValueError):
        logging.error(
            "Could not convert buildnumber parameter '%s' to an integer",
            build_number)
        return None
    if name:
      query = query.filter("name =", name)
    if before:
      date = datetime.strptime(before, "%Y-%m-%dT%H:%M:%SZ")
      query = query.filter("date <", date)

    files = query.order("-date").fetch(limit)
    if load_data:
      for record in files:
        record.load_data()

    return files

  @classmethod
  def save_file(cls, record, data):
    if record.save(data):
      status_string = "Saved file. %s" % record.file_information
      status_code = 200
    else:
      status_string = "Couldn't save file. %s" % record.file_information
      status_code = 500
    return status_string, status_code

  @classmethod
  def add_file(cls, master, builder, test_type, build_number, name, data):
    record = TestFile()
    record.master = master
    record.builder = builder
    record.test_type = test_type
    record.build_number = build_number
    record.name = name
    return cls.save_file(record, data)

  def save(self, data):
    if not self.save_data(data):
      return False

    self.date = datetime.now()
    self.put()

    return True

  def delete_all(self):
    self.delete_data()
    self.delete()
