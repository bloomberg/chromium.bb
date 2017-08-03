<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>

<style>/* unterminated comment </style>
<style>/*</style>

<style>/* terminated comment */</style>
<style>/* terminated comment *//*</style>

<script>

function test()
{
    InspectorTest.consoleModel.messages().forEach(function(message)
    {
        InspectorTest.addResult(message.message + " (line " + message.line + ")");
    });

    InspectorTest.completeTest();
}

</script>
</head>

<body onload="runTest()">
<p id="p">Tests that unterminated comment in CSS generates a warning.</p>
</body>
</html>
