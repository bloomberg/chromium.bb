var initialize_SassTest = function() {

InspectorTest.preloadModule("sass");

var cssParserService = null;

InspectorTest.cssParserService = function()
{
    if (!cssParserService)
        cssParserService = new WebInspector.CSSParserService();
    return cssParserService;
}

InspectorTest.parseCSS = function(url, text)
{
    return WebInspector.SASSSupport.parseCSS(InspectorTest.cssParserService(), url, text);
}

InspectorTest.parseSCSS = function(url, text)
{
    return self.runtime.instancePromise(WebInspector.TokenizerFactory)
        .then(onTokenizer);

    function onTokenizer(tokenizer)
    {
        return WebInspector.SASSSupport.parseSCSS(tokenizer, url, text);
    }
}

InspectorTest.loadASTMapping = function(header, callback)
{
    var completeSourceMapURL = WebInspector.ParsedURL.completeURL(header.sourceURL, header.sourceMapURL);
    WebInspector.SourceMap.load(completeSourceMapURL, header.sourceURL).then(onSourceMapLoaded);

    function onSourceMapLoaded(sourceMap)
    {
        var astService = new WebInspector.ASTService();
        WebInspector.ASTSourceMap.fromSourceMap(astService, header.cssModel(), sourceMap)
            .then(mapping => callback(mapping))
            .then(() => astService.dispose());
    }
}

InspectorTest.dumpAST = function(ast)
{
    var lines = [String.sprintf("=== AST === %s", ast.document.url)];
    for (var i = 0; i < ast.rules.length; ++i) {
        var rule = ast.rules[i];
        lines.push(String.sprintf("rule %d: \"%s\"", i, rule.selector));
        var ruleLines = dumpRule(rule);
        lines = lines.concat(indent(ruleLines));
    }
    lines.push("======");
    InspectorTest.addResult(lines.join("\n"));
    return ast;

    function dumpRule(rule)
    {
        var lines = [];
        for (var i = 0; i < rule.properties.length; ++i) {
            var property = rule.properties[i];
            lines.push("property " + i);
            var propertyLines = dumpProperty(property);
            lines = lines.concat(indent(propertyLines));
        }
        return lines;
    }

    function dumpProperty(property)
    {
        var lines = [];
        lines.push(String.sprintf("name: \"%s\"", property.name.text));
        lines = lines.concat(indent(dumpTextNode(property.name)));
        lines.push(String.sprintf("value: \"%s\"", property.value.text));
        lines = lines.concat(indent(dumpTextNode(property.value)));
        lines.push(String.sprintf("range: %s", property.range.toString()));
        lines.push(String.sprintf("disabled: %s", property.disabled));
        return lines;
    }

    function dumpTextNode(textNode)
    {
        return [String.sprintf("range: %s", textNode.range.toString())];
    }

}

function indent(lines)
{
    return lines.map(line => "    " + line);
}

InspectorTest.dumpASTDiff = function(diff)
{
    InspectorTest.addResult("=== Diff ===");
    var changesPerRule = new Map();
    for (var change of diff.changes) {
        var oldRule = change.oldRule;
        var ruleChanges = changesPerRule.get(oldRule);
        if (!ruleChanges) {
            ruleChanges = [];
            changesPerRule.set(oldRule, ruleChanges);
        }
        ruleChanges.push(change);
    }
    var T = WebInspector.SASSSupport.PropertyChangeType;
    for (var rule of changesPerRule.keys()) {
        var changes = changesPerRule.get(rule);
        var names = [];
        var values = [];
        for (var property of rule.properties) {
            names.push(str(property.name, "    "));
            values.push(str(property.value));
        }
        for (var i = changes.length - 1; i >= 0; --i) {
            var change = changes[i];
            var newProperty = change.newRule.properties[change.newPropertyIndex];
            var oldProperty = change.oldRule.properties[change.oldPropertyIndex];
            switch (change.type) {
            case T.PropertyAdded:
                names.splice(change.oldPropertyIndex, 0, str(newProperty.name, "[+] "));
                values.splice(change.oldPropertyIndex, 0, str(newProperty.value));
                break;
            case T.PropertyRemoved:
                names[change.oldPropertyIndex] = str(oldProperty.name, "[-] ");
                break;
            case T.PropertyToggled:
                names[change.oldPropertyIndex] = str(oldProperty.name, "[T] ");
                break;
            case T.NameChanged:
                names[change.oldPropertyIndex] = str(oldProperty.name, "[M] ");
                break;
            case T.ValueChanged:
                values[change.oldPropertyIndex] = str(oldProperty.value, "[M] ");
                break;
            }
        }
        InspectorTest.addResult("Changes for rule: " + rule.selector);
        names = indent(names);
        for (var i = 0; i < names.length; ++i)
            InspectorTest.addResult(names[i] + ": " + values[i]);
    }

    function str(node, prefix)
    {
        prefix = prefix || "";
        return prefix + node.text.trim();
    }
}

InspectorTest.validateASTRanges = function(ast)
{
    var invalidNodes = [];
    for (var rule of ast.rules) {
        for (var property of rule.properties) {
            validate(property.name);
            validate(property.value);
        }
    }
    if (invalidNodes.length) {
        InspectorTest.addResult("Bad ranges: " + invalidNodes.length);
        for (var node of invalidNodes)
            InspectorTest.addResult(String.sprintf("  - range: %s text: %s", node.range.toString(), node.text));
    } else {
        InspectorTest.addResult("Ranges OK.");
    }
    return ast;

    function validate(textNode)
    {
        if (textNode.range.extract(textNode.document.text) !== textNode.text)
            invalidNodes.push(textNode);
    }
}

InspectorTest.validateMapping = function(mapping)
{
    InspectorTest.addResult("Mapped CSS: " + mapping._cssToSass.size);
    InspectorTest.addResult("Mapped SCSS: " + mapping._sassToCss.size);
    var cssNodes = mapping._cssToSass.keysArray();
    var staleCSS = 0;
    var staleSASS = 0;
    for (var i = 0; i < cssNodes.length; ++i) {
        var cssNode = cssNodes[i];
        staleCSS += cssNode.document !== mapping.cssAST().document ? 1 : 0;
        var sassNode = mapping.toSASSNode(cssNode);
        var sassAST = mapping.sassModels().get(sassNode.document.url);
        staleSASS += sassNode.document !== sassAST.document ? 1 : 0;
    }
    if (staleCSS || staleSASS) {
        InspectorTest.addResult("ERROR: found stale entries");
        InspectorTest.addResult("   -stale CSS: " + staleCSS);
        InspectorTest.addResult("   -stale SASS: " + staleSASS);
    } else {
        InspectorTest.addResult("No stale entries found.");
    }
}

InspectorTest.updateCSSText = function(url, newText)
{
    var styleSheetIds = InspectorTest.cssModel.styleSheetIdsForURL(url)
    var promises = styleSheetIds.map(id => InspectorTest.cssModel.setStyleSheetText(id, newText, true));
    return Promise.all(promises);
}

InspectorTest.updateSASSText = function(url, newText)
{
    var uiSourceCode = WebInspector.workspace.uiSourceCodeForURL(url);
    uiSourceCode.addRevision(newText);
}

InspectorTest.runCSSEditTests = function(header, tests)
{
    var astSourceMap;
    var astService = new WebInspector.ASTService();
    InspectorTest.loadASTMapping(header, onMapping);

    function onMapping(map)
    {
        astSourceMap = map;
        InspectorTest.addResult("INITIAL MODELS");
        logASTText(map.cssAST(), true);
        for (var ast of map.sassModels().values())
            logASTText(ast, true);
        runTests();
    }

    function runTests()
    {
        if (!tests.length) {
            InspectorTest.completeTest();
            astService.dispose();
            return;
        }
        var test = tests.shift();
        logTestName(test.name);
        var text = astSourceMap.cssAST().document.text;
        var edits = test(text);
        logSourceEdits(text, edits);
        var ranges = edits.map(edit => edit.oldRange);
        var texts = edits.map(edit => edit.newText);
        WebInspector.SASSProcessor.processCSSEdits(astService, astSourceMap, ranges, texts)
            .then(onEditsDone);
    }

    function onEditsDone(result)
    {
        if (!result.map) {
            InspectorTest.addResult("SASSProcessor failed to process edits.");
            runTests();
            return;
        }
        logASTText(result.map.cssAST());
        for (var sassURL of result.newSASSSources.keys()) {
            var ast = result.map.sassModels().get(sassURL);
            logASTText(ast);
        }
        runTests();
    }

    function logASTText(ast, avoidIndent, customTitle)
    {
        customTitle = customTitle || ast.document.url.split("/").pop();
        InspectorTest.addResult("===== " + customTitle + " =====");
        var text = ast.document.text.replace(/ /g, ".");
        var lines = text.split("\n");
        if (!avoidIndent)
            lines = indent(lines);
        InspectorTest.addResult(lines.join("\n"));
    }

    function logTestName(testName)
    {
        var titleText = " TEST: " + testName + " ";
        var totalLength = 80;
        var prefixLength = ((totalLength - titleText.length) / 2)|0;
        var suffixLength = totalLength - titleText.length - prefixLength;
        var prefix = new Array(prefixLength).join("-");
        var suffix = new Array(suffixLength).join("-");
        InspectorTest.addResult("\n" + prefix + titleText + suffix + "\n");
    }

    function logSourceEdits(text, edits)
    {
        var lines = [];
        for (var i = 0; i < edits.length; ++i) {
            var edit = edits[i];
            var range = edit.oldRange;
            var line = String.sprintf("{%d, %d, %d, %d}", range.startLine, range.startColumn, range.endLine, range.endColumn);
            line += String.sprintf(" '%s' => '%s'", range.extract(text), edit.newText);
            lines.push(line);
        }
        lines = indent(lines);
        lines.unshift("Edits:");
        InspectorTest.addResult(lines.join("\n"));
    }
}

InspectorTest.createEdit = function(source, pattern, newText, matchNumber)
{
    matchNumber = matchNumber || 0;
    var re = new RegExp(pattern.escapeForRegExp(), "g");
    var match;
    while ((match = re.exec(source)) !== null && matchNumber) {
        --matchNumber;
    }
    if (!match)
        return null;
    var sourceRange = new WebInspector.SourceRange(match.index, match[0].length);
    var textRange = sourceRange.toTextRange(source);
    return new WebInspector.SourceEdit("", textRange, newText);
}

}

