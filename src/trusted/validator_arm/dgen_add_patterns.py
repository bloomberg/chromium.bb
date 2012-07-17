#!/usr/bin/python
#
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

"""
Checks (rule) patterns associated with rows in tables, and adds an
additional column to each row (in each table) which captures
constraints in rule pattern.
"""

def add_rule_pattern_constraints(decoder):
    """Adds an additional column to each table, defining additional
       constraints assumed by rule patterns in rows.
    """
    for table in decoder.tables():
       _add_rule_pattern_constraints_to_table(table)
    return decoder

def _add_rule_pattern_constraints_to_table(table):
    """Adds an additional column to the given table, defining
       additional constraints assumed by rule patterns in rows.
    """
    constraint_col = len(table.columns())
    table.add_column('$pattern', 31, 0)
    for row in table.rows():
       _add_rule_pattern_constraints_to_row(table, row, constraint_col)

def _add_rule_pattern_constraints_to_row(table, row, constraint_col):
    """Adds an additional (constraint) colum to the given row,
       defining additional constraints assumed by the rule
       pattern in the row.
    """
    action = row.action
    rule_pattern='-'
    if action and action.__class__.__name__ == 'DecoderAction':
        pattern = action.pattern
        if pattern:
           rule_pattern = pattern
    row.add_pattern(table.define_pattern(rule_pattern, constraint_col))
