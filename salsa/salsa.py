# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import jinja2
import logging
import os
import random
import webapp2

from google.appengine.api import users
from models import Experiment, Property, Treatment


jinja_environment = jinja2.Environment(autoescape=True,
    loader=jinja2.FileSystemLoader(os.path.join(
                                        os.path.dirname(__file__),
                                        'templates')))
def load_experiment(experiment_id):
    experiment = Experiment.gql('WHERE __key__ = KEY(:1)', experiment_id).get()
    treatments = Treatment.gql('WHERE ANCESTOR IS KEY(:1)', experiment_id)
    properties = Property.gql('WHERE ANCESTOR IS KEY(:1)', experiment_id)

    return (experiment, treatments, properties)

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
        self.redirect(users.create_logout_url('/'))

class Participate(webapp2.RequestHandler):
    """ Have a participant actually run the experiment """
    def get(self):
        exp, treatments, props = load_experiment(self.request.get('exp_key'))

        # Shuffle the treatments using the user as a seed to provide
        # a consistent but random ordering on a per user basis.
        shuffler = random.Random(users.get_current_user())
        shuffled = [t for t in treatments]
        shuffler.shuffle(shuffled)

        template = jinja_environment.get_template('participate.html')
        self.response.out.write(template.render(user=users.get_current_user(),
                                                experiment=exp,
                                                treatments=shuffled,
                                                properties=props))

class Submit(webapp2.RequestHandler):
    """ Accept the rankings a user submitted from their experiment """
    def get(self):
        exp, treatments, _ = load_experiment(self.request.get('exp_key'))
        ranking = self.request.get('ranking').split('>')
        ranking = [r.split('=') for r in ranking]

        template = jinja_environment.get_template('submit.html')
        self.response.out.write(template.render(user=users.get_current_user(),
                                                experiment=exp,
                                                treatments=treatments,
                                                ranking=ranking))

class ViewExperiment(webapp2.RequestHandler):
    """ Shows the details for an experiment if the user is it's owner """
    def get(self):
        exp, treatments, props = load_experiment(self.request.get('exp_key'))
        user = users.get_current_user()
        has_access = (user == exp.owner or users.is_current_user_admin())
        experiment_url = self.request.url.replace('view', 'participate')

        template = jinja_environment.get_template('view.html')
        self.response.out.write(template.render(has_access=has_access,
                                                experiment_url=experiment_url,
                                                experiment=exp,
                                                treatments=treatments,
                                                properties=props))

app = webapp2.WSGIApplication([
                                ('/', MainPage),
                                ('/error', Error),
                                ('/logout', Logout),
                                ('/participate', Participate),
                                ('/submit', Submit),
                                ('/view', ViewExperiment),
                                ('/.+', Error),
                              ], debug=True)
