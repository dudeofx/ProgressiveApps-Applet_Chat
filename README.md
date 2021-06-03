# ProgressiveApps-Applet_Chat

A simple chat client written using emscripten. A server is also supplied which was written in C. 

It is 1st draft code. It does work thou. I just publish here to keep a record. Stuff I might want to go back to in the future. I overly document for several reasons:

    * stablish a habbit
    * reflect on my code. get out of the trees for once and look at the forest
    * maybe help somebody out or make it easier for somebody to help me

---

## Getting Started

### Compiling the client
Source files for the Client:

    ChatClient.c
    ChatClient.h
    ChatClient.interface.js
    tPacket.h

to compile execute "build.sh". The following files should be produced:

    ChatClient.js
    ChatClient.wasm
    ChatClient.js.mem

The following four files need to be copied over to your web server

    index.html
    ChatClient.js
    ChatClient.wasm
    ChatClient.js.mem

### Compiling the server

The applet requires two types of servers: a webserver and a websocket server. The web/http server is to deliver the applet on a traditional web page. The websocket server is for live communication. I developed my own WebSocket server for this applet. The code resides on the "ChatServer" folder. The websocket server needs to be compiled and run on the same machine as the webserver. 

Source files for the Server:

    ChatServer.c
    ChatServer.h
    ParsingUtils.c
    ParsingUtils.h
    WebSocketUtils.c
    WebSocketUtils.h
    tPacket.h

to compile execute "build.sh". The following file should be produced:

    chat_server

It is a prototype server meant to run on userspace. As soon as you execute it will display a series of dots "." Its to show that its running. As clients get connected and information is passed back and forth the server prints out what is going on. Its rather verbose. The WebSocket connection is made thru port: 25021.

To see something working make sure the chat_server is up. Point the browser to where the index.html file is located and login. If you want to simulate multiple users you can use multiple tabs. It is possible to have the websocket and web servers run on different machines but you have to manually provide the hostname on the client's main() function.

---

## Developer Commentary

I found a couple of complications in trying to get this applet done. First is that I was going for a multi-thread approach but I encountered red-tape. Firefox required CORS enabled and an encrypted connection in order to allow multi-threading. Firefox does this to defend users against spectre attacks. Other browsers are expected to do the same thing.

I decided not to use multi-threaded approach because SSL certificates would be a pain just to get simple applets working. This force me to rethink my approach to network programming. I found that a multi-threaded approach is not a requirement and that it's possible to write efficient programs using a single thread.

The second complication was that WebSockets are not regular sockets. WebSockets is a protocol that sits on top of a POSIX socket. This complicated my original plans at server side because now I had to do parsing and frame managing. I could have used a 3rd party library but I wanted this code to be as self-contained as possible. Implementing a protocol is a project in itself but furtunately WebSockets is a simple protocol.

---
