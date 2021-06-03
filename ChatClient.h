#ifndef __ChatClient_H__
#define __ChatClient_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <malloc.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include <emscripten.h>
#include <emscripten/websocket.h>
#include "tPacket.h"

typedef struct _tChatClient tChatClient;
typedef struct _tPacketCart tPacketCart;
typedef struct _tChatUser   tChatUser;

struct _tPacketCart {
   tPacket *packet;
   int uid;
   void (*func_handler)(tChatClient *, tPacket *, void *, int);
   void *func_data;
   int next;
};

struct _tChatUser {
   char *nick;
   int id;
   tChatUser *next;
};

struct _tChatClient {
   char *client_id;
   char *room;
   char *nick;
   char *host;
   char *url;

   tChatUser *users;

   EmscriptenWebSocketCreateAttributes attrs;

   int status;
   EMSCRIPTEN_WEBSOCKET_T comms;

   int callbacks;
   int call_tail;

   tChatClient *next;
};

//===[ functions that deal with user link list ]===================================
char *GetUserNick(tChatClient *ctx, int user_id);
void NewUser(tChatClient *ctx, tPacket *packet);
void DelUser(tChatClient *ctx, tPacket *packet);

//===[ Command delegation ]========================================================
void ProcessMsg(tChatClient *ctx, tPacket *packet);

//===[ Data submission ]===========================================================
void Submit_MagicString(tChatClient *ctx);
void Submit_ChatRoom(tChatClient *ctx);
void Submit_UserNick(tChatClient *ctx);
void Submit_Msg(tChatClient *ctx, char *msg);

//===[ cart wheel data structure ]=================================================
void SetCartToEmpty(int cart);
int SubmitPacket(tChatClient *ctx, tPacket *packet, void (*func)(tChatClient *, tPacket *, void *, int), void *data);
void Callback(tChatClient *ctx, int code, tPacket *packet);

//===[ tPacket callback handlers ]=================================================
void Callback_ChatRoom(tChatClient *ctx, tPacket *packet, void *data, int code);
void Callback_UserNick(tChatClient *ctx, tPacket *packet, void *data, int code);

//===[ emscripten WebSocket callback handlers ]====================================
EM_BOOL Callback_OnOpen(int eventType, const EmscriptenWebSocketOpenEvent *e, void *user_data);
EM_BOOL Callback_OnClose(int eventType, const EmscriptenWebSocketCloseEvent *e, void *userData);
EM_BOOL Callback_OnError(int eventType, const EmscriptenWebSocketErrorEvent *e, void *userData);
EM_BOOL Callback_OnMessage(int eventType, const EmscriptenWebSocketMessageEvent *e, void *userData);

#endif

