# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import calendar
import datetime
import json
import logging
import webapp2
import zlib

from google.appengine.api import memcache
from google.appengine.ext import ndb

LOGGER = logging.getLogger(__name__)


class DateTimeEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, datetime.datetime):
            return calendar.timegm(obj.timetuple())
        # Let the base class default method raise the TypeError.
        return json.JSONEncoder.default(self, obj)


class AlertsJSON(ndb.Model):
    json = ndb.BlobProperty(compressed=True)
    date = ndb.DateTimeProperty(auto_now_add=True)


class AlertsHandler(webapp2.RequestHandler):
    MEMCACHE_ALERTS_KEY = 'alerts'

    # Has no 'response' member.
    # pylint: disable=E1101
    def send_json_headers(self):
        self.response.headers.add_header('Access-Control-Allow-Origin', '*')
        self.response.headers['Content-Type'] = 'application/json'

    # Has no 'response' member.
    # pylint: disable=E1101
    def send_json_data(self, data):
        self.send_json_headers()
        self.response.write(data)

    def generate_json_dump(self, alerts):
        return json.dumps(alerts, cls=DateTimeEncoder, indent=1)

    def get_from_memcache(self, memcache_key):
        compressed = memcache.get(memcache_key)
        if not compressed:
            self.send_json_headers()
            return
        uncompressed = zlib.decompress(compressed)
        self.send_json_data(uncompressed)

    def get(self):
        self.get_from_memcache(AlertsHandler.MEMCACHE_ALERTS_KEY)

    def save_alerts_to_history(self, alerts):
        last_entry = AlertsJSON.query().order(-AlertsJSON.date).get()
        last_alerts = json.loads(last_entry.json) if last_entry else {}

        # Only changes to the fields with 'alerts' in the name should cause a
        # new history entry to be saved.
        def alert_fields(alerts_json):
            filtered_json = {}
            for key, value in alerts_json.iteritems():
                if 'alerts' in key:
                    filtered_json[key] = value
            return filtered_json

        if alert_fields(last_alerts) != alert_fields(alerts):
            new_entry = AlertsJSON(json=self.generate_json_dump(alerts))
            new_entry.put()

    # Has no 'response' member.
    # pylint: disable=E1101
    def post_to_memcache(self, memcache_key, alerts):
        uncompressed = self.generate_json_dump(alerts)
        compression_level = 1
        compressed = zlib.compress(uncompressed, compression_level)
        memcache.set(memcache_key, compressed)

    def parse_alerts(self, alerts_json):
        try:
            alerts = json.loads(alerts_json)
        except ValueError:
            warning = 'content field was not JSON'
            self.response.set_status(400, warning)
            LOGGER.warn(warning)
            return

        alerts.update({'date': datetime.datetime.utcnow()})

        return alerts

    def update_alerts(self, memcache_key):
        alerts = self.parse_alerts(self.request.get('content'))
        if alerts:
            self.post_to_memcache(memcache_key, alerts)
            self.save_alerts_to_history(alerts)

    def post(self):
        self.update_alerts(AlertsHandler.MEMCACHE_ALERTS_KEY)


app = webapp2.WSGIApplication([
    ('/alerts', AlertsHandler)
])
