
import { GPU } from "./interface.js";

type ImplType = Promise<GPU>;
let impl: ImplType;

export function getGPU(): ImplType {
  if (impl) {
    return impl;
  }

  let dawn = false;
  if (typeof require !== "undefined") {
    const fs = require("fs");
    if (fs.existsSync("dawn/index.node")) {
      dawn = true;
    }
  }

  if (dawn) {
    impl = import("../../../dawn").then((mod) => mod.default);
  } else {
    // tslint:disable-next-line no-console
    console.warn("Dawn implementation not found. Using dummy.");
    impl = import("./dummy.js").then((mod) => mod.default);
  }
  return impl;
}
