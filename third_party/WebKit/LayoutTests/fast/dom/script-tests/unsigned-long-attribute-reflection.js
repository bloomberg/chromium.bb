// http://whatwg.org/html#reflecting-content-attributes-in-idl-attributes
// http://whatwg.org/html#rules-for-parsing-non-negative-integers
function testUnsignedLong(interface, createElement, attribute)
{
    test(function()
    {
        var element = createElement();

        assert_equals(element[attribute], 0);

        function t(input, output)
        {
            element.setAttribute(attribute, input);
            assert_equals(element[attribute], output);
        }

        t("", 0);
        t("0", 0);
        t("1", 1);
        t("2147483647", 2147483647);
        t("2147483648", 0);
        t("-1", 0);
        t("+42", 42);
        t(" 42", 42);
        t("42!", 42);
    }, "get " + interface + "." + attribute);

    test(function()
    {
        var element = createElement();

        assert_false(element.hasAttribute(attribute));

        function t(input, output)
        {
            element[attribute] = input;
            assert_equals(element.getAttribute(attribute), output);
        }

        t(0, "0");
        t(2147483647, "2147483647");
        t(2147483648, "0");
        t(2147483700, "0");
        t(-1, "0");
        t(-3, "0");
    }, "set " + interface + "." + attribute);
}
