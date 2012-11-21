# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import webapp2

from google.appengine.api import users
from models import Experiment, Property, Treatment

class CreateNewExperiment(webapp2.RequestHandler):
    def parse_experiment_details(self):
        exp = {
            'name': self.request.get('exp_name'),
            'owner': users.get_current_user()
        }
        if not exp.get('name') or not exp.get('owner'):
            return None
        return exp

    def parse_treatments(self):
        # First, figure out how many treatments/properties there are
        treatment_numbers = {}
        for arg in self.request.arguments():
            matches = re.match(r'^treat([0-9]*)_prop([0-9]*)_name$', arg)
            if not matches or not matches.group(1) or not matches.group(2):
                continue
            treatment_number = matches.group(1)
            property_number = matches.group(2)
            if not treatment_numbers.get(treatment_number):
                treatment_numbers[treatment_number] = []
            treatment_numbers[treatment_number].append(property_number)

        # Then get the actual data together for them
        treatments = []
        for treatment_number, property_numbers in treatment_numbers.items():
            t = {
                'name': self.request.get('treat' + treatment_number + '_name'),
                'properties': []
            }
            if not t.get('name'):
                return None

            for property_number in property_numbers:
                name = self.request.get('treat' + treatment_number + "_prop" +
                                          property_number + "_name")
                value = self.request.get('treat' + treatment_number + "_prop" +
                                           property_number + "_value")
                if not name or not value:
                    return None
                t.get('properties').append({'name': name, 'value': value})

            treatments.append(t)

        return treatments

    def parse(self):
        """ Parse out the experiment creation details from the POSTed data """
        exp = self.parse_experiment_details()
        if not exp:
            return None

        exp['treatments'] = self.parse_treatments()
        if not exp['treatments']:
            return None

        return exp

    def post(self):
        """ Create a new Experiment (with Treatments and Properties) based on
        the POSTed form data.  The format is as follows:
            exp_name = main Experiment name
            treat##_name = the name of treatment ##
            treat##_prop##_name = the name of a property to be changed for
                                  treatment ##
            treat##_prop##_value = the value that property ## will be set to
                                  for treatment ##
        There can be any number of treatments/properties, but there can be no
        missing values, (ie: a property name without a value) or the creation
        will fail.
        """
        experiment = self.parse();

        if not experiment:
            self.redirect("error")
        else:
            exp = Experiment()
            exp.name = experiment.get('name')
            exp.owner = experiment.get('owner')
            exp.put()

            # Adding each treatment in turn
            for treatment in experiment.get('treatments'):
                t = Treatment(parent=exp.key())
                t.name = treatment.get('name')
                t.put()
                for property in treatment.get('properties'):
                    p = Property(parent=t.key())
                    p.name = property.get('name')
                    p.value = property.get('value')
                    p.put()

            self.redirect("view?exp_key=%s" % exp.key())

app = webapp2.WSGIApplication([('/create', CreateNewExperiment)], debug=True)
