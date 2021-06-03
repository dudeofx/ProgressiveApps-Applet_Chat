const char *_this_src_file = __FILE__;

#include "ChatClient.h"
#define _MAX_PACKET_CARTS 10240

tChatClient *ctx = NULL;
tPacketCart wheel[_MAX_PACKET_CARTS];
int         empty_carts;
int         empty_tail;
int         next_uid;

//-----------------------------------------------------------------------
//===[ Private JS interface functions ]============================================
//-----------------------------------------------------------------------
EM_JS(void, NewMsg, (char *p1, char *p2, char *p3), {
// Javascript function to add msg to chat history
   var client_id = UTF8ToString(p1);
   var nick = UTF8ToString(p2);
   var msg = UTF8ToString(p3);

   var target = "ChatClient_"+client_id+"_history";
   var div = document.getElementById(target);
   div.innerHTML += nick + ": " + msg + "<br>";
   div.scrollTop = div.scrollHeight;
});
//-----------------------------------------------------------------------
EM_JS(void, SwitchTab, (char *p1, char *p2), {
// Javascript function to switch tab on GUI
// p2 == "msgs" : set tab to message history
// p2 == "login" : set tab to login screen
   var client_id = UTF8ToString(p1);
   var active_tab = UTF8ToString(p2);

   var history = "ChatClient_"+client_id+"_history";
   var target = "ChatClient_"+client_id+"_"+active_tab+"_tab";
   var input = "ChatClient_"+client_id+"_input";

   if (target != "ChatClient_"+client_id+"_login_tab") {
      document.getElementById("ChatClient_"+client_id+"_login_tab").style.display = "none";
   }

   if (target != "ChatClient_"+client_id+"_msgs_tab") {
      document.getElementById("ChatClient_"+client_id+"_msgs_tab").style.display = "none";
   }

   document.getElementById(target).style.display = "block";
   document.getElementById(history).innerHTML = "";
   document.getElementById(input).focus();

});
//-----------------------------------------------------------------------
//===[ Public JS interface functions ]=============================================
//-----------------------------------------------------------------------
EMSCRIPTEN_KEEPALIVE
void ChatClient_SendMsg(char *client_id, char *msg) {
   tChatClient *tmp;

   tmp = ctx;
   while (tmp != NULL) {
      if (strcmp(tmp->client_id, client_id) == 0) break;
      tmp = tmp->next;
   }
   if (tmp == NULL) return;

   Submit_Msg(tmp, msg);
}
//-----------------------------------------------------------------------
EMSCRIPTEN_KEEPALIVE
void ChatClient_Connect(char *client_id, char *nick, char *host) {
   tChatClient *tmp;

   tmp = ctx;
   while (tmp != NULL) {
      if (strcmp(tmp->client_id, client_id) == 0) break;
      tmp = tmp->next;
   }
   if (tmp == NULL) return;
   if (tmp->status != 0) return;

   if (tmp->nick != NULL) free(tmp->nick);
   tmp->nick = (char *) malloc(strlen(nick)+1);
   strcpy(tmp->nick, nick);

   if (tmp->host != NULL) free(tmp->host);
   tmp->host = (char *) malloc(strlen(host)+1);
   strcpy(tmp->host, host);

   if (tmp->url != NULL) free(tmp->url);
   tmp->url = (char *) malloc(strlen(host)+16);
   sprintf(tmp->url, "ws://%s:25021/", host);

   emscripten_websocket_init_create_attributes(&(tmp->attrs));
   tmp->attrs.url = tmp->url;

   if (tmp->comms >= 0) emscripten_websocket_delete(tmp->comms);
   tmp->comms = emscripten_websocket_new(&(tmp->attrs));

   emscripten_websocket_set_onopen_callback(tmp->comms, (void*) tmp, Callback_OnOpen);
   emscripten_websocket_set_onclose_callback(tmp->comms, (void*) tmp, Callback_OnClose);
   emscripten_websocket_set_onerror_callback(tmp->comms, (void*) tmp, Callback_OnError);
   emscripten_websocket_set_onmessage_callback(tmp->comms, (void*) tmp, Callback_OnMessage);   

   tmp->status = 1;
};
//-----------------------------------------------------------------------
// * A hard context is the structure that represents the ChatClient on the
//   emscripten side. There is a structure on the javascript also representing
//   the ChatClient. Lets refer to that one as the soft context
// * One context per ChatClient. A page can have multiple clients but not
//   duplicates. All clients share the same callback wheel.
EMSCRIPTEN_KEEPALIVE
void ChatClient_InitHardCtx(char *client_id, char *room) {
   const char *_this_api_name = __FUNCTION__;
   tChatClient *tmp = NULL;

   tmp = (tChatClient *) malloc(sizeof(tChatClient));
   memset(tmp, 0, sizeof(tChatClient));

   tmp->client_id = NULL;
   tmp->room = NULL;
   tmp->nick = NULL;
   tmp->host = NULL;
   tmp->users = NULL;

   tmp->status = 0;
   tmp->comms = -1;

   tmp->callbacks = -1;
   tmp->call_tail = -1;

   tmp->next = ctx;
   ctx = tmp;

   tmp->client_id = (char *) malloc(strlen(client_id)+1);
   strcpy(tmp->client_id, client_id);

   tmp->room = (char *) malloc(strlen(room)+1);
   strcpy(tmp->room, room);
}
//-----------------------------------------------------------------------
//===[ functions that deal with user link list ]===================================
//-----------------------------------------------------------------------
char *GetUserNick(tChatClient *ctx, int user_id) {
   tChatUser *mark;

   mark = ctx->users;
   while (mark != NULL) {
      if (mark->id == user_id) break;
      mark = mark->next;
   }

   return mark->nick;
}
//-----------------------------------------------------------------------
void NewUser(tChatClient *ctx, tPacket *packet) {
   int len = packet->size - sizeof(tPacket);
   char *data = ((char *) packet) + sizeof(tPacket);
   char msg[64];
   tChatUser *user;
   
   user = (tChatUser *) malloc(sizeof(tChatUser));
   user->nick = (char *) malloc(len + 1);
   memcpy(user->nick, data, len);
   user->nick[len] = '\0';
   user->id = packet->p1;
   user->next = ctx->users;
   ctx->users = user;
   
   sprintf(msg, "%.*s is in the room", len, data);
   NewMsg(ctx->client_id, ">>", msg);
}
//-----------------------------------------------------------------------
void DelUser(tChatClient *ctx, tPacket *packet) {
   int len = packet->size - sizeof(tPacket);
   char *data = ((char *) packet) + sizeof(tPacket);
   char msg[64];
   tChatUser *mark;
   tChatUser *tmp;

   mark = ctx->users;
   if (mark == NULL) return;

   if (mark->id == packet->p1) {
      ctx->users = mark->next;
      free(mark->nick);
      free(mark);
   } else {
      while (mark->next != NULL) {
         if (mark->next->id == packet->p1) {
            tmp = mark->next;
            mark->next = tmp->next;
            free(tmp->nick);
            free(tmp);
            break;
         }
         mark = mark->next;
      }
   }

   sprintf(msg, "%.*s left the room", len, data);
   NewMsg(ctx->client_id, ">>", msg);
}
//-----------------------------------------------------------------------
//===[ Command delegation ]========================================================
//-----------------------------------------------------------------------
void ProcessMsg(tChatClient *ctx, tPacket *packet) {
   switch (packet->cmd) {
      case _RESP_OK:
         Callback(ctx, _RESP_OK, packet);
         break;
      case _RESP_ERROR:
         Callback(ctx, _RESP_ERROR, packet);
         break;
      case _NEW_MSG: {
         char msg[256];
         char *data;
         char *nick;
         int data_size;

         nick = GetUserNick(ctx, packet->p1);

         data = ((char *) packet) + sizeof(tPacket);
         data_size = packet->size - sizeof(tPacket);
         sprintf(msg, "%.*s", data_size, data);

         NewMsg(ctx->client_id, nick, msg);
      } break;
      case _LOGGED_MSG: {
         char msg[256];
         char *data;
         int data_size;

         data = ((char *) packet) + sizeof(tPacket);
         data_size = packet->size - sizeof(tPacket);
         sprintf(msg, "%.*s", data_size, data);

         NewMsg(ctx->client_id, "log", msg);
      } break;
      case _NEW_USER:
         NewUser(ctx, packet);
         break;
      case _DEL_USER:
         DelUser(ctx, packet);
         break;
      default:
         printf("unknown command\n");
         printf("   uid %i\n", packet->uid);
         printf("   size %i\n", packet->size);
         printf("   cmd %i\n", packet->cmd);
         printf("   p1 %i\n", packet->p1);
         printf("   p2 %i\n", packet->p2);
         break;
   };
}
//-----------------------------------------------------------------------
//===[ Data submission ]===========================================================
//-----------------------------------------------------------------------
void Submit_MagicString(tChatClient *ctx) {
   const char *magic_str = "magic string :)";
   int size;
   tPacket *tmp;
   char *data;

   size = sizeof(tPacket) + strlen(magic_str);
   tmp = (tPacket *) malloc(size);
   memcpy(tmp->frame, _FRAME_SIG_, 4);
   tmp->uid = next_uid++;
   tmp->size = size;
   tmp->cmd = 0;
   tmp->p1 = 0;
   tmp->p2 = 0;
   data = ((char *) tmp) + sizeof(tPacket);
   memcpy(data, magic_str, strlen(magic_str));

   SubmitPacket(ctx, tmp, NULL, NULL);
}
//-----------------------------------------------------------------------
void Submit_ChatRoom(tChatClient *ctx) {
   int size;
   tPacket *tmp;
   char *data;

   size = sizeof(tPacket) + strlen(ctx->room);
   tmp = (tPacket *) malloc(size);
   memcpy(tmp->frame, _FRAME_SIG_, 4);
   tmp->uid = next_uid++;
   tmp->size = size;
   tmp->cmd = _SET_ROOM;
   tmp->p1 = 0;
   tmp->p2 = 0;
   data = ((char *) tmp) + sizeof(tPacket);
   memcpy(data, ctx->room, strlen(ctx->room));

   SubmitPacket(ctx, tmp, Callback_ChatRoom, NULL);
}
//-----------------------------------------------------------------------
void Submit_UserNick(tChatClient *ctx) {
   int size;
   tPacket *tmp;
   char *data;

   size = sizeof(tPacket) + strlen(ctx->nick);
   tmp = (tPacket *) malloc(size);
   memcpy(tmp->frame, _FRAME_SIG_, 4);
   tmp->uid = next_uid++;
   tmp->size = size;
   tmp->cmd = _SET_USER_NICK;
   tmp->p1 = 0;
   tmp->p2 = 0;
   data = ((char *) tmp) + sizeof(tPacket);
   memcpy(data, ctx->nick, strlen(ctx->nick));

   SubmitPacket(ctx, tmp, Callback_UserNick, NULL);
}
//-----------------------------------------------------------------------
void Submit_Msg(tChatClient *ctx, char *msg) {
   int size;
   tPacket *tmp;
   char *data;

   size = sizeof(tPacket) + strlen(msg);
   tmp = (tPacket *) malloc(size);
   memcpy(tmp->frame, _FRAME_SIG_, 4);
   tmp->uid = next_uid++;
   tmp->size = size;
   tmp->cmd = _NEW_MSG;
   tmp->p1 = 0;
   tmp->p2 = 0;
   data = ((char *) tmp) + sizeof(tPacket);
   memcpy(data, msg, strlen(msg));

   SubmitPacket(ctx, tmp, NULL, NULL);
}
//-----------------------------------------------------------------------
//===[ cart wheel data structure ]=================================================
//-----------------------------------------------------------------------
void SetCartToEmpty(int cart) {
   if (wheel[cart].packet != NULL) free(wheel[cart].packet);

   wheel[cart].packet = NULL;
   wheel[cart].uid = -1;
   wheel[cart].func_handler = NULL;
   wheel[cart].func_data = NULL;
   wheel[cart].next = -1;

   if (empty_tail == -1) {
      empty_carts = cart;
      empty_tail = cart;
   } else {
      wheel[empty_tail].next = cart;
      empty_tail = cart;
   }     
}
//-----------------------------------------------------------------------
int SubmitPacket(tChatClient *ctx, tPacket *packet, void (*func)(tChatClient *, tPacket *, void *, int), void *data) {
   int rcode = emscripten_websocket_send_binary(ctx->comms, packet, packet->size);

//   if (rcode < 0) return -1;

   if (func != NULL) {
      int cart = empty_carts;
      if (cart == -1) return -1;

      empty_carts = wheel[cart].next;
      if (empty_carts == -1) empty_tail = -1;

      wheel[cart].packet = packet;
      wheel[cart].uid = packet->uid;
      wheel[cart].func_handler = func;
      wheel[cart].func_data = data;
      wheel[cart].next = -1;

      if (ctx->call_tail == -1) {
         ctx->callbacks = ctx->call_tail = cart;
      } else {
         wheel[ctx->call_tail].next = cart;
         ctx->call_tail = cart;
      }
      return 0;
   }

   if (packet != NULL) free(packet);

   return 0;
}
//-----------------------------------------------------------------------
void Callback(tChatClient *ctx, int code, tPacket *packet) {
   int cart;
   int prev;

   cart = ctx->callbacks;
   if (cart == -1) return;

   if (packet->uid == wheel[cart].uid) {
      ctx->callbacks = wheel[cart].next;
      if (ctx->call_tail == cart) ctx->call_tail = -1;
   } else {
      do {
         prev = cart;
         cart = wheel[cart].next;
         if (cart == -1) return;
      } while (packet->uid != wheel[cart].uid);
      wheel[prev].next = wheel[cart].next;
      if (ctx->call_tail == cart) ctx->call_tail = prev;
   }

   void (*Handler)(tChatClient *, tPacket *, void *, int) = wheel[cart].func_handler;
   void *data = wheel[cart].func_data;
   Handler(ctx, packet, data, code);
   SetCartToEmpty(cart);
}
//-----------------------------------------------------------------------
//===[ tPacket callback handlers ]=================================================
//-----------------------------------------------------------------------
void Callback_ChatRoom(tChatClient *ctx, tPacket *packet, void *data, int code) {
// called back after the server respond from _SET_ROOM
   switch (code) {
      case _RESP_OK:
         SwitchTab(ctx->client_id, "msgs"); 
         printf("%s set room to: '%s'\n", ctx->client_id, ctx->room);
         break;
      case _RESP_ERROR:
         printf("panic dunno what to do :(\n");
         break;
      default:
         printf("received unrecognized opcode\n");
         break;
   }
}
//-----------------------------------------------------------------------
void Callback_UserNick(tChatClient *ctx, tPacket *packet, void *data, int code) {
// called back after the server respond from _SET_USER_NICK
   switch (code) {
      case _RESP_OK:
         printf("%s set nick to: '%s'\n", ctx->client_id, ctx->nick);
         Submit_ChatRoom(ctx); 
         break;
      case _RESP_ERROR:
         printf("TODO: nick rejected start over\n");
         break;
      default:
         printf("received unrecognized opcode\n");
         break;
   }
}
//-----------------------------------------------------------------------
//===[ emscripten WebSocket callback handlers ]====================================
//-----------------------------------------------------------------------
// called by emscripten on WebSocket open
EM_BOOL Callback_OnOpen(int eventType, const EmscriptenWebSocketOpenEvent *e, void *user_data) {
   tChatClient *tmp = user_data;
	printf("%s is connected\n", tmp->client_id);

   Submit_MagicString(tmp);
   Submit_UserNick(tmp);
   return 0;
}
//-----------------------------------------------------------------------
// called by emscripten on WebSocket close
EM_BOOL Callback_OnClose(int eventType, const EmscriptenWebSocketCloseEvent *e, void *userData) {
   tChatClient *ctx = userData;

   printf("%s closed connection\n", ctx->client_id);

   emscripten_websocket_close(e->socket, 0, 0);
   emscripten_websocket_delete(e->socket);

   SwitchTab(ctx->client_id, "login");
   ctx->comms = -1;
   ctx->status = 0; 

   return 0;
}
//-----------------------------------------------------------------------
// called by emscripten on WebSocket error
EM_BOOL Callback_OnError(int eventType, const EmscriptenWebSocketErrorEvent *e, void *userData) {
	printf("error(eventType=%d, userData=%d)\n", eventType, (int)userData);
   return 0;
}
//-----------------------------------------------------------------------
// called by emscripten on WebSocket for incomming data
// userdata contains the ChatClient context
// e->data should contain a packet (packet validation would go here)
EM_BOOL Callback_OnMessage(int eventType, const EmscriptenWebSocketMessageEvent *e, void *userData) {
   ProcessMsg((tChatClient *) userData, (tPacket *) e->data);
   return 0;
}
//-----------------------------------------------------------------------
//===[ program main() ]============================================================
//-----------------------------------------------------------------------
int main() {
   // initialize global vars
   empty_carts = 0;
   empty_tail = _MAX_PACKET_CARTS-1;
   for (int i = 0; i < _MAX_PACKET_CARTS; i++) {
      wheel[i].packet = NULL;
      wheel[i].func_handler = NULL;
      wheel[i].func_data = NULL;
      wheel[i].next = i+1;
   }
   wheel[empty_tail].next = -1;
   next_uid = 0x1000;

   // deploy applets 
   EM_ASM (
      ChatClientCtx.hostname = location.hostname;
      var x = document.getElementsByClassName("appChatClient");
      for (i = 0; i < x.length; i++) ChatClient_DeployApplet(x[i]);
   );
}
//-----------------------------------------------------------------------

