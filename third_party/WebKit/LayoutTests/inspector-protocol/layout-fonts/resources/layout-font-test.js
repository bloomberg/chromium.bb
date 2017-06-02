function test()
{
    var documentNodeId;
    var documentNodeSelector;
    var testSelectors = [];
    var collectedFontUsage = {};

    InspectorTest.requestDocumentNodeId(onDocumentNodeId);

    function onDocumentNodeId(nodeId)
    {
        documentNodeId = nodeId;

        InspectorTest.evaluateInInspectedPage(
            'JSON.stringify(' +
                'Array.prototype.map.call(' +
                'document.querySelectorAll(".test *"),' +
                'function(element) { return element.id } ));',
            testPageElements);
    }


    function testPageElements(result)
    {
        testSelectors = JSON.parse(result.result.result.value);
        testNextPageElement();
    }

    function testNextPageElement()
    {
        var nextSelector = testSelectors.shift()
        if (nextSelector) {
            documentNodeSelector = "#" + nextSelector;
            platformFontsForElementWithSelector(documentNodeSelector);
        } else {
            reportAndComplete();
        }
    }

    function reportAndComplete()
    {
        postResultsToPage();
        InspectorTest.completeTest();
    }


    function postResultsToPage()
    {
        // This interleaves the used fonts data with the page elements so that
        // the content_shell text dump is easier to validate.
        InspectorTest.evaluateInInspectedPage("injectCollectedResultsInPage(" +
                                              JSON.stringify(collectedFontUsage) +
                                              ")");
        InspectorTest.evaluateInInspectedPage("postTestHookWithFontResults(" +
                                              JSON.stringify(collectedFontUsage) +
                                              ")");

    }

    function platformFontsForElementWithSelector(selector)
    {
        InspectorTest.requestNodeId(documentNodeId, selector, onNodeId);

        function onNodeId(nodeId)
        {
            InspectorTest.sendCommandOrDie("CSS.getPlatformFontsForNode", { nodeId: nodeId }, onGotComputedFonts);
        }

        function onGotComputedFonts(response)
        {
            collectResults(response);
            testNextPageElement();
        }
    }

    function collectResults(response)
    {
        var usedFonts = response.fonts;
        usedFonts.sort(function(a, b) {
            return b.glyphCount - a.glyphCount;
        });
        collectedFontUsage[documentNodeSelector] = usedFonts;
    }
};


function formatResult(selector, resultForSelector)
{
    var resultFormatted = selector + ":\n";
    for (var i = 0; i < resultForSelector.length; i++) {
        resultFormatted += '"' + resultForSelector[i].familyName + '"' +
            " : " +
            resultForSelector[i].glyphCount;
        if (i + 1 < resultForSelector.length)
            resultFormatted += ",\n";
        else
            resultFormatted += "\n\n";
    }
    return resultFormatted;
}

function injectCollectedResultsInPage(results)
{
    for (nodeSelector in results) {
        var testNode = document.querySelector(nodeSelector);
        var resultElement = document.createElement("div");
        var resultTextPre = document.createElement("pre");
        var resultText = formatResult(nodeSelector, results[nodeSelector]);
        var resultTextNode = document.createTextNode(resultText);
        resultTextPre.appendChild(resultTextNode);
        resultElement.appendChild(resultTextPre);
        testNode.parentNode.insertBefore(resultElement, testNode.nextSibling);
    }
}

window.addEventListener("DOMContentLoaded", function () {
    runTest();
}, false);
