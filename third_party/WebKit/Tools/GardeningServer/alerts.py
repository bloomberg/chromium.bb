# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import calendar
import datetime
import json
import webapp2

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
        alerts = memcache.get(AlertsHandler.MEMCACHE_ALERTS_KEY)
        if not alerts:
            return
        self.response.write(json.dumps(alerts, cls=DateTimeEncoder, indent=1))

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
        memcache.set(AlertsHandler.MEMCACHE_ALERTS_KEY, alerts)


app = webapp2.WSGIApplication([
    ('/alerts', AlertsHandler)
])
