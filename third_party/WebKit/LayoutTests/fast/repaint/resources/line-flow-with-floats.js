var frameDoc;
var loadCount = 0;

function loaded()
{
    loadCount++;
    if (loadCount == 2)
        runRepaintTest();
}

function repaintTest()
{
    test(document.getElementById("iframe").contentDocument);
}
