#ifndef __tPacket_h__
#define __tPacket_h__

#define _FRAME_SIG_  "Ej8@"

// commands
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

typedef struct _tPacket tPacket;

struct _tPacket {
   char frame[4];
   int uid;
   int size;   // size of packet + data as a whole
   int cmd;
   int p1;
   int p2;
};

#endif

