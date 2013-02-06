# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import math

def average(l):
    return float(sum(l)) / len(l)

def variance(l):
    avg = average(l)
    return sum([(v - avg)**2 for v in l]) / len(l)

def row_class(iteration):
    if iteration % 2 == 0:
        return "even_row"
    return "odd_row"

def binomial_pmf(k, n, p):
    # Compute Pr(X == k) if X ~ binomial(n, p)
    choices = math.factorial(n)
    choices /= math.factorial(k)
    choices /= math.factorial(n - k)
    return choices * p**k * (1.0 - p)**(n - k)

def sign_test(x, y):
    # First, see how often x was ranked better than y
    w = sum([1 for x_val, y_val in zip(x, y) if x_val < y_val])

    # Next, count the number on non-ties, tied votes should not count
    n = sum([1 for x_val, y_val in zip(x, y) if x_val != y_val])

    # Compute the probability of doing even better for H_0: Pr(X < Y) = 0.5
    # p_value: Pr(W >= w) ie: the probability of seeing this or more extreme
    p_value = sum([binomial_pmf(i, n, 0.5) for i in range(w, n + 1)])

    return p_value

def encode_property(prop):
    """ Return an encoded version of a property by merging it's encoded
    name and value with the hex value for the : character (3a)

    NAME:VALUE
    """
    return "%s3a%s" % (prop.name.encode("hex"), prop.value.encode("hex"))

def encode_treatment(treatment, properties):
    """ Return an encoded version of a treatment by merging it's encoded
    properties with the hex value for the , character (0x2c)

    PROPERTY 1,PROPERTY 2, PROPERTY 3
    """
    return "2c".join([encode_property(p) for p in properties
                      if p.parent_key() == treatment.key()])

def encode_experiment(experiment, treatments, properties):
    """ Return an encoded version of an experiment by merging it's encoded
    treatments with the hex value for the + character (0x2b)

    TREATMENT 1+TREATMENT 2+TREATMENT 3
    """
    return "2b".join([encode_treatment(t, properties) for t in treatments])
