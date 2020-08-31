function currentMonth() {
    return popupWindow.global.picker.currentMonth().toString();
}

function availableDayCells() {
    skipAnimation();
    return Array.prototype.map.call(popupWindow.document.querySelectorAll(".day-cell:not(.disabled):not(.hidden)"), function(element) {
        return element.$view.day.toString();
    }).sort().join();
}

function highlightedDayCells() {
    skipAnimation();
    return Array.prototype.map.call(popupWindow.document.querySelectorAll(".day-cell.highlighted:not(.hidden)"), function(element) {
        return element.$view.day.toString();
    }).sort().join();
}

function selectedDayCells() {
    skipAnimation();
    return Array.prototype.map.call(popupWindow.document.querySelectorAll(".day-cell.selected:not(.hidden)"), function(element) {
        return element.$view.day.toString();
    }).sort().join();
}

function availableWeekNumberCells() {
    skipAnimation();
    return Array.prototype.map.call(popupWindow.document.querySelectorAll(".week-number-cell.highlighted:not(.hidden)"), function(element) {
        return element.$view.day.toString();
    }).sort().join();
}

function highlightedWeekNumberCells() {
    skipAnimation();
    return Array.prototype.map.call(popupWindow.document.querySelectorAll(".week-number-cell.highlighted:not(.hidden)"), function(element) {
        return element.$view.day.toString();
    }).sort().join();
}

function selectedWeekNumberCells() {
    skipAnimation();
    return Array.prototype.map.call(popupWindow.document.querySelectorAll(".week-number-cell.selected:not(.hidden)"), function(element) {
        return element.$view.day.toString();
    }).sort().join();
}

function highlightedValue() {
    var highlight = popupWindow.global.picker.highlight();
    if (highlight)
        return highlight.toString();
    return null;
}

function selectedValue() {
    var selection = popupWindow.global.picker.selection();
    if (selection)
        return selection.toString();
    return null;
}

function skipAnimation() {
    popupWindow.AnimationManager.shared._animationFrameCallback(Infinity);
}

function hoverOverDayCellAt(column, row) {
  skipAnimation();
  const calendarTableView = popupWindow.global.picker.datePicker ?
      popupWindow.global.picker.datePicker.calendarTableView :
      popupWindow.global.picker.calendarTableView;
  var offset = cumulativeOffset(calendarTableView.element);
  var x = offset[0];
  var y = offset[1];
  if (calendarTableView.hasWeekNumberColumn)
    x += popupWindow.WeekNumberCell.Width;
  x += (column + 0.5) * popupWindow.DayCell.GetWidth();
  y += (row + 0.5) * popupWindow.DayCell.GetHeight() +
      popupWindow.CalendarTableHeaderView.GetHeight();
  eventSender.mouseMoveTo(x, y);
};

function clickDayCellAt(column, row) {
  hoverOverDayCellAt(column, row);
  eventSender.mouseDown();
  eventSender.mouseUp();
}

function hoverOverTimeCellAt(column, row) {
  const timeCellWidth = 56;
  const timeCellHeight = 36;

  skipAnimation();
  const timeColumns = popupWindow.global.picker.timePicker ?
      popupWindow.global.picker.timePicker.timeColumns :
      popupWindow.global.picker.timeColumns;
  var offset = cumulativeOffset(timeColumns);
  var x = offset[0];
  var y = offset[1];
  x += (column + 0.5) * timeCellWidth;
  y += (row + 0.5) * timeCellHeight;
  eventSender.mouseMoveTo(x, y);
}

function clickTimeCellAt(column, row) {
  hoverOverTimeCellAt(column, row);
  eventSender.mouseDown();
  eventSender.mouseUp();
}

function highlightedMonthButton() {
    skipAnimation();
    var year = popupWindow.global.picker.monthPopupView.yearListView.selectedRow + 1;
    return Array.prototype.map.call(popupWindow.document.querySelectorAll(".month-button.highlighted"), function(element) {
        return new popupWindow.Month(year, Number(element.dataset.month)).toString();
    }).sort().join();
}

function skipAnimationAndGetPositionOfMonthPopupButton() {
    skipAnimation();
    const calendarHeaderView = popupWindow.global.picker.datePicker ?
        popupWindow.global.picker.datePicker.calendarHeaderView :
        popupWindow.global.picker.calendarHeaderView;
    var buttonElement = calendarHeaderView.monthPopupButton.element;
    var offset = cumulativeOffset(buttonElement);
    return {x: offset[0] + buttonElement.offsetWidth / 2, y: offset[1] + buttonElement.offsetHeight / 2};
}

function hoverOverMonthPopupButton() {
    var position = skipAnimationAndGetPositionOfMonthPopupButton();
    eventSender.mouseMoveTo(position.x, position.y);
}

