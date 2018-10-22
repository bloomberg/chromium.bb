// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LineChartTest = LineChartTest || {};

LineChartTest.Menu = function() {
  test('Menu integration test', function() {
    const menu = new LineChart.Menu(function() {});
    const data1 = new LineChart.DataSeries('test1', '#aabbcc');
    const data2 = new LineChart.DataSeries('test2', '#aabbcc');
    const data3 = new LineChart.DataSeries('test3', '#aabbcc');

    menu.addDataSeries(data1);
    menu.addDataSeries(data2);
    menu.addDataSeries(data3);
    /* Add second time won't do nothing. */
    menu.addDataSeries(data3);
    assertEquals(menu.dataSeries_.length, 3);
    assertEquals(menu.buttons_.length, 3);

    const buttons = menu.buttons_;
    MockInteractions.tap(buttons[0]);
    assertFalse(data1.isVisible());
    MockInteractions.tap(buttons[2]);
    assertFalse(data3.isVisible());
    MockInteractions.tap(buttons[0]);
    assertTrue(data1.isVisible());

    assertFalse(menu.buttonOuterDiv_.hasAttribute('hidden'));
    MockInteractions.tap(menu.handleDiv_);
    assertTrue(menu.buttonOuterDiv_.hasAttribute('hidden'));
    MockInteractions.tap(menu.handleDiv_);
    assertFalse(menu.buttonOuterDiv_.hasAttribute('hidden'));

    menu.removeDataSeries(data1);
    menu.removeDataSeries(data3);
    menu.removeDataSeries(data2);
    /* Remove second time won't do nothing. */
    menu.removeDataSeries(data2);
    assertEquals(menu.dataSeries_.length, 0);
    assertEquals(menu.buttons_.length, 0);
  });

  mocha.run();
};
