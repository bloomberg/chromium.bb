'use strict';
// Copyright (C) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Picker used by <input type='month' />
 */

function initializeMonthPicker(config) {
  global.picker = new MonthPicker(config);
  main.append(global.picker);
  main.style.border = '1px solid #bfbfbf';
  if (global.params.isBorderTransparent) {
    main.style.borderColor = 'transparent';
  }
  main.style.height = (MonthPicker.Height - 2) + 'px';
  main.style.width = (MonthPicker.Width - 2) + 'px';
  resizeWindow(MonthPicker.Width, MonthPicker.Height);
}

/**
 * MonthPicker: Custom element providing a month picker implementation.
 *              MonthPicker contains 2 parts:
 *                - year list view
 *                - today button
 */
class MonthPicker extends HTMLElement {
  constructor(config) {
    super();

    this.initializeFromConfig_(config);

    this.yearListView_ =
        new YearListView(this.minimumMonth_, this.maximumMonth_, config);
    this.append(this.yearListView_.element);
    this.initializeYearListView_();

    this.todayButton_ = new CalendarNavigationButton();
    this.append(this.todayButton_.element);
    this.initializeTodayButton_();

    window.addEventListener('resize', this.onWindowResize_);
    this.addEventListener('keydown', this.onKeyDown_);
  }

  initializeFromConfig_ = (config) => {
    const minimum = (typeof config.min !== 'undefined' && config.min) ?
        parseDateString(config.min) :
        Month.Minimum;
    const maximum = (typeof config.max !== 'undefined' && config.max) ?
        parseDateString(config.max) :
        Month.Maximum;
    this.minimumMonth_ = Month.createFromDay(minimum.firstDay());
    this.maximumMonth_ = Month.createFromDay(maximum.lastDay());
  };

  initializeYearListView_ = () => {
    this.yearListView_.setWidth(MonthPicker.YearWidth);
    this.yearListView_.setHeight(MonthPicker.YearHeight);
    if (global.params.isLocaleRTL) {
      this.yearListView_.element.style.right = MonthPicker.YearPadding + 'px';
      this.yearListView_.scrubbyScrollBar.element.style.right =
          MonthPicker.YearWidth + 'px';
    } else {
      this.yearListView_.element.style.left = MonthPicker.YearPadding + 'px';
      this.yearListView_.scrubbyScrollBar.element.style.left =
          MonthPicker.YearWidth + 'px';
    }
    this.yearListView_.element.style.top = MonthPicker.YearPadding + 'px';

    let yearForInitialScroll = this.selectedMonth ?
        this.selectedMonth.year - 1 :
        Month.createFromToday().year - 1;
    this.yearListView_.scrollToRow(yearForInitialScroll, false);
    this.yearListView_.selectWithoutAnimating(yearForInitialScroll);

    this.yearListView_.on(
        YearListView.EventTypeYearListViewDidSelectMonth,
        this.onYearListViewDidSelectMonth_);
  };

  onYearListViewDidSelectMonth_ = (sender, month) => {
    const selectedValue = month.toString();
    window.pagePopupController.setValueAndClosePopup(0, selectedValue);
  };

  initializeTodayButton_ = () => {
    this.todayButton_.element.textContent = global.params.todayLabel;
    this.todayButton_.element.setAttribute(
        'aria-label', global.params.todayLabel);
    this.todayButton_.element.classList.add(MonthPicker.ClassNameTodayButton);
    const monthContainingToday = Month.createFromToday();
    this.todayButton_.setDisabled(
        !this.yearListView_.isValid(monthContainingToday));
    this.todayButton_.on(
        CalendarNavigationButton.EventTypeButtonClick,
        this.onTodayButtonClick_);
  };

  onTodayButtonClick_ = (sender) => {
    const selectedValue = Month.createFromToday().toString();
    window.pagePopupController.setValueAndClosePopup(0, selectedValue);
  };

  onKeyDown_ = (event) => {
    switch (event.key) {
      case 'Enter':
        // Don't do anything here if user has hit Enter on 'This month'
        // button.  We'll handle that in this.onTodayButtonClick_.
        if (!event.target.matches('.calendar-navigation-button')) {
          if (this.selectedMonth) {
            window.pagePopupController.setValueAndClosePopup(
                0, this.selectedMonth.toString());
          } else {
            window.pagePopupController.closePopup();
          }
        }
        break;
      case 'Escape':
        if (!this.selectedMonth ||
            (this.selectedMonth.equals(this.initialSelectedMonth))) {
          window.pagePopupController.closePopup();
        } else {
          this.resetToInitialValue_();
          window.pagePopupController.setValue(
              this.hadValidValueWhenOpened ?
                  this.initialSelectedMonth.toString() :
                  '');
        }
        break;
      case 'ArrowUp':
      case 'ArrowDown':
      case 'ArrowLeft':
      case 'ArrowRight':
      case 'PageUp':
      case 'PageDown':
        if (this.selectedMonth) {
          window.pagePopupController.setValue(this.selectedMonth.toString());
        }
        break;
    }
  };

  resetToInitialValue_ = () => {
    this.yearListView_.setSelectedMonthAndUpdateView(this.initialSelectedMonth);
  };

  onWindowResize_ = (event) => {
    window.removeEventListener('resize', this.onWindowResize_);
    this.yearListView_.element.focus();
  };

  get selectedMonth() {
    return this.yearListView_._selectedMonth;
  };

  get initialSelectedMonth() {
    return this.yearListView_._initialSelectedMonth;
  };

  get hadValidValueWhenOpened() {
    return this.yearListView_._hadValidValueWhenOpened;
  };
}
MonthPicker.Width = 232;
MonthPicker.YearWidth = 194;
MonthPicker.YearHeight = 128;
MonthPicker.YearPadding = 12;
MonthPicker.Height = 182;
MonthPicker.ClassNameTodayButton = 'today-button-refresh';
window.customElements.define('month-picker', MonthPicker);
