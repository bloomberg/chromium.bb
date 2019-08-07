// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to run and get details about user commands.
 */
class Commands {
  /**
   * @param {SwitchAccessInterface} switchAccess
   */
  constructor(switchAccess) {
    /**
     * SwitchAccess reference.
     * @private {SwitchAccessInterface}
     */
    this.switchAccess_ = switchAccess;

    /**
     * A map from command name to the function binding for the command.
     * @private {!Map<!SAConstants.Command, !function(): void>}
     */
    this.commandMap_ = this.buildCommandMap_();
  }

  /**
   * Run the function binding for the specified command.
   * @param {!SAConstants.Command} command
   */
  runCommand(command) {
    this.commandMap_.get(command)();
  }

  /**
   * Build a map from command name to the function binding for the command.
   * @return {!Map<!SAConstants.Command, !function(): void>}
   */
  buildCommandMap_() {
    return new Map([
      [
        SAConstants.Command.MENU,
        this.switchAccess_.enterMenu.bind(this.switchAccess_)
      ],
      [
        SAConstants.Command.NEXT,
        this.switchAccess_.moveForward.bind(this.switchAccess_)
      ],
      [
        SAConstants.Command.PREVIOUS,
        this.switchAccess_.moveBackward.bind(this.switchAccess_)
      ],
      [
        SAConstants.Command.SELECT,
        this.switchAccess_.selectCurrentNode.bind(this.switchAccess_)
      ]
    ]);
  }
}