function clickMonthPopupButton() {
    hoverOverMonthPopupButton();
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function skipAnimationAndGetPositionOfPrevNextMonthButton(buttonIndex) {
  skipAnimation();
  var prevNextMonthButton = popupWindow.global.picker.element ?
      popupWindow.global.picker.element.querySelectorAll(
          '.calendar-navigation-button')[buttonIndex] :
      popupWindow.global.picker.querySelectorAll(
          '.calendar-navigation-button')[buttonIndex];
  prevNextMonthButton.foo;
  var offset = cumulativeOffset(prevNextMonthButton);
  return {
    x: offset[0] + prevNextMonthButton.offsetWidth / 2,
    y: offset[1] + prevNextMonthButton.offsetHeight / 2
  };
}

function hoverOverPrevNextMonthButton(buttonIndex) {
  var position = skipAnimationAndGetPositionOfPrevNextMonthButton(buttonIndex);
  eventSender.mouseMoveTo(position.x, position.y);
}

function clickPrevMonthButton() {
  hoverOverPrevNextMonthButton(/*buttonIndex*/ 0);
  eventSender.mouseDown();
  eventSender.mouseUp();
}

function clickNextMonthButton() {
  hoverOverPrevNextMonthButton(/*buttonIndex*/ 1);
  eventSender.mouseDown();
  eventSender.mouseUp();
}

function skipAnimationAndGetPositionOfTodayButton() {
  skipAnimation();
  const calendarTableView = popupWindow.global.picker.datePicker ?
      popupWindow.global.picker.datePicker.calendarTableView :
      popupWindow.global.picker.calendarTableView;
  var buttonElement =
      calendarTableView.element.querySelector('.today-button-refresh');
  var offset = cumulativeOffset(buttonElement);
  return {
    x: offset[0] + buttonElement.offsetWidth / 2,
    y: offset[1] + buttonElement.offsetHeight / 2
  };
}

function hoverOverTodayButton() {
  var position = skipAnimationAndGetPositionOfTodayButton();
  eventSender.mouseMoveTo(position.x, position.y);
}

function clickTodayButton() {
  hoverOverTodayButton();
  eventSender.mouseDown();
  eventSender.mouseUp();
}

function skipAnimationAndGetPositionOfThisMonthButton() {
  skipAnimation();
  const button =
      popupWindow.global.picker.querySelector('.today-button-refresh');
  var offset = cumulativeOffset(button);
  return {
    x: offset[0] + button.offsetWidth / 2,
    y: offset[1] + button.offsetHeight / 2
  };
}

function hoverOverThisMonthButton() {
  var position = skipAnimationAndGetPositionOfThisMonthButton();
  eventSender.mouseMoveTo(position.x, position.y);
}

function clickThisMonthButton() {
  hoverOverThisMonthButton();
  eventSender.mouseDown();
  eventSender.mouseUp();
}

function clickYearListCell(year) {
    skipAnimation();
    var row = year - 1;
    var rowCell = popupWindow.global.picker.monthPopupView.yearListView.cellAtRow(row);

    var rowScrollOffset = popupWindow.global.picker.monthPopupView.yearListView.scrollOffsetForRow(row);
    var scrollOffset = popupWindow.global.picker.monthPopupView.yearListView.scrollView.contentOffset();
    var rowOffsetFromViewportTop = rowScrollOffset - scrollOffset;

    var scrollViewOffset = cumulativeOffset(popupWindow.global.picker.monthPopupView.yearListView.scrollView.element);
    var rowCellCenterX = scrollViewOffset[0] + rowCell.element.offsetWidth / 2;
    var rowCellCenterY = scrollViewOffset[1] + rowOffsetFromViewportTop + rowCell.element.offsetHeight / 2;
    eventSender.mouseMoveTo(rowCellCenterX, rowCellCenterY);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function skipAnimationAndGetPositionOfMonthButton(year, month, inMonthPicker) {
    let yearListView = inMonthPicker ? popupWindow.global.picker.yearListView_
                                     : popupWindow.global.picker.monthPopupView.yearListView;
    skipAnimation();
    var row = year - 1;
    var rowCell = yearListView.cellAtRow(row);
    var rowScrollOffset = yearListView.scrollOffsetForRow(row);
    var scrollOffset = yearListView.scrollView.contentOffset();
    var rowOffsetFromViewportTop = rowScrollOffset - scrollOffset;

    var button = yearListView.buttonForMonth(new popupWindow.Month(year, month));
    var buttonOffset = cumulativeOffset(button);
    var rowCellOffset = cumulativeOffset(rowCell.element);
    var buttonOffsetRelativeToRowCell = [buttonOffset[0] - rowCellOffset[0], buttonOffset[1] - rowCellOffset[1]];

    var scrollViewOffset = cumulativeOffset(yearListView.scrollView.element);
    var buttonCenterX = scrollViewOffset[0] + buttonOffsetRelativeToRowCell[0] + button.offsetWidth / 2;
    var buttonCenterY = scrollViewOffset[1] + buttonOffsetRelativeToRowCell[1] + rowOffsetFromViewportTop + button.offsetHeight / 2;
    return {x: buttonCenterX, y: buttonCenterY};
}

function hoverOverMonthButton(year, month) {
    var position = skipAnimationAndGetPositionOfMonthButton(year, month, false);
    eventSender.mouseMoveTo(position.x, position.y);
}

function clickMonthButton(year, month) {
    hoverOverMonthButton(year, month);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function hoverOverMonthButtonForMonthPicker(year, month) {
    var position = skipAnimationAndGetPositionOfMonthButton(year, month, true);
    eventSender.mouseMoveTo(position.x, position.y);
}

var lastYearListViewScrollOffset = NaN;
function checkYearListViewScrollOffset() {
    skipAnimation();
    var scrollOffset = popupWindow.global.picker.monthPopupView.yearListView.scrollView.contentOffset();
    var change = lastYearListViewScrollOffset - scrollOffset;
    lastYearListViewScrollOffset = scrollOffset;
    return change;
}

function isCalendarTableScrollingWithAnimation() {
    var animator = popupWindow.global.picker.calendarTableView.scrollView.scrollAnimator();
    if (!animator)
        return false;
    return animator.isRunning();
}

function removeCommitDelay() {
    popupWindow.CalendarPicker.commitDelayMs = 0;
}

function setNoCloseOnCommit() {
    popupWindow.CalendarPicker.commitDelayMs = -1;
}
