function testComputedStyle(ancestorValue, childValue)
{
    shouldBe("window.getComputedStyle(ancestor).getPropertyValue('text-indent')",  "'" + ancestorValue + "'");
    shouldBe("window.getComputedStyle(child).getPropertyValue('text-indent')",  "'" + childValue + "'");
    debug('');
}

function ownValueTest(ancestorValue, childValue)
{
    debug("Value of ancestor is '" + ancestorValue + "', while child is '" + childValue + "':");
    ancestor.style.textIndent = ancestorValue;
    child.style.textIndent = childValue;
    testComputedStyle(ancestorValue, childValue);
}

function inheritanceTest(ancestorValue)
{
    debug("Value of ancestor is '" + ancestorValue + "':");
    ancestor.style.textIndent = ancestorValue;
    testComputedStyle(ancestorValue, ancestorValue);
}

description("This test checks that the value of text-indent is properly inherited to the child.");

ancestor = document.getElementById('ancestor');
child = document.getElementById('child');

inheritanceTest("10px");
inheritanceTest("10px each-line");
inheritanceTest("10px hanging");
inheritanceTest("10px hanging each-line");

ownValueTest("10px each-line", "10px");
ownValueTest("10px hanging", "10px");
ownValueTest("10px hanging each-line", "10px");
