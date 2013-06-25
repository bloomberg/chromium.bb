var debugMode = false;

if (window.testRunner)
  testRunner.dumpAsText();

function write(str)
{
  var pre = document.getElementById('console');
  var text = document.createTextNode(str + '\n');
  pre.appendChild(text);
}

function didOptIn(element)
{
  var nonFastScrollableRects = window.internals.nonFastScrollableRects(document);
  var elementBoundingBox = window.internals.boundingBox(element);

  for (var i = 0; i < nonFastScrollableRects.length; ++i) {
    var rect = nonFastScrollableRects[i];

    if (rect.top === elementBoundingBox.top && rect.left === elementBoundingBox.left &&
        rect.bottom === elementBoundingBox.bottom && rect.right === elementBoundingBox.right)
      return false;
  }

  return true;
}

function getStackingOrder(element)
{
  var divElements = [];

  var stackingOrder = window.internals.nodesFromRect(document, 100, 75, 200, 200, 200, 200, false, false, false);

  for (var i = 0; i < stackingOrder.length; ++i)
    if (stackingOrder[i].nodeName === "DIV")
      divElements.push(stackingOrder[i]);

  return divElements;
}

function addDomElement(elementType, className, id, parent)
{
  var element = document.createElement(elementType);
  element.setAttribute("class", className);
  element.setAttribute("id", id);
  if (parent === "body")
    document.body.appendChild(element);
  else
    document.getElementById(parent).appendChild(element);
}

function buildDom()
{
  addDomElement("div", "", "ancestor", "body");
  addDomElement("div", "positioned", "predecessor", "ancestor");
  addDomElement("div", "container", "container", "ancestor");
  addDomElement("div", "scrolled", "firstChild", "container");
  addDomElement("div", "scrolled", "secondChild", "container");
  addDomElement("div", "", "normalFlow", "container");
  addDomElement("div", "positioned", "successor", "ancestor");
  addDomElement("pre", "", "console", "body");
}

var permutationIteration = 0;

// We've already decided on the ordering of the elements, now we need to do the
// rest of the permutations.
function permuteWithFixedOrdering(testPermutation, siblingIndex, containerIndex, firstChildIndex, secondChildIndex, numElementsWithZIndexZero)
{
  if (firstChildIndex > secondChildIndex)
    return;
  // One of the two children must be immediately to the right of us.
  if (numElementsWithZIndexZero === 3 && containerIndex !== firstChildIndex - 1 && containerIndex !== secondChildIndex - 1)
    return;
  // If we have 2 elements with z-index:0, they will be the container and the
  // sibling (we don't care about the case when the container and a child are
  // both z-index:0 because the tree order would ensure that ordering anyway).
  // The two elements with z-index:0 need to be adjacent in the ordering.
  if (numElementsWithZIndexZero === 2 && containerIndex !== siblingIndex - 1 && containerIndex !== siblingIndex + 1)
    return;

  for (var containerIsPositioned = 0; containerIsPositioned <= 1; ++containerIsPositioned) {
    for (var hasPositionedAncestor = 0; hasPositionedAncestor <= 1; ++hasPositionedAncestor) {
      if (containerIsPositioned === 1 && hasPositionedAncestor === 1)
        continue;
      for (var siblingPrecedesContainer = 0; siblingPrecedesContainer <= 1; ++siblingPrecedesContainer) {
        // If the sibling and the container both have z-index: 0, we only have
        // one choice for who gets to be the sibling. For example, if the
        // sibling is supposed to precede us in paint order, but we both have
        // z-index: 0, the order will depend entirely on tree order, so we have
        // to choose the predecessor as the sibling. If all elements have unique
        // z-indices, either the predecessor or the successor could be our
        // sibling; the z-indices will ensure that the paint order is correct.
        if (numElementsWithZIndexZero >= 2 && siblingPrecedesContainer === 1)
          continue;

        var sibling = predecessor;
        var toHide = successor;
        if ((numElementsWithZIndexZero === 1 && siblingPrecedesContainer === 0) ||
            (numElementsWithZIndexZero >= 2 && siblingIndex > containerIndex)) {
          sibling = successor;
          toHide = predecessor;
        }

        var ordering = [ null, null, null, null ];
        ordering[siblingIndex] = sibling;
        ordering[containerIndex] = hasPositionedAncestor === 1 ? ancestor : container;
        ordering[firstChildIndex] = firstChild;
        ordering[secondChildIndex] = secondChild;

        toHide.style.display = 'none';
        sibling.style.display = '';

        var positioned = null;
        if (containerIsPositioned === 1) {
          container.style.position = 'relative';
          positioned = container;
        } else
          container.style.position = '';

        if (hasPositionedAncestor === 1) {
          ancestor.style.position = 'relative';
          positioned = ancestor;
        } else
          ancestor.style.position = '';

        var currentZIndex = siblingPrecedesContainer === 1 ? 0 : -1;
        for (var currentIndex = containerIndex - 1; currentIndex >= 0; --currentIndex) {
          ordering[currentIndex].style.zIndex = currentZIndex.toString();
          currentZIndex--;
        }

        currentZIndex = 1;
        for (var currentIndex = containerIndex + 1; currentIndex < 4; ++currentIndex) {
          ordering[currentIndex].style.zIndex = currentZIndex.toString();
          currentZIndex++;
        }

        if (numElementsWithZIndexZero >= 1)
          ordering[containerIndex].style.zIndex = '0';

        if (numElementsWithZIndexZero >= 2)
            ordering[siblingIndex].style.zIndex = '0';

        // We want the first descendant succeeding us in the ordering to get
        // z-index: 0.
        if (numElementsWithZIndexZero === 3) {
          if (firstChildIndex > containerIndex)
            ordering[firstChildIndex].style.zIndex = '0';
          else
            ordering[secondChildIndex].style.zIndex = '0';
        }

        testPermutation(permutationIteration, ordering, hasPositionedAncestor, containerIsPositioned);
        permutationIteration++;
      }
    }
  }
}

