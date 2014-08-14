# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import calendar
import datetime
import json
import webapp2
import zlib

from google.appengine.api import memcache


class DateTimeEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, datetime.datetime):
            return calendar.timegm(obj.timetuple())
        # Let the base class default method raise the TypeError
        return json.JSONEncoder.default(self, obj)


class AlertsHandler(webapp2.RequestHandler):
    MEMCACHE_ALERTS_KEY = 'alerts'

    def get(self):
        self.response.headers.add_header('Access-Control-Allow-Origin', '*')
        self.response.headers['Content-Type'] = 'application/json'
        compressed = memcache.get(AlertsHandler.MEMCACHE_ALERTS_KEY)
        if not compressed:
            return
        uncompressed = zlib.decompress(compressed)
        self.response.write(uncompressed)

    def post(self):
        try:
            alerts = json.loads(self.request.get('content'))
        except ValueError:
            self.response.set_status(400, 'content field was not JSON')
            return
        alerts.update({
            'date': datetime.datetime.utcnow(),
            'alerts': alerts['alerts']
        })
        uncompressed = json.dumps(alerts, cls=DateTimeEncoder, indent=1)
        compression_level = 1
        compressed = zlib.compress(uncompressed, compression_level)
        memcache.set(AlertsHandler.MEMCACHE_ALERTS_KEY, compressed)


app = webapp2.WSGIApplication([
    ('/alerts', AlertsHandler)
])
