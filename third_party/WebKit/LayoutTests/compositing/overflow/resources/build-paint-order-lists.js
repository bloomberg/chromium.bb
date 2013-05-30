function getPaintOrder(element)
{
  var divElementsBeforePromote = [];
  var divElementsAfterPromote = [];
  // Force a style recalc.
  document.body.style.overflow = 'scroll';
  document.body.offsetTop;
  document.body.style.overflow = '';
  document.body.offsetTop;

  var paintOrderListBeforePromote = window.internals.paintOrderListBeforePromote(element);
  var paintOrderListAfterPromote = window.internals.paintOrderListAfterPromote(element);

  for (var i = 0; i < paintOrderListBeforePromote.length; ++i)
    if (paintOrderListBeforePromote[i].nodeName === "DIV")
      divElementsBeforePromote.push(paintOrderListBeforePromote[i]);

  for (var i = 0; i < paintOrderListAfterPromote.length; ++i)
    if (paintOrderListAfterPromote[i].nodeName === "DIV")
      divElementsAfterPromote.push(paintOrderListAfterPromote[i]);

  return {"beforePromote": divElementsBeforePromote,
          "afterPromote": divElementsAfterPromote};
}

function comparePaintOrderLists(oldPaintOrder, newPaintOrder)
{
  if (oldPaintOrder.length !== newPaintOrder.length)
    return false;

  for (var i = 0; i < oldPaintOrder.length; i++)
    if (oldPaintOrder[i] !== newPaintOrder[i])
      return false;

  return true;
}

function countOccurrencesOfElementInPaintOrderList(paintOrder, element) {
  var occurrenceCount = 0;
  for (var i = 0; i < paintOrder.length; i++)
    if (paintOrder[i] === element)
      occurrenceCount++;

  return occurrenceCount;
}
