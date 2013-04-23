(function() {

function checkColumnRowValues(gridItem, columnValue, rowValue)
{
    this.gridItem = gridItem;
    this.gridColumnValue = columnValue;
    this.gridRowValue = rowValue;

    var gridStartEndValues = columnValue.split("/")
    this.gridStartValue = gridStartEndValues[0].trim();
    this.gridEndValue = gridStartEndValues[1].trim();

    var gridBeforeAfterValues = rowValue.split("/")
    this.gridBeforeValue = gridBeforeAfterValues[0].trim();
    this.gridAfterValue = gridBeforeAfterValues[1].trim();

    shouldBe("getComputedStyle(gridItem, '').getPropertyValue('-webkit-grid-column')", "gridColumnValue");
    shouldBe("getComputedStyle(gridItem, '').getPropertyValue('-webkit-grid-start')", "gridStartValue");
    shouldBe("getComputedStyle(gridItem, '').getPropertyValue('-webkit-grid-end')", "gridEndValue");
    shouldBe("getComputedStyle(gridItem, '').getPropertyValue('-webkit-grid-row')", "gridRowValue");
    shouldBe("getComputedStyle(gridItem, '').getPropertyValue('-webkit-grid-before')", "gridBeforeValue");
    shouldBe("getComputedStyle(gridItem, '').getPropertyValue('-webkit-grid-after')", "gridAfterValue");
}

window.testColumnRowCSSParsing = function(id, columnValue, rowValue)
{
    var gridItem = document.getElementById(id);
    checkColumnRowValues(gridItem, columnValue, rowValue);
}

window.testColumnRowJSParsing = function(columnValue, rowValue, expectedColumnValue, expectedRowValue)
{
    var gridItem = document.createElement("div");
    document.body.appendChild(gridItem);
    gridItem.style.webkitGridColumn = columnValue;
    gridItem.style.webkitGridRow = rowValue;

    checkColumnRowValues(gridItem, expectedColumnValue ? expectedColumnValue : columnValue, expectedRowValue ? expectedRowValue : rowValue);

    document.body.removeChild(gridItem);
}

window.testColumnRowInvalidJSParsing = function(columnValue, rowValue)
{
    var gridItem = document.createElement("div");
    document.body.appendChild(gridItem);
    gridItem.style.webkitGridColumn = columnValue;
    gridItem.style.webkitGridRow = rowValue;

    checkColumnRowValues(gridItem, "auto / auto", "auto / auto");

    document.body.removeChild(gridItem);
}

})();
