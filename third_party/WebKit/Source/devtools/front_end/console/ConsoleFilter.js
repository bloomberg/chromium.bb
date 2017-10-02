// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Console.ConsoleFilter = class {
  /**
   * @param {string} name
   * @param {!Array<!TextUtils.FilterParser.ParsedFilter>} parsedFilters
   * @param {!Object<string, boolean>=} levelsMask
   */
  constructor(name, parsedFilters, levelsMask) {
    this.name = name;
    this.parsedFilters = parsedFilters;
    this.levelsMask = levelsMask || Console.ConsoleFilter.defaultLevelsFilterValue();
    this.infoCount = 0;
    this.warningCount = 0;
    this.errorCount = 0;
  }

  /**
   * @return {!Object<string, boolean>}
   */
  static allLevelsFilterValue() {
    var result = {};
    for (var name of Object.values(ConsoleModel.ConsoleMessage.MessageLevel))
      result[name] = true;
    return result;
  }

  /**
   * @return {!Object<string, boolean>}
   */
  static defaultLevelsFilterValue() {
    var result = Console.ConsoleFilter.allLevelsFilterValue();
    result[ConsoleModel.ConsoleMessage.MessageLevel.Verbose] = false;
    return result;
  }

  /**
   * @param {!Console.ConsoleViewMessage} viewMessage
   * @return {boolean}
   */
  applyFilter(viewMessage) {
    var visible = this._shouldBeVisible(viewMessage);
    if (visible)
      this._incrementCounters(viewMessage.consoleMessage().level);
    return visible;
  }

  /**
   * @param {!Console.ConsoleViewMessage} viewMessage
   * @return {boolean}
   */
  _shouldBeVisible(viewMessage) {
    var message = viewMessage.consoleMessage();
    if (message.level && !this.levelsMask[/** @type {string} */ (message.level)])
      return false;

    for (var filter of this.parsedFilters) {
      if (!filter.key) {
        if (filter.regex && viewMessage.matchesFilterRegex(filter.regex) === filter.negative)
          return false;
        if (filter.text && viewMessage.matchesFilterText(filter.text) === filter.negative)
          return false;
      } else {
        switch (filter.key) {
          case Console.ConsoleFilter.FilterType.Context:
            if (!passesFilter(filter, message.context, false /* exactMatch */))
              return false;
            break;
          case Console.ConsoleFilter.FilterType.Source:
            var sourceNameForMessage = message.source ?
                ConsoleModel.ConsoleMessage.MessageSourceDisplayName.get(
                    /** @type {!ConsoleModel.ConsoleMessage.MessageSource} */ (message.source)) :
                message.source;
            if (!passesFilter(filter, sourceNameForMessage, true /* exactMatch */))
              return false;
            break;
          case Console.ConsoleFilter.FilterType.Url:
            if (!passesFilter(filter, message.url, false /* exactMatch */))
              return false;
            break;
        }
      }
    }
    return true;

    /**
     * @param {!TextUtils.FilterParser.ParsedFilter} filter
     * @param {?string|undefined} value
     * @param {boolean} exactMatch
     * @return {boolean}
     */
    function passesFilter(filter, value, exactMatch) {
      if (!value && !filter.negative)
        return false;
      if (value) {
        var filterText = /** @type {string} */ (filter.text).toLowerCase();
        var lowerCaseValue = value.toLowerCase();
        if (exactMatch && (lowerCaseValue === filterText) === filter.negative)
          return false;
        if (!exactMatch && lowerCaseValue.includes(filterText) === filter.negative)
          return false;
      }
      return true;
    }
  }

  /**
   * @param {?ConsoleModel.ConsoleMessage.MessageLevel} level
   */
  _incrementCounters(level) {
    if (level === ConsoleModel.ConsoleMessage.MessageLevel.Info ||
        level === ConsoleModel.ConsoleMessage.MessageLevel.Verbose)
      this.infoCount++;
    else if (level === ConsoleModel.ConsoleMessage.MessageLevel.Warning)
      this.warningCount++;
    else if (level === ConsoleModel.ConsoleMessage.MessageLevel.Error)
      this.errorCount++;
  }

  resetCounters() {
    this.infoCount = 0;
    this.warningCount = 0;
    this.errorCount = 0;
  }
};

/** @enum {string} */
Console.ConsoleFilter.FilterType = {
  Context: 'context',
  Source: 'source',
  Url: 'url'
};
