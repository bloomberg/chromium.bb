TestRunner.addResult("This tests if the StaticViewportControl works properly.");

var items = [];
var heights = [];
for (var i = 0; i < 100; i++){
    items[i] = document.createElement("div");
    items[i].style.height = (heights[i] = (i % 4) ? 50 : 28) + "px";
    items[i].textContent = i;
}
var viewport = new UI.StaticViewportControl({
    fastItemHeight: i => heights[i],
    itemCount: _ => items.length,
    itemElement: i => items[i]
});
viewport.element.style.height = "300px";
UI.inspectorView.element.appendChild(viewport.element);

viewport.refresh();
dumpViewport();

viewport.forceScrollItemToBeFirst(26);
dumpViewport();

viewport.scrollItemIntoView(33);
dumpViewport();

viewport.scrollItemIntoView(30);
dumpViewport();

viewport.forceScrollItemToBeFirst(30);
dumpViewport();

viewport.forceScrollItemToBeLast(88);
dumpViewport();

for (var i = 0; i < 100; i++)
    items[i].style.height = (heights[i] = (i % 2) ? 55 : 63) + "px";
viewport.refresh();
viewport.forceScrollItemToBeLast(88);
dumpViewport();

TestRunner.completeTest();

function dumpViewport()
{
    TestRunner.addResult("First:" + viewport.firstVisibleIndex());
    TestRunner.addResult("Last:" + viewport.lastVisibleIndex());
    TestRunner.addResult("Active Items:" + viewport._innerElement.children.length);
    TestRunner.addResult("");
}