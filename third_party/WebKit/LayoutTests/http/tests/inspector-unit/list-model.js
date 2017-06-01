TestRunner.addResult('Test ListModel API.');

var list = new UI.ListModel();
list.addEventListener(UI.ListModel.Events.ItemsReplaced, event => {
  var data = event.data;
  var inserted = [];
  for (var index = data.index; index < data.index + data.inserted; index++)
    inserted.push(list.itemAtIndex(index));
  TestRunner.addResult(`Replaced [${data.removed.join(', ')}] at index ${data.index} with [${inserted.join(', ')}]`);
  var all = [];
  for (var index = 0; index < list.length(); index++)
    all.push(list.itemAtIndex(index));
  TestRunner.addResult(`Resulting list: [${all.join(', ')}]`);
  TestRunner.addResult('');
});

TestRunner.addResult('Adding 0, 1, 2');
list.replaceAllItems([0, 1, 2]);

TestRunner.addResult('Replacing 0 with 5, 6, 7');
list.replaceItemsInRange(0, 1, [5, 6, 7]);

TestRunner.addResult('Pushing 10');
list.pushItem(10);

TestRunner.addResult('Popping 10');
list.popItem();

TestRunner.addResult('Removing 2');
list.removeItemAtIndex(4);

TestRunner.addResult('Inserting 8');
list.insertItemAtIndex(1, 8);

TestRunner.addResult('Replacing with 0...20');
list.replaceAllItems([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19]);

TestRunner.addResult('Replacing 7 with 27');
list.replaceItemsInRange(7, 8, [27]);

TestRunner.addResult('Replacing 18, 19 with 28, 29');
list.replaceItemsInRange(18, 20, [28, 29]);

TestRunner.addResult('Replacing 1, 2, 3 with [31-43]');
list.replaceItemsInRange(1, 4, [31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43]);

TestRunner.addResult('Replacing all but 29 with []');
list.replaceItemsInRange(0, 29, []);

TestRunner.completeTest();
