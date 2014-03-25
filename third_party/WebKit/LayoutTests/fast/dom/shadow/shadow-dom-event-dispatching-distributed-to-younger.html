<!DOCTYPE html>
<html>
<head>
    <script src="../../../resources/js-test.js"></script>
    <script src="resources/shadow-dom.js"></script>
    <script src="resources/event-dispatching.js"></script>
    <style>
      sandbox /deep/ div {
        padding-top: 20px;
        width: 40px;
      }
    </style>
</head>
<body>
    <div id="sandbox"></div>
    <div id="console"></div>
    <script>
        var sandbox = document.getElementById('sandbox');
        sandbox.appendChild(
            createDOM('div', {'id': 'top'},
                      createDOM('div', {'id': 'A'},
                                createShadowRoot({'id': 'older-shadow-root'},
                                                 createDOM('div', {'id': 'C'})),
                                createShadowRoot({'id': 'younger-shadow-root'},
                                                 createDOM('shadow', {'id': 'D'}),
                                                 createDOM('content', {'id': 'E'}),
                                                 createDOM('div', {'id': 'F'})),
                                createDOM('div', {'id': 'B'}))));

        addEventListeners(['top', 'A', 'B', 'A/', 'A/C', 'A//', 'A//D', 'A//E', 'A//F']);
        showSandboxTree();

        moveMouse('B', 'A//F');
        moveMouse('A/C', 'B');
        moveMouse('A/C', 'A//F');
    </script>
</body>
</html>
