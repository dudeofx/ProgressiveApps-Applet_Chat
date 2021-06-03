## Server Internals

---

### Developer commentary.

I would consider this code as a first draft for a single threaded approach in implementing a websocket server. It does represent a complete project but its far from a commercial product. In the future I plan on working on a simple multi-player game (checkers). When I reach that point I will pick up again, overhaul it and hopefully improve its design.

NOTES: If there is no activity the sockets get dropped, didn't have a chance to add a heartbeat (keepalive) mechanism.

---

### Summary of logic flow

see jpeg file "server_logic_flow.jpg" for a visual aid.

#### 1. Server_Loop()

At the root of the server is the server loop. This function coordinates 3 tasks: poll the network, accept new connections and read incomming data. The most concerning is the reading incomming data. Accepting new connections only intializes a communication channel. 

#### 2. ReadSocketData()

The job of ReadSocketData() is to remove the WebSocket layer. This function has the following jobs:

   * removing frame headers 
   * removing the mask
   * respond to pings and pongs
   * assemble frame fragments. 

see rfc6455 section 5 WebSocket data framing.

#### 3. ProcessData()

After a new connection is accepted, the client and the server perform the handshake ritual. The server has to manage two protocols: The WebSocket protocol and our own sub-protocol which I'm calling the tPacket protocol. Each has its own handshake procedure. The jpeg file "basic_engagement_procedure.jpg" illustrates the process.

Data flows from ReadSocketData() to ProcessData(). When data arrives at ProcessData() we assured that framing is removed and the task at hand is to get thru the handshake. So we start at level 0 and work our way up to level 3.

   * trust level 0: the websocket handshake is performed. See rfc6455 section 4 WebSocket opening handshake
   * trust level 1: server awaits magic string
   * trust level 2: server awaits user nick
   * trust level 3: ProcessPacket()

#### 4. ProcessPacket()

After we reach trust level 3 data simply flows from  ProcessData() to ProcessPacket(). When data arrives at ProcessPacket() The handshake is behind us and we don't have to worry about the WebSocket protocol. The task at hand is to process packet data. The tPacket protocol consists of a header and a data-blob. 

Below is the data structure for the header. The item of interest is the command which tells us what to do with the data-blob.

    typedef struct _tPacket tPacket;
    struct _tPacket {
       char frame[4];     // its there for a sanity check, it indicates this is the begining of a packet
       int uid;           // unique id for packet; used by the client to keep track of server responses
       int size;          // should equal sizeof(tPacket) + data_size
       int cmd;           // command
       int p1;            // parameter 1
       int p2;            // parameter 2
    };

Below is the list of commands used by our tPacket protocol. In truth ProcessPacket() only worries about two commands: \_SET_ROOM and and \_NEW_MSG. This is after all a prototype server. 


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

To keep it simple, the server does not allow changing nicks or rooms. 





