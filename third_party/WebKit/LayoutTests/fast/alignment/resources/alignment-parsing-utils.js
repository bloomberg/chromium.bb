function checkValues(element, property, propertyID, value, computedValue)
{
    window.element = element;
    var elementID = element.id || "element";
    shouldBeEqualToString("element.style." + property, value);
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('" + propertyID + "')", computedValue);
}

function checkBadValues(element, property, propertyID, value)
{
    element.style.justifyItems = value;
    checkValues(element, property, propertyID, "", "start");
}

function checkInitialValues(element, property, propertyID, display, value)
{
    var initial = "start";
    if (display == "grid" || display == "flex") {
      element.style.display = display;
      initial = "stretch";
    }

    element.style.justifyItems = value;
    checkValues(element, property, propertyID, value, value);
    element.style.justifyItems = "initial";
    checkValues(element, property, propertyID, "initial", initial);
}

function checkInheritValues(element, property, propertyID, value)
{
    parentElement = document.createElement("div");
    document.body.appendChild(parentElement);
    parentElement.style.justifyItems = value;
    checkValues(parentElement, property, propertyID, value, value);

    element = document.createElement("div");
    parentElement.appendChild(element);
    element.style.justifyItems = "inherit";
    checkValues(element, property, propertyID, "inherit", value);
}

function checkLegacyValues(element, property, propertyID, value)
{
    document.body.appendChild(parentElement);
    parentElement.style.justifyItems = value;
    checkValues(parentElement, property, propertyID, value, value);

    element = document.createElement("div");
    parentElement.appendChild(element);
    checkValues(element, property, propertyID, "", value);
}
