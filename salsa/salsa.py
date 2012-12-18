# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import filters
import jinja2
import logging
import os
import random
import urllib
import webapp2

from google.appengine.api import users
from google.appengine.ext import db
from models import Experiment, Property, Treatment


jinja_environment = jinja2.Environment(autoescape=True,
    loader=jinja2.FileSystemLoader(os.path.join(
                                        os.path.dirname(__file__),
                                        'templates')))
jinja_environment.filters['average'] = filters.average
jinja_environment.filters['row_class'] = filters.row_class
jinja_environment.filters['sign_test'] = filters.sign_test
jinja_environment.filters['variance'] = filters.variance

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
        msg = self.request.get('msg')
        template = jinja_environment.get_template('error.html')
        self.response.out.write(template.render(msg=msg))

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
        ranks = self.request.get('ranking').split('>')
        ranks = [r.split('=') for r in ranks]

        if str(users.get_current_user()) in exp.participants:
            return self.fail('You have already taken this experiment.')

        if treatments.count() != sum([len(r) for r in ranks]):
            return self.fail('You must rank ALL treatments to submit.')

        score = 0
        updated_treatments = []
        for same_rank in ranks:
            for treatment in same_rank:
                results = Treatment.gql('WHERE __key__ = KEY(:1) '
                                        'AND ANCESTOR IS KEY(:2)',
                                        treatment, str(exp.key()))
                if results.count() != 1:
                    return self.fail('Unable to find treatment: ' + treatment)
                treatment_data = results.get()
                treatment_data.scores.append(score)
                updated_treatments.append(treatment_data)
            score += len(same_rank)

        exp.participants.append(str(users.get_current_user()))
        self.save(exp, updated_treatments)

        template = jinja_environment.get_template('submit.html')
        self.response.out.write(template.render(user=users.get_current_user(),
                                                experiment=exp,
                                                treatments=treatments,
                                                ranking=ranks))

    @db.transactional
    def save(self, experiment, treatments):
        """ Atomically write all the changes """
        experiment.put()
        for treatment in treatments:
            treatment.put()

    def fail(self, message):
        arguments = urllib.urlencode({'msg': message})
        self.redirect('error?' + arguments)


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
