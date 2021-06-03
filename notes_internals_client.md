## Client Internals
-- *never thought describing a simple chat client be soo involved ehh*

The applet is event driven. The main() function initializes and deploys the applet. Thereafter, all gears get turned via events.

---

### Private JS interface 
Interacting with the DOM requires JavaScript. We do this by either placing functions on "ChatClient.Interface.js" or by using EM_JS(). Functions implemented using EM_JS() are considered to be private. 

#### List of function calls:

    // Adds a new message to the chat history
    void NewMsg(char *client_id, char *nick, char *msg);

    // Switches the tab from login between chat history
    void SwitchTab(char *client_id, char *active_tab);

---

### Public JS interface 
Functions called by the HTML/DOM side are considered to be public. These functions reside on "ChatClient.Interface.js". Functions implemented in C with the EMSCRIPTEN_KEEPALIVE macro are half private half public. They can be called publicly but their code is private. 

With some ammusement I found that every public JS function ended up with a C emscripten counter part. It makes sense, a public actor would benefit from a liaison in the private sector. 

Public functions need to coexist with other public code hence I added the "ChatClient_" prefix to isolate them a bit.

#### List of functions calls:

    // handles clicking the login button
    ChatClient_LoginBtn(client_id)
    void ChatClient_Connect(char *client_id, char *nick, char *host);

    // handles clicking the msg send button
    ChatClient_SendBtn(client_id)
    void ChatClient_SendMsg(char *client_id, char *msg);

    // used by main to deploy applet. This HTML/JS code needs to be public
    ChatClient_DeployApplet(obj)
    void ChatClient_InitHardCtx(char *client_id, char *room);



---

### The Protocol
The server and the client exchange tPacket structures back and forth as a protocol. For initial contact there are rules of engagement. That procedure is described in the picture "basic_engagement_procedure.jpg". In general a client sends requests and the server responds but at any point in time the server might send packets that the client might find useful. The client does not need to respond to the server.

A complete packet consists of a header and a data blob. The header is a tPacket structure which is used to determine what to do with the data-blob.

    typedef struct _tPacket tPacket;
    struct _tPacket {
       char frame[4];     // its there for a sanity check, it indicates this is the begining of a packet
       int uid;           // unique id for packet; used by the client to keep track of server responses
       int size;          // should equal sizeof(tPacket) + data_size
       int cmd;           // command
       int p1;            // parameter 1
       int p2;            // parameter 2
    };

I used simple command approach as a protocol. One command with two parameters to determine what to do with the data-blob. The following commands where implemented.

    enum {
       _NOP,               // ignore packet
       _SET_USER_NICK,     // client sets his user nick
       _SET_ROOM,          // client sets chat room
       _RESP_OK,           // server responds OK to requested cmd
       _RESP_ERROR,        // server responds ERROR to requested cmd
       _NEW_USER,          // server informs client of new user in chat room
       _DEL_USER,          // server informs client user left chat room
       _NEW_MSG,           // client/server submit/reports a new msg in room
       _LOGGED_MSG,        // server sends available msg history
       _MAX_MSG_COUNT
    };

---

### Chat users linked-list
The client keeps a list of the users that are on the room. The server keeps track of users with a unique id as opposed to their nick. Everytime it broadcasts a message the server only gives you the id of the owner. Hence the client has to match that id with the nick.

    struct _tChatUser {
       char *nick;       // c-string contain user nick
       int id;           // uid of user
       tChatUser *next;  // used for link-listing
    };

#### List of functions

    // return the nick given the uid
    char *GetUserNick(tChatClient *ctx, int user_id);

    // add user to the list
    void NewUser(tChatClient *ctx, tPacket *packet);

    // remove user from the list
    void DelUser(tChatClient *ctx, tPacket *packet);

---

### Command delegation
Once a packet is recevied its delegated to a specialist

#### List of functions

    // the big switch statement delegating the work
    void ProcessMsg(tChatClient *ctx, tPacket *packet);

---

### Data submission
To make it easy a series of dedicated functions handle a specific data submissions. These functions send specific data to the server.

#### List of functions

    // sends the magic string
    void Submit_MagicString(tChatClient *ctx);

    // sends the name of the chat room
    void Submit_ChatRoom(tChatClient *ctx);

    // sends the user nick
    void Submit_UserNick(tChatClient *ctx);

    // sends a text message
    void Submit_Msg(tChatClient *ctx, char *msg);

---

### Cart-wheel data structure
The job of the cart-wheel structure is to provide a callback mechanism for packets submitted to the server. What we want is the ability to link callback functions with packets. The callbacks get invoked after the server responds to the packets. Identifying which callback gets invoked is done by matching the tPacket.uid (tPacket.uid should equal tPacketCart.uid)

#### List of function calls:

    // empties out the cart
    void SetCartToEmpty(int cart);

    // sends the packet over the network, and queues in a callback if provisioned
    SubmitPacket(tChatClient *ctx, tPacket *packet, void (*func)(tChatClient *, tPacket *, void *, int), void *data);

    // invokes the callback associated with the packet (if any)
    void Callback(tChatClient *ctx, int code, tPacket *packet)

#### Submitting a packet to the cart-wheel
 - performed by SubmitPacket()

  * Grab an empty cart from the wheel
  * Attach the packet that was sent
  * Attach the callback function
  * Place the cart at the end of the callback tail

#### Invoking a callback
 - performed by Callback()

  * Given a uid of a packet search the callback link-list for a matching cart
  * Remove cart from link-list
  * Invoke the callback function
  * Clear the cart and put it back into the wheel.

---

### Packet callback handlers
When the client makes requests, the server usually responds with OK or ERROR. To process the responds, we have to supply a callback function when submitting a request. These callbacks pertain to our own tPacket sub-protocol. They do not pertain to the WebSocket protocol.

#### List of functions

    // called when a response Submit_ChatRoom() is received
    void Callback_ChatRoom(tChatClient *ctx, tPacket *packet, void *data, int code);

    // called when a response to Submit_UserNick() is received
    void Callback_UserNick(tChatClient *ctx, tPacket *packet, void *data, int code);

---

### WebSocket API callback handlers
Emscripten provides wrapper interface to JavaScript's WebSocket object. This means we are to provide four callback functions to make use of the interface.

#### List of functions

    // called by emscripten everytime the websocket object receives a full frame
    EM_BOOL Callback_OnMessage(int eventType, const EmscriptenWebSocketMessageEvent *e, void *userData);

    // called by emscripten when it encounters an error
    EM_BOOL Callback_OnError(int eventType, const EmscriptenWebSocketErrorEvent *e, void *userData);

    // called by emscripten when a WebSocket connection is successfull
    EM_BOOL Callback_OnOpen(int eventType, const EmscriptenWebSocketOpenEvent *e, void *user_data);

    // called by emscripten when a WebSocket connection is closed
    EM_BOOL Callback_OnClose(int eventType, const EmscriptenWebSocketCloseEvent *e, void *userData);

---




