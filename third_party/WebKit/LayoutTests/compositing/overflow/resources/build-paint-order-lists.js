function getPaintOrder(element)
{
  var divElementsBeforePromote = [];
  var divElementsAfterPromote = [];

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

function compareStackingOrderWithPaintOrder(stackingOrder, paintOrder)
{
  if (debugMode) {
    write("paint order:")
    for (var i = 0; i < paintOrder.length; i++)
      write(paintOrder[i].id + " " + paintOrder[i].className + " " + paintOrder[paintOrder.length - i - 1].tagName);

    write("stacking order:")
    for (var i = 0; i < stackingOrder.length; i++)
      write(stackingOrder[i].id + " " + stackingOrder[i].className + " " + stackingOrder[i].tagName);
  }

  if (stackingOrder.length < paintOrder.length)
    return false;

  // We expect the stacking order list to contain more than the paint order
  // list sometimes because after we promote, the container's children won't
  // appear in the stacking context's ancestor's lists anymore (which is
  // expected and correct).  They'll still be in the stacking order list.
  // The important part is that the order of the things present in the paint
  // order list is preserved in the stacking order list.
  for (var i = 0, j = 0; i < stackingOrder.length && j < paintOrder.length; i++)
    if (stackingOrder[i] === paintOrder[paintOrder.length - j - 1])
      j++;

  if (debugMode)
    write(stackingOrder.length + " " + i + " " + paintOrder.length + " " + j);

  return j == paintOrder.length;
}

function testPaintOrderListPermutation(count) {
  if (!window.internals)
    return;

  var container = document.getElementById('container');

  window.internals.setNeedsCompositedScrolling(container,
      window.internals.COMPOSITED_SCROLLING_ALWAYS_OFF);

  var oldStackingOrder = getStackingOrder(container);
  var oldPaintOrder = getPaintOrder(container);

  window.internals.setNeedsCompositedScrolling(container,
      window.internals.COMPOSITED_SCROLLING_ALWAYS_ON);

  var newStackingOrder = getStackingOrder(container);
  var newPaintOrder = getPaintOrder(container);

  window.internals.setNeedsCompositedScrolling(container,
      window.internals.DO_NOT_FORCE_COMPOSITED_SCROLLING);
  // The getPaintOrder() function should return a pair of paint orders.
  // One before promotion and one after. This pair of lists should remain
  // identical whether the element is actually currently promoted or not,
  // its purpose is to generate hypothetical pre- and post-lists to
  // determine if the element is promotable.
  if (!comparePaintOrderLists(oldPaintOrder, newPaintOrder))
    write("iteration " + count + " FAIL - paint order lists not identical before/after promotion");

  if (!compareStackingOrderWithPaintOrder(oldStackingOrder, oldPaintOrder.beforePromote))
    write("iteration " + count + " FAIL - paint order list before promote doesn't match stacking order");

  if (!compareStackingOrderWithPaintOrder(newStackingOrder, oldPaintOrder.afterPromote))
    write("iteration " + count + " FAIL - paint order list after promote doesn't match stacking order");

  var containerOccurrences = countOccurrencesOfElementInPaintOrderList(oldPaintOrder.beforePromote, container);
  if (containerOccurrences !== 1)
    write("iteration " + count + " FAIL - paint order list before promote contains " + containerOccurrences + " occurrences of container. Should be exactly 1.");

  containerOccurrences = countOccurrencesOfElementInPaintOrderList(oldPaintOrder.afterPromote, container);
  if (containerOccurrences !== 1)
    write("iteration " + count + " FAIL - paint order list after promote contains " + containerOccurrences + " occurrences of container. Should be exactly 1.");
}

function runPaintOrderPermutationSet(permutationSet) {
  runPermutationSet(testPaintOrderListPermutation, permutationSet);
}
