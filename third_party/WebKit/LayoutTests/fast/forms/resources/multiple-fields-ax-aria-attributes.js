function focusedElementDescription()
{
    var element = accessibilityController.focusedElement;
    return element.deprecatedHelpText + ', ' +  element.valueDescription + ', intValue:' + element.intValue + ', range:'+ element.minValue + '-' + element.maxValue;
}

function checkFocusedElementAXAttributes(expected) {
    shouldBeEqualToString('focusedElementDescription()', expected);
}
