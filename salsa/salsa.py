# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import jinja2
import os
import webapp2

from google.appengine.api import users
from models import Experiment, Property, Treatment


jinja_environment = jinja2.Environment(autoescape=True,
    loader=jinja2.FileSystemLoader(os.path.join(
                                        os.path.dirname(__file__),
                                        'templates')))

class MainPage(webapp2.RequestHandler):
    """ Displays the Create form and a list of the user's experiments """
    def get(self):
        user = users.get_current_user()
        exps = Experiment.gql('WHERE owner = :1', user)

        template = jinja_environment.get_template('index.html')
        self.response.out.write(template.render(user=user, experiments=exps))

class Error(webapp2.RequestHandler):
    """ Displays an error message """
    def get(self):
        template = jinja_environment.get_template('error.html')
        self.response.out.write(template.render())

class Logout(webapp2.RequestHandler):
    """ Logs the user out and redirects to the main page """
    def get(self):
        user = users.get_current_user()
        self.redirect(users.create_logout_url("/"))

class ViewExperiment(webapp2.RequestHandler):
    """ Shows the details for an experiment if the user is it's owner """
    def get(self):
        user = users.get_current_user()
        key = self.request.get('exp_key')
        exp = Experiment.gql('WHERE __key__ = KEY(:1)', key).get()

        has_access = (user == exp.owner or users.is_current_user_admin())
        treatments = properties = experiment_url = None
        if has_access:
            treatments = Treatment.gql('WHERE ANCESTOR IS KEY(:1)', key)
            properties = Property.gql('WHERE ANCESTOR IS KEY(:1)', key)
            experiment_url = self.request.url.replace("view", "participate")

        template = jinja_environment.get_template('view.html')
        self.response.out.write(template.render(has_access=has_access,
                                                experiment_url=experiment_url,
                                                experiment=exp,
                                                treatments=treatments,
                                                properties=properties))

app = webapp2.WSGIApplication([
                                ('/', MainPage),
                                ('/error', Error),
                                ('/logout', Logout),
                                ('/view', ViewExperiment),
                                ('/.+', Error),
                              ], debug=True)
