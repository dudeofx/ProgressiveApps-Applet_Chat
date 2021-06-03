emcc ChatClient.c --pre-js ChatClient.Interface.js -s WASM=1 -lwebsocket.js -o ChatClient.js

