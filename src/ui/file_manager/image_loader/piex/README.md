Install emscripten http://lmgtfy.com/?q=install+the+emscripten+sdk

  % git clone https://github.com/juj/emsdk.git
  % cd emsdk
  % ./emsdk install latest
  % ./emsdk activate latest
  % source ./emsdk_env.sh

Install piexwasm project components

  % cd ui/file_manager/image_loader/piex
  % npm install

Build piexwasm code a.out.js a.out.wasm

  % npm run build

Run tests

  % npm run test
