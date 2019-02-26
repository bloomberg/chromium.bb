// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview  Utility functions that are used in Cups printer setup dialogs.
 */

cr.define('settings.printing', function() {
  /**
   * Returns true if the printer's name and address is valid. This function
   * uses regular expressions to determine whether the provided printer name
   * and address are valid. Address can be either an ipv4/6 address or a
   * hostname followed by an optional port.
   * NOTE: The regular expression for hostnames will allow hostnames that are
   * over 255 characters.
   * @param {string} name
   * @param {string} address
   * @return {boolean}
   */
  function isNameAndAddressValid(name, address) {
    if (!name || !address)
      return false;

    const hostnamePrefix = '([a-z\\d]|[a-z\\d][a-z\\d\\-]{0,61}[a-z\\d])';

    // Matches an arbitrary number of 'prefix patterns' which are separated by a
    // dot.
    const hostnameSuffix = `(\\.${hostnamePrefix})*`;

    // Matches an optional port at the end of the address.
    const portNumber = '(:\\d+)?';

    const ipv6Full = '(([a-f\\d]){1,4}(:(:)?([a-f\\d]){1,4}){1,7})';

    // Special cases for addresses using a shorthand notation.
    const ipv6Prefix = '(::([a-f\\d]){1,4})';
    const ipv6Suffix = '(([a-f\\d]){1,4}::)';
    const ipv6Combined = `(${ipv6Full}|${ipv6Prefix}|${ipv6Suffix})`;
    const ipv6WithPort = `(\\[${ipv6Combined}\\]${portNumber})`;

    // Matches valid hostnames and ipv4 addresses.
    const hostnameRegex =
        new RegExp(`^${hostnamePrefix}${hostnameSuffix}${portNumber}$`, 'i');

    // Matches valid ipv6 addresses.
    const ipv6AddressRegex =
        new RegExp(`^(${ipv6Combined}|${ipv6WithPort})$`, 'i');

    const invalidIpv6Regex = new RegExp('.*::.*::.*');

    return hostnameRegex.test(address) ||
        (ipv6AddressRegex.test(address) && !invalidIpv6Regex.test(address));
  }

  /**
   * Returns true if the printer's manufacturer and model or ppd path is valid.
   * @param {string} manufacturer
   * @param {string} model
   * @param {string} ppdPath
   * @return {boolean}
   */
  function isPPDInfoValid(manufacturer, model, ppdPath) {
    return !!((manufacturer && model) || ppdPath);
  }

  return {
    isNameAndAddressValid: isNameAndAddressValid,
    isPPDInfoValid: isPPDInfoValid,
  };
});
