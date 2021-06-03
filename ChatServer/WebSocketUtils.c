#include "ChatServer.h"

const char *ws_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

//-------------------------------------------------------------------
void CreateSecWSAcceptKey(tChatServer *ctx, tSubStr *key) {
   char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
   char ibuf[64];
   char obuf[20];
   char *msg = ctx->accept_key;
   int bytes_left;
   int src, dst;
   int code;

   sprintf(ibuf, "%.*s%s", key->length, key->str+key->anchor, ws_GUID);
   SHA1(ibuf, strlen(ibuf), obuf);

   bytes_left = 20;
   dst = 0;
   src = 0;
   while (bytes_left > 0) {
      if (bytes_left >= 3) {
         code = obuf[src+2] | (obuf[src+1] << 8) | (obuf[src] << 16);
         msg[dst++] = table[(code >> 18) & 0x3F];
         msg[dst++] = table[(code >> 12) & 0x3F];
         msg[dst++] = table[(code >> 6) & 0x3F];
         msg[dst++] = table[code & 0x3F];
         src += 3;
         bytes_left -= 3;
      } else if (bytes_left == 2) {
         code = (obuf[src+1] << 8) | (obuf[src] << 16);
         msg[dst++] = table[(code >> 18) & 0x3F];
         msg[dst++] = table[(code >> 12) & 0x3F];
         msg[dst++] = table[(code >> 6) & 0x3C];
         msg[dst++] = '=';
         bytes_left -= 2;
      } else { // bytes_left == 1
         code = (obuf[src] << 16);
         msg[dst++] = table[(code >> 18) & 0x3F];
         msg[dst++] = table[(code >> 12) & 0x30];
         msg[dst++] = '=';
         msg[dst++] = '=';
         bytes_left -= 1;
      }
   }
   msg[dst] = '\0';
}
//-------------------------------------------------------------------
void Process_HeaderField(tChatServer *ctx, tSubStr *name, tSubStr *value) {
   // for now all we care about is the key
   if (memcmp("Sec-WebSocket-Key", name->str+name->anchor, 17) == 0) {
      SubStr_Trim(value);
      CreateSecWSAcceptKey(ctx, value);
   }
}
//-------------------------------------------------------------------
void Submit_WebSocketServerHeader(tChatServer *ctx, int ch) {
   char buff[256];
   int rcode;

   sprintf(buff,
           "HTTP/1.1 101 Switching Protocols\x0d\x0a"
           "Upgrade: websocket\x0d\x0a"
           "Connection: Upgrade\x0d\x0a"
//           "sec-Websocket-version: 13\x0d\x0a"
           "Sec-WebSocket-Accept: %s\x0d\x0a\x0d\x0a", ctx->accept_key);
   rcode = send(ctx->fds[ch].fd, buff, strlen(buff), 0);
   printf("Response Header\n\n%s",  buff);
}
//-------------------------------------------------------------------
void Submit_CloseRequest(tChatServer *ctx, int ch) {
}
//-------------------------------------------------------------------
void Submit_CloseResponse(tChatServer *ctx, int ch) {
   //TODO: for the time being I am just echoing the close but I think I need to remove the mask or remask the data
   int rcode;
   rcode = send(ctx->fds[ch].fd, ctx->inbox, ctx->inbox_len, 0);
}
//-------------------------------------------------------------------
void Submit_Ping(tChatServer *ctx, int ch) {
   char *ping = "\x89\x12yippy-ka-yo-ka-yay";
   int rcode;

   rcode = send(ctx->fds[ch].fd, ping, 20, 0);

}
//-------------------------------------------------------------------
void Submit_Pong(tChatServer *ctx, int ch, int size, void *data) {
   // TODO: pong data is limited to 125 bytes, that might need fixin
   char pong[128];
   int rcode;

   if (size > 125) {
      printf("warning: truncating pong data: %i bytes is too big\n", size);
      size = 125;
   }

   pong[0] = 0x8A;
   pong[1] = size;

   memcpy(pong + 2, data, size);
   rcode = send(ctx->fds[ch].fd, pong, 10, 0);
}
//-------------------------------------------------------------------
static int chat_server_heart_beat = 0;
void Submit_Heartbeat(tChatServer *ctx, int ch) {
   char *pong = "\x8A\x04----";
   int rcode;

   memcpy(pong + 2, &chat_server_heart_beat, 4);
   rcode = send(ctx->fds[ch].fd, pong, 6, 0);
}
//-------------------------------------------------------------------
void Submit_Error(tChatServer *ctx, int ch, int uid, int err_code) {
   int rcode;
   char buff[26];
   tPacket packet;

   buff[0] = 0x82;
   buff[1] = sizeof(tPacket);

   memcpy(packet.frame, _FRAME_SIG_, 4);
   packet.uid = uid;
   packet.size = sizeof(tPacket);
   packet.cmd = _RESP_ERROR;
   packet.p1 = err_code;
   packet.p2 = 0;

   memcpy(buff + 2, &packet, sizeof(tPacket));
   rcode = send(ctx->fds[ch].fd, &buff, sizeof(tPacket) + 2, 0);
}
//-------------------------------------------------------------------
void Submit_Ok(tChatServer *ctx, int ch, int uid) {
   int rcode;
   char buff[26];
   tPacket packet;

   buff[0] = 0x82;
   buff[1] = sizeof(tPacket);

   memcpy(packet.frame, _FRAME_SIG_, 4);
   packet.uid = uid;
   packet.size = sizeof(tPacket);
   packet.cmd = _RESP_OK;
   packet.p1 = 0;
   packet.p2 = 0;

   memcpy(buff + 2, &packet, sizeof(tPacket));
   rcode = send(ctx->fds[ch].fd, &buff, sizeof(tPacket) + 2, 0);
}
//-------------------------------------------------------------------
void BroadCastMsg(tChatServer *ctx, int ch, tPacket *packet_in) {
   int rcode;
   int dst;
   tChannel *mark;
   tPacket *packet_out;
   char *data;
   int data_size;

   data_size = packet_in->size - sizeof(tPacket);
   data = ((char *) packet_in) + sizeof(tPacket);

   dst = 0;
   ctx->outbox[dst++] = 0x82;
   if (packet_in->size <= 125) {
      ctx->outbox[dst++] = packet_in->size;
   } else {
      rcode = packet_in->size;
      ctx->outbox[dst++] = 0x7E;
      ctx->outbox[dst++] = (rcode & 0x0000FF00) >> 8;
      ctx->outbox[dst++] = (rcode & 0x000000FF);
   }

   packet_out = (tPacket *) (ctx->outbox + dst);
   memcpy(packet_out->frame, _FRAME_SIG_, 4);
   packet_out->uid = -1;
   packet_out->size = packet_in->size;
   packet_out->cmd = _NEW_MSG;
   packet_out->p1 = ctx->channels[ch].uid;
   packet_out->p2 = 0;
   dst += sizeof(tPacket);

   memcpy(ctx->outbox + dst, data, data_size);
   dst += data_size;

   ctx->outbox_len = dst;

   mark = ctx->channels[ch].room->users;
   while (mark != NULL) {
      rcode = send(ctx->fds[mark->ch].fd, ctx->outbox, ctx->outbox_len, 0);
      mark = mark->next;
   }




}
//-------------------------------------------------------------------
void Submit_RoomRoster(tChatServer *ctx, int ch) {
   tChannel *mark;
   int rcode;
   int packet_size;
   int dst;
   tPacket *packet_out;

   mark = ctx->channels[ch].room->users;
   while (mark != NULL) {
      packet_size = sizeof(tPacket) + strlen(mark->nick);

      dst = 0;
      ctx->outbox[dst++] = 0x82;
      if (packet_size <= 125) {
         ctx->outbox[dst++] = packet_size;
      } else {
         rcode = packet_size;
         ctx->outbox[dst++] = 0x7E;
         ctx->outbox[dst++] = (rcode & 0x0000FF00) >> 8;
         ctx->outbox[dst++] = (rcode & 0x000000FF);
      }

      packet_out = (tPacket *) (ctx->outbox + dst);
      memcpy(packet_out->frame, _FRAME_SIG_, 4);
      packet_out->uid = -1;
      packet_out->size = packet_size;
      packet_out->cmd = _NEW_USER;
      packet_out->p1 = mark->uid;
      packet_out->p2 = 0;
      dst += sizeof(tPacket);

      memcpy(ctx->outbox + dst, mark->nick, strlen(mark->nick));
      dst += strlen(mark->nick);

      ctx->outbox_len = dst;

      rcode = send(ctx->fds[ch].fd, ctx->outbox, ctx->outbox_len, 0);
      mark = mark->next;
   }
}
//-------------------------------------------------------------------
void Submit_RoomHistory(tChatServer *ctx, int ch) {
   tChatRoom *room = ctx->channels[ch].room;
   int head = room->hist_head;
   int rcode;
   int len;
   int dst;
   tPacket *packet_out;

   for (int i = 0; i < room->hist_count; i++) {
      len = room->hist[head];
      head = (head+1) % HIST_SIZE_LIMIT;

      dst = 0;
      ctx->outbox[dst++] = 0x82;
      if (len <= 125) {
         ctx->outbox[dst++] = len + sizeof(tPacket);
      } else {
         rcode = len;
         ctx->outbox[dst++] = 0x7E;
         ctx->outbox[dst++] = (rcode & 0x0000FF00) >> 8;
         ctx->outbox[dst++] = (rcode & 0x000000FF);
      }
      
      packet_out = (tPacket *) (ctx->outbox + dst);
      memcpy(packet_out->frame, _FRAME_SIG_, 4);
      packet_out->uid = -1;
      packet_out->size = sizeof(tPacket) + len;
      packet_out->cmd = _LOGGED_MSG;
      packet_out->p1 = 0;
      packet_out->p2 = 0;
      dst += sizeof(tPacket);

      for (int j = 0; j < len; j++) {
         ctx->outbox[dst++] = room->hist[head];
         head = (head+1) % HIST_SIZE_LIMIT;
      }
      ctx->outbox_len = dst;

      rcode = send(ctx->fds[ch].fd, ctx->outbox, ctx->outbox_len, 0);
   }
}
//-------------------------------------------------------------------
void Submit_UserEnteredRoom(tChatServer *ctx, int ch) {
   tChatRoom *room = ctx->channels[ch].room;
   tChannel *mark = room->users;
   char *nick = ctx->channels[ch].nick;
   int dst;
   int len;
   int rcode;
   tPacket *packet_out;

   len = strlen(nick);
   dst = 0;
   ctx->outbox[dst++] = 0x82;
   if (len <= 125) {
      ctx->outbox[dst++] = len + sizeof(tPacket);
   } else {
      rcode = len;
      ctx->outbox[dst++] = 0x7E;
      ctx->outbox[dst++] = (rcode & 0x0000FF00) >> 8;
      ctx->outbox[dst++] = (rcode & 0x000000FF);
   }
      
   packet_out = (tPacket *) (ctx->outbox + dst);
   memcpy(packet_out->frame, _FRAME_SIG_, 4);
   packet_out->uid = -1;
   packet_out->size = sizeof(tPacket) + len;
   packet_out->cmd = _NEW_USER;
   packet_out->p1 = ctx->channels[ch].uid;
   packet_out->p2 = 0;
   dst += sizeof(tPacket);

   memcpy(ctx->outbox + dst, nick, len);
   dst += len;

   ctx->outbox_len = dst;

   while (mark != NULL) {
      if (mark->ch != ch) {
         rcode = send(ctx->fds[mark->ch].fd, ctx->outbox, ctx->outbox_len, 0);
      }
      mark = mark->next;
   }
}
//-------------------------------------------------------------------
void Submit_UserLeftRoom(tChatServer *ctx, int ch) {
   tChatRoom *room = ctx->channels[ch].room;
   tChannel *mark = room->users;
   char *nick = ctx->channels[ch].nick;
   int dst;
   int len;
   int rcode;
   tPacket *packet_out;

   len = strlen(nick);
   dst = 0;
   ctx->outbox[dst++] = 0x82;
   if (len <= 125) {
      ctx->outbox[dst++] = len + sizeof(tPacket);
   } else {
      rcode = len;
      ctx->outbox[dst++] = 0x7E;
      ctx->outbox[dst++] = (rcode & 0x0000FF00) >> 8;
      ctx->outbox[dst++] = (rcode & 0x000000FF);
   }
      
   packet_out = (tPacket *) (ctx->outbox + dst);
   memcpy(packet_out->frame, _FRAME_SIG_, 4);
   packet_out->uid = -1;
   packet_out->size = sizeof(tPacket) + len;
   packet_out->cmd = _DEL_USER;
   packet_out->p1 = ctx->channels[ch].uid;
   packet_out->p2 = 0;
   dst += sizeof(tPacket);

   memcpy(ctx->outbox + dst, nick, len);
   dst += len;

   ctx->outbox_len = dst;

   while (mark != NULL) {
      if (mark->ch != ch) {
         rcode = send(ctx->fds[mark->ch].fd, ctx->outbox, ctx->outbox_len, 0);
      }
      mark = mark->next;
   }
}
//-------------------------------------------------------------------

