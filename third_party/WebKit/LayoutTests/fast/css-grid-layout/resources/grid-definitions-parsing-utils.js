function testGridDefinitionsValues(element, columnValue, rowValue)
{
    window.element = element;
    shouldBeEqualToString("window.getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", columnValue);
    shouldBeEqualToString("window.getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", rowValue);
}

function testGridDefinitionsSetJSValues(columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue)
{
    window.element = document.createElement("div");
    document.body.appendChild(element);
    element.style.font = "10px Ahem"; // Used to resolve em font consistently.
    element.style.gridDefinitionColumns = columnValue;
    element.style.gridDefinitionRows = rowValue;
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", computedColumnValue || columnValue);
    shouldBeEqualToString("element.style.gridDefinitionColumns", jsColumnValue || columnValue);
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", computedRowValue || rowValue);
    shouldBeEqualToString("element.style.gridDefinitionRows", jsRowValue || rowValue);
    document.body.removeChild(element);
}

function testGridDefinitionsSetBadJSValues(columnValue, rowValue)
{
    window.element = document.createElement("div");
    document.body.appendChild(element);
    element.style.gridDefinitionColumns = columnValue;
    element.style.gridDefinitionRows = rowValue;
    // We can't use testSetJSValues as element.style.gridDefinitionRows returns "".
    testGridDefinitionsValues(element, "none", "none");
    document.body.removeChild(element);
}
