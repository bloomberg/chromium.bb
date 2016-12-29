TestRunner.addResult('Test ListControl rendering and selection for fixed height case.');

class Delegate {
  constructor() {
    this.height = 10;
  }

  createElementForItem(item) {
    TestRunner.addResult('Creating element for ' + item);
    var element = document.createElement('div');
    element.style.height = this.height + 'px';
    element.textContent = item;
    return element;
  }

  heightForItem(item) {
    return this.height;
  }

  isItemSelectable(item) {
    return (item % 5 === 0) || (item % 5 === 2);
  }

  selectedItemChanged(from, to, fromElement, toElement) {
    TestRunner.addResult('Selection changed from ' + from + ' to ' + to);
    if (fromElement)
      fromElement.classList.remove('selected');
    if (toElement)
      toElement.classList.add('selected');
  }
}

var delegate = new Delegate();
var list = new UI.ListControl(delegate);
list.setHeightMode(UI.ListHeightMode.Fixed);
list.element.style.height = '73px';
UI.inspectorView.element.appendChild(list.element);

function dumpList()
{
  var height = list.element.offsetHeight;
  TestRunner.addResult(`----list[length=${list.length()}][height=${height}]----`);
  for (var child of list.element.children) {
    var offsetTop = child.getBoundingClientRect().top - list.element.getBoundingClientRect().top;
    var offsetBottom = child.getBoundingClientRect().bottom - list.element.getBoundingClientRect().top;
    var visible = (offsetBottom <= 0 || offsetTop >= height) ? ' ' :
        (offsetTop >= 0 && offsetBottom <= height ? '*' : '+');
    var selected = child.classList.contains('selected') ? ' (selected)' : '';
    var text = child === list._topElement ? 'top' : (child === list._bottomElement ? 'bottom' : child.textContent);
    TestRunner.addResult(`${visible}[${offsetTop}] ${text}${selected}`);
  }
  TestRunner.addResult('');
}

TestRunner.addResult('Adding 0, 1, 2');
list.replaceAllItems([0, 1, 2]);
dumpList();

TestRunner.addResult('Scrolling to 0');
list.scrollItemAtIndexIntoView(0);
dumpList();

TestRunner.addResult('Scrolling to 2');
list.scrollItemAtIndexIntoView(2);
dumpList();

TestRunner.addResult('ArrowDown');
list.onKeyDown(TestRunner.createKeyEvent('ArrowDown'));
dumpList();

TestRunner.addResult('Selecting 2');
list.selectItemAtIndex(2);
dumpList();

TestRunner.addResult('PageUp');
list.onKeyDown(TestRunner.createKeyEvent('PageUp'));
dumpList();

TestRunner.addResult('PageDown');
list.onKeyDown(TestRunner.createKeyEvent('PageDown'));
dumpList();

TestRunner.addResult('ArrowDown');
list.onKeyDown(TestRunner.createKeyEvent('ArrowDown'));
dumpList();

TestRunner.addResult('Replacing 0 with 5, 6, 7');
list.replaceItemsInRange(0, 1, [5, 6, 7]);
dumpList();

TestRunner.addResult('ArrowUp');
list.onKeyDown(TestRunner.createKeyEvent('ArrowUp'));
dumpList();

TestRunner.addResult('Pushing 10');
list.pushItem(10);
dumpList();

TestRunner.addResult('Selecting 10');
list.selectItemAtIndex(5);
dumpList();

TestRunner.addResult('Popping 10');
list.popItem();
dumpList();

TestRunner.addResult('Removing 2');
list.removeItemAtIndex(4);
dumpList();

TestRunner.addResult('Inserting 8');
list.insertItemAtIndex(1, 8);
dumpList();

TestRunner.addResult('Replacing with 0...20');
list.replaceAllItems([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19]);
dumpList();

TestRunner.addResult('Resizing');
list.element.style.height = '84px';
list.viewportResized();
dumpList();

TestRunner.addResult('Scrolling to 19');
list.scrollItemAtIndexIntoView(19);
dumpList();

TestRunner.addResult('Scrolling to 5');
list.scrollItemAtIndexIntoView(5);
dumpList();

TestRunner.addResult('Scrolling to 12');
list.scrollItemAtIndexIntoView(12);
dumpList();

TestRunner.addResult('Scrolling to 13');
list.scrollItemAtIndexIntoView(13);
dumpList();

TestRunner.addResult('Changing the item height, switching to measure');
delegate.height = 15;
list.setHeightMode(UI.ListHeightMode.Measured);
dumpList();

TestRunner.addResult('Selecting 7');
list.selectItemAtIndex(7);
dumpList();

TestRunner.addResult('Replacing 7 with 27');
list.replaceItemsInRange(7, 8, [27]);
dumpList();

TestRunner.addResult('Replacing 18, 19 with 28, 29');
list.replaceItemsInRange(18, 20, [28, 29]);
dumpList();

TestRunner.addResult('PageDown');
list.onKeyDown(TestRunner.createKeyEvent('PageDown'));
dumpList();

TestRunner.addResult('Replacing 1, 2, 3 with [31-43]');
list.replaceItemsInRange(1, 4, [31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43]);
dumpList();

TestRunner.addResult('ArrowUp');
list.onKeyDown(TestRunner.createKeyEvent('ArrowUp'));
dumpList();

TestRunner.addResult('Selecting -1');
list.selectItemAtIndex(-1);
dumpList();

TestRunner.addResult('ArrowUp');
list.onKeyDown(TestRunner.createKeyEvent('ArrowUp'));
dumpList();

TestRunner.addResult('Selecting -1');
list.selectItemAtIndex(-1);
dumpList();

TestRunner.addResult('ArrowDown');
list.onKeyDown(TestRunner.createKeyEvent('ArrowDown'));
dumpList();

TestRunner.addResult('Selecting -1');
list.selectItemAtIndex(-1);
dumpList();

TestRunner.addResult('PageUp');
list.onKeyDown(TestRunner.createKeyEvent('PageUp'));
dumpList();

TestRunner.addResult('Replacing all but 29 with []');
list.replaceItemsInRange(0, 29, []);
dumpList();

TestRunner.addResult('ArrowDown');
list.onKeyDown(TestRunner.createKeyEvent('ArrowDown'));
dumpList();

TestRunner.completeTest();
