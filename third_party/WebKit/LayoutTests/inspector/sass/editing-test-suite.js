var initialize_SassEditingTest = function() {

InspectorTest.runEditingTests = function(cssAST)
{
    InspectorTest.runTestSuite([
        function testSetPropertyName(next)
        {
            var clone = cssAST.clone();
            for (var property of clone.rules[0].properties)
                property.name.setText("NEW-NAME");
            InspectorTest.addResult(clone.document.newText().value());
            next();
        },

        function testSetPropertyValue(next)
        {
            var clone = cssAST.clone();
            for (var property of clone.rules[0].properties)
                property.value.setText("NEW-VALUE");
            InspectorTest.addResult(clone.document.newText().value());
            next();
        },

        function testDisableProperties(next)
        {
            var clone = cssAST.clone();
            for (var property of clone.rules[0].properties)
                property.setDisabled(true);
            InspectorTest.addResult(clone.document.newText().value());
            next();
        },

        function testEnableProperties(next)
        {
            var clone = cssAST.clone();
            for (var property of clone.rules[0].properties)
                property.setDisabled(false);
            InspectorTest.addResult(clone.document.newText().value());
            next();
        },

        function testRemoveFirstProperty(next)
        {
            var clone = cssAST.clone();
            clone.rules[0].properties[0].remove();
            InspectorTest.addResult(clone.document.newText().value());
            next();
        },

        function testRemoveAllProperties(next)
        {
            var clone = cssAST.clone();
            var properties = clone.rules[0].properties;
            while (properties.length)
                properties[0].remove();
            InspectorTest.addResult(clone.document.newText().value());
            next();
        },

        function testInsertFirstProperty(next)
        {
            var clone = cssAST.clone();
            var rule = clone.rules[0];
            var anchor = rule.properties[0];
            rule.insertProperties(["NEW-NAME"], ["NEW-VALUE"], [false], anchor, true);
            InspectorTest.addResult(clone.document.newText().value());
            next();
        },

        function testInsertLastProperty(next)
        {
            var clone = cssAST.clone();
            var rule = clone.rules[0];
            var anchor = rule.properties[rule.properties.length - 1];
            rule.insertProperties(["NEW-NAME"], ["NEW-VALUE"], [false], anchor, false);
            InspectorTest.addResult(clone.document.newText().value());
            next();
        },

        function testInsertDisabledProperty(next)
        {
            var clone = cssAST.clone();
            var rule = clone.rules[0];
            var anchor = rule.properties[1];
            rule.insertProperties(["NEW-NAME"], ["NEW-VALUE"], [true], anchor, true);
            InspectorTest.addResult(clone.document.newText().value());
            next();
        },

        function testInsertMultipleProperties(next)
        {
            var clone = cssAST.clone();
            var rule = clone.rules[0];
            var anchor = rule.properties[rule.properties.length - 1];
            rule.insertProperties(["TRAILING-4", "TRAILING-3", "TRAILING-2", "TRAILING-1"],
                ["VALUE", "VALUE", "VALUE", "VALUE"],
                [false, false, false, false],
                anchor, false);
            InspectorTest.addResult(clone.document.newText().value());
            next();
        },

        function testPrependMultipleProperties(next)
        {
            var clone = cssAST.clone();
            var rule = clone.rules[0];
            var anchor = rule.properties[0];
            rule.insertProperties(["TRAILING-1", "TRAILING-2", "TRAILING-3", "TRAILING-4"],
                ["VALUE", "VALUE", "VALUE", "VALUE"],
                [false, false, false, false],
                anchor, true);
            InspectorTest.addResult(clone.document.newText().value());
            next();
        },

        function testAppendAndRemoveLastProperty(next)
        {
            var clone = cssAST.clone();
            var rule = clone.rules[0];
            var anchor = rule.properties[rule.properties.length - 1];
            rule.insertProperties(["NEW-NAME"], ["NEW-VALUE"], [false], anchor, false);
            anchor.remove();
            InspectorTest.addResult(clone.document.newText().value());
            next();
        },

        function testComplexChange(next)
        {
            var clone = cssAST.clone();
            var rule = clone.rules[0];
            var lastProperty = rule.properties[rule.properties.length - 1];
            rule.insertProperties(["NEW-NAME"], ["NEW-VALUE"], [false], lastProperty, false);
            lastProperty.name.setText("CHANGED");
            rule.properties[0].value.setText("CHANGED");
            rule.properties[1].setDisabled(true);
            InspectorTest.addResult(clone.document.newText().value());
            next();
        },
    ]);
}
}
