function emptyFontFaceDeclarations()
{
    var stylesheet = document.styleSheets[0];
    var rules = stylesheet.cssRules;
    var rulesLengthSnapshot = stylesheet.cssRules.length;
    if (!rulesLengthSnapshot)
        return;
    for (var i = 0; i < rulesLengthSnapshot; i++) {
        stylesheet.deleteRule(0);
    }
}

function makeFontFaceDeclaration(stretch, style, weight)
{
    var fontFaceDeclaration = '@font-face { font-family: "CSSMatchingtest"; ' +
        "font-stretch: " + stretch + ";" +
        "font-style: " + style + ";" +
        "font-weight: " + weight + ";" +
        'src: url("' +
        "resources/fonts/CSSMatchingTest_" +
        stretch + "_" + style + "_" + weight + '.ttf");';
    return fontFaceDeclaration;
}

function notifyInspectorFontsReady() {
    var cssRules = document.styleSheets[0].cssRules;
    var fontsAvailable = [];
    for (var i = 0; i < cssRules.length; i++) {
        urlmatch = /url\(".*fonts\/CSSMatchingTest_(.*).ttf"\)/.exec(
            cssRules[i].cssText);
        fontsAvailable.push(urlmatch[1]);
    }
    evaluateInFrontend('window.onFontsLoaded(' +
                       JSON.stringify(fontsAvailable) + ");");
}

function updateAvailableFontFaceDeclarations(available)
{
    emptyFontFaceDeclarations();
    for (stretch of available.stretches)
        for (style of available.styles)
            for (weight of available.weights) {
                document.styleSheets[0].addRule(
                    makeFontFaceDeclaration(stretch, style, weight));
            }

    document.fonts.ready.then(window.notifyInspectorFontsReady);
}