function testOptInPermutation(count, ordering, hasPositionedAncestor, containerIsPositioned)
{
  if (!window.internals)
    return;

  var container = document.getElementById('container');
  var containerOptedIn = didOptIn(container);

  window.internals.setNeedsCompositedScrolling(container,
      window.internals.COMPOSITED_SCROLLING_ALWAYS_OFF);

  var oldStackingOrder = getStackingOrder(container);

  window.internals.setNeedsCompositedScrolling(container,
      window.internals.COMPOSITED_SCROLLING_ALWAYS_ON);

  var newStackingOrder = getStackingOrder(container);

  window.internals.setNeedsCompositedScrolling(container,
      window.internals.DO_NOT_FORCE_COMPOSITED_SCROLLING);

  var shouldOptIn = oldStackingOrder.length === newStackingOrder.length;
  for (var i = 0; i < oldStackingOrder.length; ++i) {
    if (oldStackingOrder[i] !== newStackingOrder[i]) {
      shouldOptIn = false;
      break;
    }
  }

  if (shouldOptIn !== containerOptedIn) {
    if (shouldOptIn)
      write("FAIL - should've automatically opted in but didn't " + count);
    else
      write('FAIL - automatically opted in and changed stacking order ' + count);

    var additionalFailureInfo = "\tOrdering:";
    for(var i = 0; i < ordering.length; ++i) {
      additionalFailureInfo += " " + ordering[i].id + " (z-index: " + ordering[i].style.zIndex + ")";
      if(i < ordering.length - 1)
        additionalFailureInfo += ",";
    }

    additionalFailureInfo += "\n\thasPositionedAncestor: " + hasPositionedAncestor + "; containerIsPositioned: " + containerIsPositioned;
    write(additionalFailureInfo);
  } else {
    if (shouldOptIn)
      write("PASS iteration " + count + ": opted in, since doing so does not change the stacking order.");
    else
      write("PASS iteration " + count + ": did not opt in, since doing so would change the stacking order.");
  }
}

function runPermutationSet(testPermutation, siblingIndex)
{
  var ancestor = document.getElementById('ancestor');
  var predecessor = document.getElementById('predecessor');
  var successor = document.getElementById('successor');
  var container = document.getElementById('container');
  var firstChild = document.getElementById('firstChild');
  var secondChild = document.getElementById('secondChild');

  write('Testing permutation set ' + siblingIndex + ' - this means that the "sibling" will be placed at the index ' + siblingIndex + ' of the paint order.')

  var indices = [ 0, 1, 2, 3 ];
  var siblingIndices = indices.slice(0);
  siblingIndices.splice(siblingIndex, 1);
  for (var containerSubindex = 0; containerSubindex < siblingIndices.length; ++containerSubindex) {
    var containerIndices = siblingIndices.slice(0);
    var containerIndex = containerIndices[containerSubindex];
    containerIndices.splice(containerSubindex, 1);
    var firstChildIndex = containerIndices[0];
    var secondChildIndex = containerIndices[1];

    permuteWithFixedOrdering(testPermutation, siblingIndex, containerIndex, firstChildIndex, secondChildIndex, 1);
    permuteWithFixedOrdering(testPermutation, siblingIndex, containerIndex, firstChildIndex, secondChildIndex, 2);
    permuteWithFixedOrdering(testPermutation, siblingIndex, containerIndex, firstChildIndex, secondChildIndex, 3);
  }
} // function doTest

function runOptInPermutationSet(permutationSet) {
  runPermutationSet(testOptInPermutation, permutationSet);
}
