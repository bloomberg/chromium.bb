var frameDoc;
var loadCount = 0;

function loaded()
{
    loadCount++;
    if (loadCount == 2) {
        document.body.offsetTop;
        runRepaintTest();
    }
}

function repaintTest()
{
    test(document.getElementById("iframe").contentDocument);
}
