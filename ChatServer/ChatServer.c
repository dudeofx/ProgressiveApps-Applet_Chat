#include "ChatServer.h"

const char *_this_src_file = __FILE__;

//-------------------------------------------------------------------
void Runway_Aggregate(tDataRunway *runway, char *data, int len) {
   if (BUFF_SIZE_LIMIT == runway->length) return; // buffer is full
   if (BUFF_SIZE_LIMIT - runway->length < len) len = BUFF_SIZE_LIMIT - runway->length;
   memcpy(runway->data + runway->length, data, len);
   runway->length += len;
}
//-------------------------------------------------------------------
void Runway_Reset(tChatServer *ctx, tDataRunway *runway) {
   if (runway == NULL) return;

   if (runway->owner != -1) {
      ctx->channels[runway->owner].runway = NULL;
   }
   runway->length = 0;
   runway->owner = -1;
   runway->opcode = -1;
}
//-------------------------------------------------------------------
void DestroyRoom(tChatServer *ctx, tChatRoom *room) {
   tChatRoom *mark;

   if (room->name != NULL) {
      free(room->name);
      room->name = NULL;
   }

   mark = ctx->rooms;
   if (mark == room) {
      ctx->rooms = mark->next;
   } else {
      while (mark->next != room) {
         mark = mark->next;
      }
      mark->next = room->next;
   }

   free(room);
}
//-------------------------------------------------------------------
void RemoveUserFromRoom(tChatServer *ctx, int ch) {
   tChannel *channel = ctx->channels + ch;
   tChannel *mark = NULL;

   if (channel->room == NULL) return;

   mark = channel->room->users;
   if (mark == channel) {
      channel->room->users = channel->next;
   } else {
      while (mark->next != channel) {
         mark = mark->next;
      }
      mark->next = channel->next;
   }

   if (channel->room->users == NULL) {
      DestroyRoom(ctx, channel->room);
   };

   channel->next = NULL;
   channel->room = NULL;
}
//-------------------------------------------------------------------
void SetDescriptorFlags(int fd, int flag) {
   int tmp = fcntl(fd, F_SETFL, 0);
   tmp |= flag;
   fcntl(fd, F_SETFL, tmp);
}
//-------------------------------------------------------------------
int KillConnection(tChatServer *ctx, int ch) {
   Submit_UserLeftRoom(ctx, ch);
   printf("\nKilling channel: %d\n", ch);
   close(ctx->fds[ch].fd);
   ctx->fds[ch].fd = -1;
   Runway_Reset(ctx, ctx->channels[ch].runway);
   RemoveUserFromRoom(ctx, ch);

   return 0;
}
//-------------------------------------------------------------------
int Server_Init(tChatServer **ctx) {
   const char *_this_api_name = __FUNCTION__;
   tChatServer *tmp = NULL;
   struct addrinfo net_criteria;
   struct addrinfo *prospects;
   struct addrinfo *p;
   int rcode, opt, i;
   tDataRunway *runway;

   //................................................................
   int Fail(int ln, const char *api_name, const char *fmt, ...) {
      printf("error: %s, line %i, %s()\n   ", _this_src_file, ln, api_name);

      va_list args;
      va_start(args, fmt);
      vprintf(fmt, args);
      va_end(args);
      printf("\n");

      if (tmp != NULL) {
         if (tmp->server_socket != -1) close(tmp->server_socket);
         free(tmp);
      }

      return -1;
   }
   //................................................................
   tmp = (tChatServer *) malloc(sizeof(tChatServer));

   tmp->inbox_len = 0;
   tmp->inbox_ofs = 0;
   tmp->outbox_len = 0;
   tmp->exit_signal = 0;
   tmp->next_uid = 0x1000;
   tmp->server_socket = INVALID_SOCKET;
   tmp->rooms = NULL;

   tmp->runway = NULL;
   for (i = 0; i < MAX_RUNWAYS; i++) {
      runway = (tDataRunway *) malloc(sizeof(tDataRunway));
      runway->length = 0;
      runway->owner = -1;
      runway->opcode = -1;
      runway->next = tmp->runway;

      tmp->runway = runway;
   }

   for (i = 0; i < MAX_CONNECTIONS+1; i++) {
      tmp->fds[i].fd = INVALID_SOCKET;
      tmp->fds[i].events = POLLIN;
      tmp->fds[i].revents = 0;
   }
   tmp->FieldHandler = Process_HeaderField;

   memset(&net_criteria, 0, sizeof(net_criteria));
   net_criteria.ai_family = AF_UNSPEC;
   net_criteria.ai_socktype = SOCK_STREAM;
   net_criteria.ai_flags = AI_PASSIVE;

   rcode = getaddrinfo(NULL, "25021", &net_criteria, &prospects);
   if (rcode != 0) return Fail(__LINE__, _this_api_name, gai_strerror(rcode));

   for (p = prospects; p != NULL; p = p->ai_next) {

      tmp->server_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if (tmp->server_socket == -1) continue;

      opt = 1;
      setsockopt(tmp->server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

      rcode = bind(tmp->server_socket, p->ai_addr, p->ai_addrlen);
      if (rcode == -1) {
         close(tmp->server_socket);
         tmp->server_socket = -1;
         continue;
      }

      SetDescriptorFlags(tmp->server_socket, O_NONBLOCK);

      rcode = listen(tmp->server_socket, 128);
      if (rcode < 0) {
         close(tmp->server_socket);
         tmp->server_socket = -1;
         continue;
      }

      break;
   }

   freeaddrinfo(prospects);
   if (p == NULL) return Fail(__LINE__, _this_api_name, "unable to initialize server socket!");

   tmp->fds[MAX_CONNECTIONS].fd = tmp->server_socket;
   *ctx = tmp;

   return 0;
}
//-------------------------------------------------------------------
void InitChannel(tChatServer *ctx, int ch) {
   tChannel *channel = ctx->channels + ch;

   channel->trust_level = 0;
   channel->ch = ch;
   channel->uid = ctx->next_uid++;

   if (channel->room != NULL) {
      RemoveUserFromRoom(ctx, ch);
   }

   if (channel->nick != NULL) {
      free(channel->nick);
      channel->nick = NULL;
   }

   if (channel->runway != NULL) {
      channel->runway->owner = -1;
      channel->runway = NULL;
   }

   channel->next = NULL;
}
//-------------------------------------------------------------------
tChatRoom *FindRoom(tChatServer *ctx, char *data, int data_size) {
   tChatRoom *mark = ctx->rooms;

   while (mark != NULL) {
      if ( (data_size == strlen(mark->name)) && (memcmp(data, mark->name, data_size) == 0) ) return mark;
      mark = mark->next;
   }

   return mark;
}
//-------------------------------------------------------------------
int AvoidHeadCollision(tChatRoom *room, int next) {
   next = (next+1) % HIST_SIZE_LIMIT;
   if (next == room->hist_head) {
      room->hist_head = (room->hist_head + 2 + room->hist[room->hist_head]) %  HIST_SIZE_LIMIT;
      room->hist_count--;
   }
   return next;
}
//-------------------------------------------------------------------
void RecordMsgOnHist(tChatRoom *room, char *nick, tPacket *packet) {
   int data_size = packet->size - sizeof(tPacket);
   char *data = ((char *) packet) + sizeof(tPacket);
   int len;
   char buff[256];

   if (data_size <= 0) return;

   sprintf(buff, "%s: %.*s", nick, data_size, data);
   len = strlen(buff);

   room->hist[room->hist_tail] = len;
   room->hist_tail = AvoidHeadCollision(room, room->hist_tail);

   for (int i = 0; i < len; i++) {
      room->hist[room->hist_tail] = buff[i];
      room->hist_tail = AvoidHeadCollision(room, room->hist_tail);
   }
   room->hist_count++;
}
//-------------------------------------------------------------------
void ProcessPacket(tChatServer *ctx, int ch, tPacket *packet, char *data, int data_size) {

   switch (packet->cmd) {
      case _SET_USER_NICK:  // change user nick
         // changing nicks is not supported right now
         Submit_Error(ctx, ch, packet->uid, -1);
         break;
      case _SET_ROOM: {       // client sets chat room
         tChannel *channel = ctx->channels + ch;
         tChatRoom *room = NULL;

         // changing the room not supported right now
         if (channel->room != NULL) Submit_Error(ctx, ch, packet->uid, -1);

         room = FindRoom(ctx, data, data_size);

         if (room == NULL) { // if room does not exist create it
            room = (tChatRoom *) malloc(sizeof(tChatRoom));
            room->name = (char *) malloc(data_size+1);
            memcpy(room->name, data, data_size);
            room->name[data_size] = '\0';
            room->users = NULL;
            room->hist_head = 0;
            room->hist_tail = 0;
            room->hist_count = 0;

            room->next = ctx->rooms;
            ctx->rooms = room;
         }

         channel->room = room;

         channel->next = room->users;
         room->users = channel;

         Submit_Ok(ctx, ch, packet->uid);
         printf("user [%i]%s set room to %s\n", ch, channel->nick, channel->room->name);

         Submit_RoomRoster(ctx, ch);
         Submit_RoomHistory(ctx, ch);
         Submit_UserEnteredRoom(ctx, ch);
      } break;
      case _NEW_MSG: {        // client submits a new msg
         tChannel *channel = ctx->channels + ch;
         printf("Received msg from: %s\n%.*s\n", channel->nick, data_size, data);
         RecordMsgOnHist(ctx->channels[ch].room, channel->nick, packet);
         BroadCastMsg(ctx, ch, packet);
      } break;
      default:
         break;
   }
}
//-------------------------------------------------------------------
int ProcessData(tChatServer *ctx, int ch) {
   int rcode;
   tPacket *packet;
   char *data = NULL;
   int data_size = 0;

   if (ctx->channels[ch].trust_level > 0) {

      packet = (tPacket *) (ctx->inbox + ctx->inbox_ofs);
      ctx->inbox_ofs += sizeof(tPacket);

      if (memcmp(packet->frame, _FRAME_SIG_, 4) != 0) {
         printf("warning: invalid _FRAME_SIG_\n");
      }

      data = ctx->inbox + ctx->inbox_ofs;
      data_size = packet->size - sizeof(tPacket);
   }


   switch (ctx->channels[ch].trust_level) {
      case 0: // POSIX socket just connected; Data should consist of websocket client header
         printf("\nRequest Header\n\n%.*s\n", ctx->inbox_len, ctx->inbox);
         rcode = Parse_WebSocketClientHeader(ctx);
         if (rcode < 0) return KillConnection(ctx, ch);
         ctx->channels[ch].trust_level++;
         Submit_WebSocketServerHeader(ctx, ch);
         break;
      case 1: // WebSocket connection completed, server expects a magic string
         rcode = Parse_RequireToken(ctx, "magic string :)", 15);
         if (rcode < 0) return KillConnection(ctx, ch);
         ctx->channels[ch].trust_level++;
         printf("\nmagic string accepted\n");
         break;
      case 2: { // Server expects client to set the user nick
         int i;
         for (i = 0; i < MAX_CONNECTIONS; i++) {
            if (ctx->channels[i].nick == NULL) continue;
            int st_len = strlen(ctx->channels[i].nick);
            if ( (st_len == data_size + 1) && (memcmp(ctx->channels[i].nick, data, data_size) == 0) ) {
               printf("\nnick already in use\n");
               Submit_Error(ctx, ch, packet->uid, -1);
               break;
            }
         }
         if (i != MAX_CONNECTIONS) break;

         if (ctx->channels[ch].nick != NULL) free(ctx->channels[ch].nick);
         ctx->channels[ch].nick = (char *) malloc(data_size + 1);
         memcpy(ctx->channels[ch].nick, data, data_size);
         ctx->channels[ch].nick[data_size] = '\0';

         Submit_Ok(ctx, ch, packet->uid);
         ctx->channels[ch].trust_level++;
         printf("nick set to: %s\n", ctx->channels[ch].nick);
         } break;
      case 3: // minimum profile requirements are met, server here-in-after will take in commands and respond to them accordingly
         ProcessPacket(ctx, ch, packet, data, data_size);
         break;
      default:
         return KillConnection(ctx, ch);
   }
   return 0;
}
//-------------------------------------------------------------------
void AcceptNewConnection(tChatServer *ctx) {
   int              i;
   socklen_t        slen;
   struct sockaddr  client_specs;
   int              client_socket;

   // check for new incomming connection
   if ((ctx->fds[MAX_CONNECTIONS].revents & POLLIN) != 0) { 
      slen = sizeof(client_specs);
      client_socket = accept(ctx->server_socket, &client_specs, &slen);
      if (client_socket >= 0) {
         SetDescriptorFlags(client_socket, O_NONBLOCK);
         for (i = 0; i < MAX_CONNECTIONS; i++) {
            if (ctx->fds[i].fd == -1) {
               ctx->fds[i].fd = client_socket;
               InitChannel(ctx, i);
               printf("connected\n");
               break;
            };
         } 
         // TODO client is connected, get him setup
      }
   }
}
//-------------------------------------------------------------------
void ReadSocketData(tChatServer *ctx) {
   tDataRunway * runway;
   int i;
   int rcode;

   int fin;
   int opcode;
   int p_len;
   int mask_flag;
   int src;
   unsigned char mask[4];
   unsigned char *b = (unsigned char *) ctx->inbox;


   // retrieve info from socket connections
   for (i = 0; i < MAX_CONNECTIONS; i++) {
      if (ctx->fds[i].fd == -1) continue;
      int revents = ctx->fds[i].revents;
      if ((revents & POLLIN) != 0) {
         // socket ready to be read
         if (ctx->channels[i].trust_level == 0) {
            rcode = recv(ctx->fds[i].fd, ctx->inbox, BUFF_SIZE_LIMIT, 0);
            if (rcode <= 0) {
               if (rcode == 0) {
                  printf("error: connection appears to be closed\n");
               } else {
                  printf("error: %s\n", strerror(errno));
               }
               KillConnection(ctx, i);
               continue;
            }
            ctx->inbox_ofs = 0;
            ctx->inbox_len = rcode;
            ProcessData(ctx, i);
         } else { 
            rcode = recv(ctx->fds[i].fd, ctx->inbox, BUFF_SIZE_LIMIT, MSG_PEEK);
            if (rcode <= 0) {
               if (rcode == 0) {
                  printf("error: connection appears to be closed\n");
               } else {
                  printf("error: %s\n", strerror(errno));
               }
               KillConnection(ctx, i);
               continue;
            }
            if (rcode < 6) continue; // do we have enought to understand the frame

            src = 0;
            fin = (b[src] >> 7) & 1;
            opcode = b[src] & 0xF;
            src++;
            mask_flag = (b[src] >> 7) & 1;
            p_len = b[src] & 0x7F;
            src++;
            if (p_len == 126) {
               p_len = 0;
               p_len |= b[src++] << 8; 
               p_len |= b[src++];
            } else if (p_len == 127) {
               // this server doesn't support heavy frames
               KillConnection(ctx, i);
               continue;
               //p_len = 0;
               //p_len |= b[src++] << 24; 
               //p_len |= b[src++] << 16; 
               //p_len |= b[src++] << 8; 
               //p_len |= b[src++];
            }
            if (rcode < (src+4+p_len)) continue; // do we have a full frame
            rcode = recv(ctx->fds[i].fd, ctx->inbox, (src+4+p_len), 0); // just read the frame
            if (rcode <= 0) {
               if (rcode == 0) {
                  printf("error: connection appears to be closed\n");
               } else {
                  printf("error: %s\n", strerror(errno));
               }
               KillConnection(ctx, i);
               continue;
            }

            mask[0] = b[src++];
            mask[1] = b[src++];
            mask[2] = b[src++];
            mask[3] = b[src++];

            ctx->inbox_ofs = src;

            for (int n = 0; n < p_len; n++) {
               b[src++] ^= mask[n & 3];
            }

            switch (opcode) {
               case 8: // connection close 
                  printf("connection close\n");
                  Submit_CloseResponse(ctx, i);
                  KillConnection(ctx, i);
                  continue;
               case 9: // ping
                  printf("received a ping\n");
                  Submit_Pong(ctx, i, p_len, ctx->inbox + ctx->inbox_ofs);
                  continue;
               case 10: // pong
                  printf("received a pong\n");
//TODO: reset timout timer
                  continue;
            }


            if ((fin == 1) && (ctx->channels[i].runway == NULL)) { // we have a single isolated packet
               ctx->inbox_len = rcode;
               ProcessData(ctx, i);
            } else if ((fin == 0) && (ctx->channels[i].runway == NULL)) { // we have the 1st fragment
               if ((opcode == 1) || (opcode == 2)) { 
                  runway = ctx->runway;
                  while (runway != NULL) {
                     if (runway->owner == -1) break;
                     runway = runway->next;
                  }                 
                  if (runway == NULL) {
                     printf("line %i Warning: out of runways! Dropping frame.\n", __LINE__);
                  } else {
                     Runway_Aggregate(runway, ctx->inbox + ctx->inbox_ofs, p_len);
                     runway->owner = i;
                     runway->opcode = opcode;
                     ctx->channels[i].runway = runway;
                  }
               } else {
                  printf("line %i Warning: Invalid opcode! Dropping frame.\n", __LINE__);
               }
            } else if ((fin == 0) && (ctx->channels[i].runway != NULL)) { // wa have another fragment
               if (opcode != 0) {
                  printf("line %i Warning: Invalid opcode. Anomally! Dropping frame.\n", __LINE__);
               } else {
                  runway = ctx->channels[i].runway;
                  Runway_Aggregate(runway, ctx->inbox + ctx->inbox_ofs, p_len);
               }
            } else if ((fin == 1) && (ctx->channels[i].runway != NULL)) { // we have the last fragment
               if (opcode != 0) {
                  printf("line %i Warning: Invalid opcode. Anomally! Dropping frame.\n", __LINE__);
               } else {
                  runway = ctx->channels[i].runway;
                  Runway_Aggregate(runway, ctx->inbox + ctx->inbox_ofs, p_len);
                  memcpy(ctx->inbox, runway->data, runway->length);
                  ctx->inbox_len = runway->length;
                  ctx->inbox_ofs = 0;
                  Runway_Reset(ctx, runway);
                  ProcessData(ctx, i);
               }
            }


         }

      }
   }   
}
//-------------------------------------------------------------------
int Server_Loop(tChatServer *ctx) {
   const char *_this_api_name = __FUNCTION__;
   struct timespec  timeout;
   int              rcode;

   //................................................................
   int Fail(int ln, const char *fmt, ...) {
      printf("error: %s, line %i, %s()\n   ", _this_src_file, ln, _this_api_name);

      va_list args;
      va_start(args, fmt);
      vprintf(fmt, args);
      va_end(args);
      printf("\n");

      return -1;
   }
   //................................................................

   SetDescriptorFlags(0, O_NDELAY);

   timeout.tv_sec = 0;
   timeout.tv_nsec = 100000000;
   printf("\n--------------------------\nPress ENTER to stop server\n");
   while (1) {
      rcode = ppoll(ctx->fds, MAX_CONNECTIONS+1, &timeout, NULL);
      if (rcode < 0) return Fail(__LINE__, "ppoll() --- %s\n", strerror(errno));
 
      AcceptNewConnection(ctx);
      ReadSocketData(ctx);

      // give feedback and retrieve input
      printf(".");
      if (getchar() != EOF) return 0;

   } 
}
//-------------------------------------------------------------------
int Server_Exit(tChatServer *ctx) {
   for (int i = 0; i < MAX_CONNECTIONS+1; i++) {
      if (ctx->fds[i].fd >= 0) {
         close(ctx->fds[i].fd);
         ctx->fds[i].fd = INVALID_SOCKET;
      }
   }
}
//-------------------------------------------------------------------
int main() {
   const char *_this_api_name = __FUNCTION__;
   tChatServer *ctx = NULL;
   int rcode;

   rcode = Server_Init(&ctx);
   if (rcode < 0) return -1;

   rcode = Server_Loop(ctx);
   if (rcode < 0) return -1;

   rcode = Server_Exit(ctx);
   if (rcode < 0) return -1;

   return 0;
}
//-------------------------------------------------------------------

