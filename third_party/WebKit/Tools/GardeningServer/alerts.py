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

LOGGER = logging.getLogger(__name__)


class DateTimeEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, datetime.datetime):
            return calendar.timegm(obj.timetuple())
        # Let the base class default method raise the TypeError.
        return json.JSONEncoder.default(self, obj)


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

    # Has no 'response' member.
    # pylint: disable=E1101
    def post_to_memcache(self, memcache_key):
        try:
            alerts = json.loads(self.request.get('content'))
        except ValueError:
            warning = 'content field was not JSON'
            self.response.set_status(400, warning)
            LOGGER.warn(warning)
            return
        alerts.update({
            'date': datetime.datetime.utcnow(),
            'alerts': alerts['alerts']
        })
        uncompressed = self.generate_json_dump(alerts)
        compression_level = 1
        compressed = zlib.compress(uncompressed, compression_level)
        memcache.set(memcache_key, compressed)

    def post(self):
        self.post_to_memcache(AlertsHandler.MEMCACHE_ALERTS_KEY)


app = webapp2.WSGIApplication([
    ('/alerts', AlertsHandler)
])
