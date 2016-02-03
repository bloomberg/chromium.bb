function clientRectToJson(rect) {
  if (!rect)
    return null;
  return {
    top: rect.top,
    right: rect.right,
    bottom: rect.bottom,
    left: rect.left,
    width: rect.width,
    height: rect.height
  };
}

function coordinatesToClientRectJson(top, right, bottom, left) {
  return {
    top: top,
    right: right,
    bottom: bottom,
    left: left,
    width: right - left,
    height: bottom - top
  };
}

function entryToJson(entry) {
  return {
    boundingClientRect: clientRectToJson(entry.boundingClientRect),
    intersectionRect: clientRectToJson(entry.intersectionRect),
    rootBounds: clientRectToJson(entry.rootBounds),
    time: entry.time,
    target: entry.target.id
  };
}
