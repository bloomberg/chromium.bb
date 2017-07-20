<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>

function onload()
{
    var a = [];
    for (var i = 0; i < 42; ++i)
        a[i] = i;
    a[100] = 100;
    console.dir(a);

    var b = [];
    for (var i = 0; i < 10; ++i)
      b[i] = undefined;
    console.dir(b);

    var c = [];
    for (var i = 0; i < 10; ++i)
        c[i] = i;
    c[100] = 100;
    console.dir(c);

    var d = [];
    for (var i = 0; i < 405; ++i)
      d[i] = i;
    console.dir(d);

    var e = [];
    for (var i = 0; i < 10; ++i)
        e[i] = i;
    e[123] = 123;
    e[-123] = -123;
    e[3.14] = 3.14;
    e[4294967295] = 4294967295;
    e[4294967296] = 4294967296;
    e[Infinity] = Infinity;
    e[-Infinity] = -Infinity;
    e[NaN] = NaN;
    console.log("%O", e);

    var f = [];
    f[4294967294] = 4294967294;
    for (var i = 20; i >= 0; i -= 2)
        f[i] = i;
    for (var i = 2, n = 33; n--; i *= 2)
        f[i] = i;
    for (var i = 1; i < 20; i += 2)
        f[i] = i;
    console.log("%O", f)

    var g = new Uint8Array(new ArrayBuffer(Math.pow(20, 6) + Math.pow(20, 4) + 3));
    console.dir(g);

    runTest();
}

function test()
{
    ObjectUI.ArrayGroupingTreeElement._bucketThreshold = 20;

    var messages = Console.ConsoleView.instance()._visibleViewMessages;
    var sections = [];
    for (var i = 0; i < messages.length; ++i) {
        var consoleMessage = messages[i].consoleMessage();
        var element = messages[i].toMessageElement();
        var node = element.traverseNextNode(element);
        while (node) {
            if (node._section) {
                sections.push(node._section);
                node._section.expand();
            }
            node = node.traverseNextNode(element);
        }
    }

    InspectorTest.addSniffer(ObjectUI.ArrayGroupingTreeElement.prototype, "onpopulate", populateCalled, true);
    var populated = false;
    function populateCalled()
    {
        populated = true;
    }

    InspectorTest.deprecatedRunAfterPendingDispatches(expandRecursively);

    function expandRecursively()
    {
        for (var i = 0; i < sections.length; ++i) {
            var children = sections[i].rootElement().children();
            for (var j = 0; j < children.length; ++j) {
                for (var treeElement = children[j]; treeElement; treeElement = treeElement.traverseNextTreeElement(true, null, true)) {
                    if (treeElement.listItemElement.textContent.indexOf("__proto__") === -1)
                        treeElement.expand();
                }
            }
        }
        if (populated)
            InspectorTest.deprecatedRunAfterPendingDispatches(completeTest);
        else
            InspectorTest.deprecatedRunAfterPendingDispatches(expandRecursively);
    }

    function completeTest()
    {
        InspectorTest.dumpConsoleMessages(false, false, InspectorTest.textContentWithLineBreaks);
        InspectorTest.completeTest();
    }
}

</script>
</head>

<body onload="onload()">
<p>
Tests that console logging dumps large arrays properly.
</p>

</body>
</html>
