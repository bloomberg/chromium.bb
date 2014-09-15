#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import sys
from string import Template


_HTML_TEMPLATE = """<!DOCTYPE html>
<script src="https://www.google.com/jsapi"></script>
<script>
var all_data = $ALL_DATA;
google.load('visualization', '1', {packages:['corechart', 'table']});
google.setOnLoadCallback(drawVisualization);
function drawVisualization() {
  // Apply policy 'l2' by default.
  var default_policy = '$DEF_POLICY';
  document.getElementById(default_policy).style.fontWeight = 'bold';
  turnOn(default_policy);
}

function turnOn(policy) {
  var data = google.visualization.arrayToDataTable(all_data[policy]);
  var charOptions = {
    title: 'DMP Graph (Policy: ' + policy + ')',
    hAxis: {title: 'Timestamp',  titleTextStyle: {color: 'red'}},
    isStacked : true
  };
  var chart = new google.visualization.AreaChart(
      document.getElementById('chart_div'));
  chart.draw(data, charOptions);
  var table = new google.visualization.Table(
      document.getElementById('table_div'));
  table.draw(data);
}

window.onload = function() {
  var ul = document.getElementById('policies');
  for (var i = 0; i < ul.children.length; ++i) {
    var li = ul.children[i];
    li.onclick = function() {
      for (var j = 0; j < ul.children.length; ++j) {
        var my_li = ul.children[j];
        my_li.style.fontWeight = 'normal';
      }
      this.style.fontWeight = 'bold';
      turnOn(this.id);
    }
  }
};
</script>
<style>
#policies li {
  display: inline-block;
  padding: 5px 10px;
}
</style>
Click to change an applied policy.
<ul id="policies">$POLICIES</ul>
<div id="chart_div" style="width: 1024px; height: 640px;"></div>
<div id="table_div" style="width: 1024px; height: 640px;"></div>
"""

def _GenerateGraph(json_data):
  policies = list(json_data['policies'])

  default_policy = "l2"
  if default_policy not in policies:
    default_policy = policies[0]

  policies = "".join(map(lambda x: '<li id="'+x+'">'+x+'</li>', policies))

  all_data = {}
  for policy in json_data['policies']:
    legends = list(json_data['policies'][policy]['legends'])
    legends = ['second'] + legends[legends.index('FROM_HERE_FOR_TOTAL') + 1:
                                     legends.index('UNTIL_HERE_FOR_TOTAL')]
    data = []
    for snapshot in json_data['policies'][policy]['snapshots']:
      data.append([0] * len(legends))
      for k, v in snapshot.iteritems():
        if k in legends:
          data[-1][legends.index(k)] = v
    all_data[policy] = [legends] + data

  print Template(_HTML_TEMPLATE).safe_substitute(
      {'POLICIES': policies,
       'DEF_POLICY': default_policy,
       'ALL_DATA': json.dumps(all_data)})


def main(argv):
  _GenerateGraph(json.load(file(argv[1], 'r')))


if __name__ == '__main__':
  sys.exit(main(sys.argv))
