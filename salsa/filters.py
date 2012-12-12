# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def average(l):
    return float(sum(l)) / len(l)

def variance(l):
    avg = average(l)
    return sum([(v - avg)**2 for v in l]) / len(l)

def row_class(iteration):
    if iteration % 2 == 0:
        return "even_row"
    return "odd_row"
