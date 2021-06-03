#ifndef __ParsingUtils_H__
#define __ParsingUtils_H__

typedef struct _tSubStr tSubStr;

#include "ChatServer.h"

struct _tSubStr {
   char *str;
   int anchor;
   int length;
};

void SubStr_Trim(tSubStr *item);
int Parse_RequestURI(tChatServer *ctx);
int Parse_HeaderFieldName(tChatServer *ctx, tSubStr *name);
int Parse_HeaderFieldValue(tChatServer *ctx, tSubStr *value);
int Parse_HeaderField(tChatServer *ctx);
int Parse_ClearSpaces(tChatServer *ctx);
int Parse_RequestLine(tChatServer *ctx);
int Parse_WebSocketClientHeader(tChatServer *ctx);
int Parse_RequireToken(tChatServer *ctx, char *st, int len);

#endif

