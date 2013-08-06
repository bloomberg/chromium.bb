function testElementStyle(propertyJS, propertyCSS, type, value)
{
    if (type != null) {
        shouldBe("e.style." + propertyJS, "'" + value + "'");
        shouldBe("e.style.getPropertyCSSValue('" + propertyCSS + "').toString()", "'" + type + "'");
        shouldBe("e.style.getPropertyCSSValue('" + propertyCSS + "').cssText", "'" + value + "'");
    } else
        shouldBeNull("e.style.getPropertyCSSValue('" + propertyCSS + "')");
}

function testComputedStyle(propertyJS, propertyCSS, type, value)
{
    computedStyle = window.getComputedStyle(e, null);
    shouldBe("computedStyle." + propertyJS, "'" + value + "'");
    shouldBe("computedStyle.getPropertyCSSValue('" + propertyCSS + "').toString()", "'" + type + "'");
    shouldBe("computedStyle.getPropertyCSSValue('" + propertyCSS + "').cssText", "'" + value + "'");
}

description("Test to make sure text-decoration-line property returns values properly.")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

testContainer.innerHTML = '<div id="test">hello world</div>';
debug("Initial value:");
e = document.getElementById('test');
testElementStyle("textDecorationLine", "text-decoration-line", null, '');
testComputedStyle("textDecorationLine", "text-decoration-line", "[object CSSPrimitiveValue]", "none");
debug('');

debug("Initial value (explicit):");
e.style.textDecorationLine = 'initial';
testElementStyle("textDecorationLine", "text-decoration-line", "[object CSSValue]", "initial");
testComputedStyle("textDecorationLine", "text-decoration-line", "[object CSSPrimitiveValue]", "none");
debug('');

debug("Value 'none':");
e.style.textDecorationLine = 'none';
testElementStyle("textDecorationLine", "text-decoration-line", "[object CSSPrimitiveValue]", "none");
testComputedStyle("textDecorationLine", "text-decoration-line", "[object CSSPrimitiveValue]", "none");
debug('');

debug("Value 'underline':");
e.style.textDecorationLine = 'underline';
testElementStyle("textDecorationLine", "text-decoration-line", "[object CSSValueList]", "underline");
testComputedStyle("textDecorationLine", "text-decoration-line", "[object CSSValueList]", "underline");
debug('');

debug("Value 'overline':");
e.style.textDecorationLine = 'overline';
testElementStyle("textDecorationLine", "text-decoration-line", "[object CSSValueList]", "overline");
testComputedStyle("textDecorationLine", "text-decoration-line", "[object CSSValueList]", "overline");
debug('');

debug("Value 'line-through':");
e.style.textDecorationLine = 'line-through';
testElementStyle("textDecorationLine", "text-decoration-line", "[object CSSValueList]", "line-through");
testComputedStyle("textDecorationLine", "text-decoration-line", "[object CSSValueList]", "line-through");
debug('');

debug("Value 'blink' (valid, but ignored on computed style):");
e.style.textDecorationLine = 'blink';
testElementStyle("textDecorationLine", "text-decoration-line", "[object CSSValueList]", "blink");
testComputedStyle("textDecorationLine", "text-decoration-line", "[object CSSPrimitiveValue]", "none");
debug('');

debug("Value 'underline overline line-through blink':");
e.style.textDecorationLine = 'underline overline line-through blink';
testElementStyle("textDecorationLine", "text-decoration-line", "[object CSSValueList]", "underline overline line-through blink");
testComputedStyle("textDecorationLine", "text-decoration-line", "[object CSSValueList]", "underline overline line-through");
debug('');

debug("Value '':");
e.style.textDecorationLine = '';
testElementStyle("textDecorationLine", "text-decoration-line", null, '');
testComputedStyle("textDecorationLine", "text-decoration-line", "[object CSSPrimitiveValue]", "none");
debug('');

testContainer.innerHTML = '<div id="test-parent" style="text-decoration-line: underline;">hello <span id="test-ancestor" style="text-decoration-line: inherit;">world</span></div>';
debug("Parent gets 'underline' value:");
e = document.getElementById('test-parent');
testElementStyle("textDecorationLine", "text-decoration-line", "[object CSSValueList]", "underline");
testComputedStyle("textDecorationLine", "text-decoration-line", "[object CSSValueList]", "underline");
debug('');

debug("Ancestor should explicitly inherit value from parent when 'inherit' value is used:");
e = document.getElementById('test-ancestor');
testElementStyle("textDecorationLine", "text-decoration-line", "[object CSSValue]", "inherit");
testComputedStyle("textDecorationLine", "text-decoration-line", "[object CSSValueList]", "underline");
debug('');

debug("Ancestor should not implicitly inherit value from parent (i.e. when value is void):");
e.style.textDecorationLine = '';
testElementStyle("textDecorationLine", "text-decoration-line", null, '');
testComputedStyle("textDecorationLine", "text-decoration-line", "[object CSSPrimitiveValue]", "none");
debug('');

document.body.removeChild(testContainer);
