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

description("Test to make sure text-underline-position property returns values properly.")

// FIXME: This test tests property values 'auto', 'alphabetic' and 'under'. We don't fully match
// the specification as we don't support [ left | right ] and this is left for another implementation
// as the rendering will need to be added.

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

testContainer.innerHTML = '<div id="test">hello world</div>';

debug("Initial value:");
e = document.getElementById('test');
testElementStyle("textUnderlinePosition", "text-underline-position", null, '');
testComputedStyle("textUnderlinePosition", "text-underline-position", "[object CSSPrimitiveValue]", "auto");
debug('');

debug("Value '':");
e.style.textUnderlinePosition = '';
testElementStyle("textUnderlinePosition", "text-underline-position", null, '');
testComputedStyle("textUnderlinePosition", "text-underline-position", "[object CSSPrimitiveValue]", "auto");
debug('');

debug("Initial value (explicit):");
e.style.textUnderlinePosition = 'initial';
testElementStyle("textUnderlinePosition", "text-underline-position", "[object CSSValue]", "initial");
testComputedStyle("textUnderlinePosition", "text-underline-position", "[object CSSPrimitiveValue]", "auto");
debug('');

debug("Value 'auto':");
e.style.textUnderlinePosition = 'auto';
testElementStyle("textUnderlinePosition", "text-underline-position", "[object CSSPrimitiveValue]", "auto");
testComputedStyle("textUnderlinePosition", "text-underline-position", "[object CSSPrimitiveValue]", "auto");
debug('');

debug("Value 'alphabetic':");
e.style.textUnderlinePosition = 'alphabetic';
testElementStyle("textUnderlinePosition", "text-underline-position", "[object CSSPrimitiveValue]", "alphabetic");
testComputedStyle("textUnderlinePosition", "text-underline-position", "[object CSSPrimitiveValue]", "alphabetic");
debug('');

debug("Value 'under':");
e.style.textUnderlinePosition = 'under';
testElementStyle("textUnderlinePosition", "text-underline-position", "[object CSSPrimitiveValue]", "under");
testComputedStyle("textUnderlinePosition", "text-underline-position", "[object CSSPrimitiveValue]", "under");
debug('');

testContainer.innerHTML = '<div id="test-parent" style="text-underline-position: under;">hello <span id="test-ancestor">world</span></div>';
debug("Ancestor inherits values from parent:");
e = document.getElementById('test-ancestor');
testElementStyle("textUnderlinePosition", "text-underline-position", null, "");
testComputedStyle("textUnderlinePosition", "text-underline-position", "[object CSSPrimitiveValue]", "under");
debug('');

debug("Value 'auto alphabetic':");
e.style.textUnderlinePosition = 'auto alphabetic';
testElementStyle("textUnderlinePosition", "text-underline-position", null, "");
debug('');

debug("Value 'auto under':");
e.style.textUnderlinePosition = 'auto under';
testElementStyle("textUnderlinePosition", "text-underline-position", null, "");
debug('');

debug("Value 'under under':");
e.style.textUnderlinePosition = 'under under';
testElementStyle("textUnderlinePosition", "text-underline-position", null, "");
debug('');

debug("Value 'under under under':");
e.style.textUnderlinePosition = 'auto alphabetic under';
testElementStyle("textUnderlinePosition", "text-underline-position", null, "");
debug('');

document.body.removeChild(testContainer);
