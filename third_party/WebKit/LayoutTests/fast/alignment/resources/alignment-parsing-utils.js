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

function checkInitialValues(element, property, propertyID, display)
{
  var initial = "start";
  if (display == "grid" || display == "flex") {
    element.style.display = display;
    initial = "stretch";
  }

  element.style.justifyItems = "center";
  checkValues(element, property, propertyID, "center", "center");
  element.style.justifyItems = "initial";
  checkValues(element, property, propertyID, "initial", initial);
}
