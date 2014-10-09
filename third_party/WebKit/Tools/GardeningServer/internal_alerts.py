# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import alerts
import json
import webapp2

from google.appengine.api import users


class InternalAlertsHandler(alerts.AlertsHandler):
    INTERNAL_ALERTS_TYPE = 'internal-alerts'

    # Has no 'request' member.
    # Has no 'response' member.
    # Use of super on an old style class.
    # pylint: disable=E1002,E1101
    def get(self):
        # Require users to be logged to see builder alerts from private/internal
        # trees.
        user = users.get_current_user()
        if not user:
            alerts = {}
            alerts.update({
                'date': datetime.datetime.utcnow(),
                'redirect-url': users.create_login_url(self.request.uri)})
            uncompressed = super(InternalAlertsHandler,
                                 self).generate_json_dump(alerts)
            super(InternalAlertsHandler, self).send_json_data(uncompressed)
            return

        email = user.email()
        if not email.endswith('@google.com'):
            self.response.set_status(403, 'invalid user')
            return

        super(InternalAlertsHandler, self).get_from_memcache(
            InternalAlertsHandler.INTERNAL_ALERTS_TYPE)

    def post(self):
        self.update_alerts(InternalAlertsHandler.INTERNAL_ALERTS_TYPE)


app = webapp2.WSGIApplication([
    ('/internal-alerts', InternalAlertsHandler)])
