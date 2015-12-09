var initialize_SassTest = function() {

InspectorTest.preloadModule("sass");

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

    function indent(lines)
    {
        return lines.map(line => "    " + line);
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

InspectorTest.parseSCSS = function(url, text)
{
    return self.runtime.instancePromise(WebInspector.TokenizerFactory)
        .then(onTokenizer);

    function onTokenizer(tokenizer)
    {
        return WebInspector.SASSSupport.parseSCSS(url, text, tokenizer);
    }
}

}

