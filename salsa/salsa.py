# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import csv
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

        if treatments.count() != sum([len(r) for r in ranks]):
            return self.fail('You must rank ALL treatments to submit.')

        participant = str(users.get_current_user())
        feedback = self.request.get('feedback')
        if str(users.get_current_user()) not in exp.participants:
            exp.participants.append(participant)
            exp.feedback.append('')
        participant_number = exp.participants.index(participant)

        while participant_number >= len(exp.feedback):
            exp.feedback.append('')
        exp.feedback[participant_number] = feedback

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
                if participant_number >= len(treatment_data.scores):
                    treatment_data.scores.append(score)
                else:
                    treatment_data.scores[participant_number] = score
                updated_treatments.append(treatment_data)
            score += len(same_rank)

        self.save(exp, updated_treatments)

        template = jinja_environment.get_template('submit.html')
        self.response.out.write(template.render(experiment=exp))

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

class DownloadCSV(webapp2.RequestHandler):
    """ Download a CSV file of the results for a given experiment """
    def get(self):
        exp, treatments, props = load_experiment(self.request.get('exp_key'))
        user = users.get_current_user()
        has_access = (user == exp.owner or users.is_current_user_admin())
        if not has_access:
            msg = 'Sorry, you do not have permission to access these results.'
            arguments = urllib.urlencode({'msg': msg})
            self.redirect('error?' + arguments)

        self.response.headers['Content-Type'] = 'application/csv'
        self.response.headers['Content-Disposition'] = 'filename="results.csv"'
        writer = csv.writer(self.response.out)

        writer.writerow(['Name', exp.name])
        writer.writerow(['Owner', exp.owner])

        writer.writerow([])
        writer.writerow(['Treatments:'])
        for treatment in treatments:
            properties = ['%s = %s' % (prop.name, str(prop.value))
                          for prop in props
                          if prop.parent_key() == treatment.key()]
            writer.writerow([treatment.name] + properties)

        writer.writerow([])
        writer.writerow(['Raw Results:'])
        writer.writerow([''] + [treatment.name for treatment in treatments])
        for i, participant in enumerate(exp.participants):
            results = [treatment.scores[i] for treatment in treatments]
            writer.writerow([participant] + results + [exp.feedback[i]])

        writer.writerow([])
        writer.writerow(['Sign-test P-values:'])
        writer.writerow([''] + [treatment.name for treatment in treatments])
        for treatment1 in treatments:
            values = [filters.sign_test(treatment1.scores, treatment2.scores)
                      for treatment2 in treatments]
            writer.writerow([treatment1.name] + values)

app = webapp2.WSGIApplication([
                                ('/', MainPage),
                                ('/csv', DownloadCSV),
                                ('/error', Error),
                                ('/logout', Logout),
                                ('/participate', Participate),
                                ('/submit', Submit),
                                ('/view', ViewExperiment),
                                ('/.+', Error),
                              ], debug=True)
