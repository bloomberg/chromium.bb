var initialize_SassTest = function() {

InspectorTest.preloadModule("sass");

var sassSourceMapFactory = null;
InspectorTest.sassSourceMapFactory = function()
{
    if (!sassSourceMapFactory)
        sassSourceMapFactory = new Sass.SASSSourceMapFactory();
    return sassSourceMapFactory;
}

InspectorTest.parseSCSS = function(url, text)
{
    return Sass.SASSSupport.parseSCSS(url, text);
}
InspectorTest.parseCSS = InspectorTest.parseSCSS;

InspectorTest.loadASTMapping = function(header, callback)
{
    var completeSourceMapURL = Common.ParsedURL.completeURL(header.sourceURL, header.sourceMapURL);
    SDK.TextSourceMap.load(completeSourceMapURL, header.sourceURL).then(onSourceMapLoaded);

    function onSourceMapLoaded(sourceMap)
    {
        InspectorTest.sassSourceMapFactory().editableSourceMap(header.cssModel().target(), sourceMap)
            .then(map => callback(map));
    }
}

InspectorTest.dumpAST = function(ast)
{
    var lines = [String.sprintf("=== AST === %s", ast.document.url)];
    for (var i = 0; i < ast.rules.length; ++i) {
        var rule = ast.rules[i];
        lines.push(String.sprintf("rule %d", i));
        var ruleLines = dumpRule(rule);
        lines = lines.concat(indent(ruleLines));
    }
    lines.push("======");
    InspectorTest.addResult(lines.join("\n"));
    return ast;

    function dumpRule(rule)
    {
        var lines = [];
        for (var i = 0; i < rule.selectors.length; ++i) {
            var selector = rule.selectors[i];
            lines.push(`selector ${i}: "${selector.text}"`);
            var selectorLines = dumpTextNode(selector);
            lines = lines.concat(indent(selectorLines));
        }
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
    var T = Sass.SASSSupport.PropertyChangeType;
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
        var selectorText = rule.selectors.map(selector => selector.text).join(",");
        InspectorTest.addResult("Changes for rule: " + selectorText);
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
        if (textNode.document.text.extract(textNode.range) !== textNode.text)
            invalidNodes.push(textNode);
    }
}

InspectorTest.validateMapping = function(mapping)
{
    InspectorTest.addResult("Mapped CSS: " + mapping._compiledToSource.size);
    InspectorTest.addResult("Mapped SCSS: " + mapping._sourceToCompiled.size);
    var cssNodes = mapping._compiledToSource.keysArray();
    var staleCSS = 0;
    var staleSASS = 0;
    for (var i = 0; i < cssNodes.length; ++i) {
        var cssNode = cssNodes[i];
        staleCSS += cssNode.document !== mapping.compiledModel().document ? 1 : 0;
        var sassNode = mapping.toSourceNode(cssNode);
        var sassAST = mapping.sourceModels().get(sassNode.document.url);
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
    var uiSourceCode = Workspace.workspace.uiSourceCodeForURL(url);
    uiSourceCode.addRevision(newText);
}

InspectorTest.runCSSEditTests = function(header, tests)
{
    var astSourceMap;
    InspectorTest.loadASTMapping(header, onMapping);

    function onMapping(map)
    {
        astSourceMap = map;
        InspectorTest.addResult("INITIAL MODELS");
        logASTText(map.compiledModel(), true);
        for (var ast of map.sourceModels().values())
            logASTText(ast, true);
        runTests();
    }

    function runTests()
    {
        if (!tests.length) {
            InspectorTest.completeTest();
            return;
        }
        var test = tests.shift();
        logTestName(test.name);
        var text = astSourceMap.compiledModel().document.text.value();
        var edits = test(text);
        logSourceEdits(text, edits);
        var ranges = edits.map(edit => edit.oldRange);
        var texts = edits.map(edit => edit.newText);
        astSourceMap.editCompiled(ranges, texts)
            .then(onEditsDone);
    }

    function onEditsDone(result)
    {
        if (!result.map) {
            InspectorTest.addResult("SASSProcessor failed to process edits.");
            runTests();
            return;
        }
        logASTText(result.map.compiledModel());
        for (var sassURL of result.newSources.keys()) {
            var ast = result.map.sourceModels().get(sassURL);
            logASTText(ast);
        }
        runTests();
    }

    function logASTText(ast, avoidIndent, customTitle)
    {
        customTitle = customTitle || ast.document.url.split("/").pop();
        InspectorTest.addResult("===== " + customTitle + " =====");
        var text = ast.document.text.value().replace(/ /g, ".");
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
            line += String.sprintf(" '%s' => '%s'", (new Common.Text(text)).extract(range), edit.newText);
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
    var sourceRange = new Common.SourceRange(match.index, match[0].length);
    var textRange = new Common.Text(source).toTextRange(sourceRange);
    return new Common.SourceEdit("", textRange, newText);
}

}

