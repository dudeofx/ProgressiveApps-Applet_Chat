#ifndef __WebSocketUtils_H__
#define __WebSocketUtils_H__

void CreateSecWSAcceptKey(tChatServer *ctx, tSubStr *key);
void Process_HeaderField(tChatServer *ctx, tSubStr *name, tSubStr *value);
void Submit_WebSocketServerHeader(tChatServer *ctx, int ch);

void Submit_CloseRequest(tChatServer *ctx, int ch);
void Submit_CloseResponse(tChatServer *ctx, int ch);
void Submit_Ping(tChatServer *ctx, int ch);
void Submit_Pong(tChatServer *ctx, int ch, int size, void *data);
void Submit_Heartbeat(tChatServer *ctx, int ch);
void Submit_Error(tChatServer *ctx, int ch, int uid, int err_code);
void Submit_Ok(tChatServer *ctx, int ch, int uid);

void BroadCastMsg(tChatServer *ctx, int ch, tPacket *packet);
void Submit_RoomRoster(tChatServer *ctx, int ch);
void Submit_RoomHistory(tChatServer *ctx, int ch);
void Submit_UserEnteredRoom(tChatServer *ctx, int ch);
void Submit_UserLeftRoom(tChatServer *ctx, int ch);

#endif

