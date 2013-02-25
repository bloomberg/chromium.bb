# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from google.appengine.ext import db

class Experiment(db.Model):
    """ Models an individual experiment for storage in the database """
    name = db.StringProperty()
    description = db.TextProperty()
    instructions = db.TextProperty()
    device = db.StringProperty()
    owner = db.UserProperty()
    created = db.DateTimeProperty(auto_now_add=True)
    participants = db.ListProperty(str)
    feedback = db.ListProperty(db.Text)

class Treatment(db.Model):
    """ Models a single treament for experimentation """
    name = db.StringProperty()
    scores = db.ListProperty(int)

class Property(db.Model):
    """ Models a single parameter property change in a treatment """
    name = db.StringProperty()
    value = db.StringProperty()
