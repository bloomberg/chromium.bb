

function waitAndTest(tests)
{
    window.jsTestIsAsync = true;

    function runNext()
    {
        var options = tests.shift();
        if (!options)
            return finishJSTest();
        return runSingleTest(options);
    }

    function runSingleTest(options)
    {
        var ntries = 10;
        function checkWhenReady()
        {
            if (--ntries < 0) {
                testFailed("Timed out");
                return finishJSTest();
            }

            if (!options.ready())
                return setTimeout(checkWhenReady, 0);

            options.test();
            return runNext();
        }

        debug(options.description);
        if (options.setup)
            options.setup();
        checkWhenReady();
    }

    window.setTimeout(runNext, 0);
}

function createPlaceholder()
{
    var link = document.createElement("link");
    link.setAttribute("href", "resources/placeholder.html");
    link.setAttribute("rel", "import");
    document.head.appendChild(link);
    return link;
}