function test()
{
    var documentNodeId;
    var documentNodeSelector;
    var allTestSelectors = [];
    var testSelectors = [];

    var stretches = [ 'condensed', 'expanded' ];
    var styles = [ 'normal', 'italic' ];
    var weights = [ '100', '900' ];

    var weightsHumanReadable = [ "Thin", "Black" ];

    var fontSetVariations = [];
    var currentFontSetVariation = {};

    makeFontSetVariations();

    function makeFontSetVariations() {
        // For each of the three properties we have three choices:
        // Restrict to the first value, to the second, or
        // allow both. So we're iterating over those options
        // for each dimension to generate the set of allowed
        // options for each property. The fonts in the test
        // page will later be generated from these possible
        // choices / restrictions.
        function choices(property) {
            return [ property, [ property[0] ], [ property[1] ] ];
        }

        for (stretchChoice of choices(stretches)) {
            for (styleChoice of choices(styles)) {
                for (weightChoice of choices(weights)) {
                    var available = {};
                    available.stretches = stretchChoice;
                    available.styles = styleChoice;
                    available.weights = weightChoice;
                    fontSetVariations.push(available);
                }
            }
        }
    }

    InspectorTest.requestDocumentNodeId(onDocumentNodeId);

    function onDocumentNodeId(nodeId)
    {
        documentNodeId = nodeId;

        InspectorTest.evaluateInInspectedPage(
            'JSON.stringify(' +
                'Array.prototype.map.call(' +
                'document.querySelectorAll(".test *"),' +
                'function(element) { return element.id } ));',
            startTestingPage);
    }

    function startTestingPage(result)
    {
        allTestSelectors = JSON.parse(result.result.result.value);
        nextTest();
    }

    function nextTest() {
        if (testSelectors.length) {
            testNextPageElement();
        } else {
            currentFontSetVariation = fontSetVariations.shift();
            if (currentFontSetVariation) {
                testSelectors = allTestSelectors.slice();
                updateFontDeclarationsInPageForVariation(
                    currentFontSetVariation);
            } else {
                InspectorTest.completeTest();
            }
        }
    }

    function updateFontDeclarationsInPageForVariation(fontSetVariation)
    {
        window.testPageFontsReady = false;
        InspectorTest.evaluateInInspectedPage(
            'updateAvailableFontFaceDeclarations(' +
                JSON.stringify(fontSetVariation) +
                ');');
        // nextTest() is called once we receive the onFontsLoaded
        // notification from the page.
    }

    // Explicitly make this available on window scope so that testing
    // can resume through a signal from the page once the fonts.ready
    // promise resolves.
    window.onFontsLoaded = function(loadedFonts) {
        InspectorTest.log("Available fonts updated: " +
                          JSON.stringify(loadedFonts) + "\n");
        // fonts.ready event fires too early, used fonts for rendering
        // have not been updated yet. Remove this wehn crbug.com/516680
        // is fixed.
        // https://drafts.csswg.org/css-font-loading/#font-face-set-ready
        setTimeout(nextTest, 100);
    }

    function testNextPageElement(result)
    {
        var nextSelector = testSelectors.shift()
        if (nextSelector) {
            documentNodeSelector = "#" + nextSelector;
            platformFontsForElementWithSelector(documentNodeSelector);
        }
    }

    function platformFontsForElementWithSelector(selector)
    {
        InspectorTest.requestNodeId(documentNodeId, selector, onNodeId);

        function onNodeId(nodeId)
        {
            InspectorTest.sendCommandOrDie("CSS.getPlatformFontsForNode",
                                           { nodeId: nodeId },
                                           onGotComputedFonts);
        }

        function onGotComputedFonts(response)
        {
            logResults(response);
            nextTest();
        }
    }

    function logResults(response)
    {
        InspectorTest.log(documentNodeSelector);
        logPassFailResult(
            documentNodeSelector,
            response.fonts[0].familyName);
    }

    function cssStyleMatchingExpectationForSelector(selector) {
        var selectorStretchStyleWeight = selector.substr(1).split("_");
        var selectorStretch = selectorStretchStyleWeight[0].toLowerCase();
        var selectorStyle = selectorStretchStyleWeight[1].toLowerCase();
        var selectorWeight = selectorStretchStyleWeight[2];

        var expectedProperties = {};
        if (currentFontSetVariation.stretches.indexOf(selectorStretch) > 0) {
            expectedProperties.stretch = selectorStretch;
        } else {
            // If the requested property value is not availabe in the
            // current font set, then it's restricted to only one value,
            // which is the nearest match, and at index 0.
            expectedProperties.stretch = currentFontSetVariation.stretches[0];
        }

        if (currentFontSetVariation.styles.indexOf(selectorStyle) > 0) {
            expectedProperties.style = selectorStyle;
        } else {
            expectedProperties.style = currentFontSetVariation.styles[0];
        }

        if (currentFontSetVariation.weights.indexOf(selectorWeight) > 0) {
            expectedProperties.weight = selectorWeight;
        } else {
            expectedProperties.weight = currentFontSetVariation.weights[0];
        }

        return expectedProperties;
    }

    function logPassFailResult(selector, usedFontName)
    {
        var actualStretchStyleWeight =
            /CSSMatchingTest (\w*) (\w*) (.*)/.exec(usedFontName);
        var actualStretch, actualStyle, actualWeight;
        if (actualStretchStyleWeight && actualStretchStyleWeight.length > 3) {
            actualStretch = actualStretchStyleWeight[1].toLowerCase();
            actualStyle = actualStretchStyleWeight[2].toLowerCase();
            // Font names have human readable weight description,
            // we need to convert.
            actualWeight = weights [
                weightsHumanReadable.indexOf(actualStretchStyleWeight[3]) ];
        } else {
            actualStretch = usedFontName;
            actualStyle = "";
            actualWeight = "";
        }

        var expectedProperties = cssStyleMatchingExpectationForSelector(
            selector);

        InspectorTest.log("Expected: " +
                          expectedProperties.stretch + " " +
                          expectedProperties.style + " " +
                          expectedProperties.weight);
        InspectorTest.log("Actual: " + actualStretch + " " +
                          actualStyle + " " +
                          actualWeight);

        if (actualStretch != expectedProperties.stretch ||
            actualStyle != expectedProperties.style ||
            actualWeight != expectedProperties.weight) {
            InspectorTest.log("FAIL\n");
        } else {
            InspectorTest.log("PASS\n");
        }
    }
};

window.addEventListener("DOMContentLoaded", function () {
    runTest();
}, false);
