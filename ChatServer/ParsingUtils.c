#include "ParsingUtils.h"

//-------------------------------------------------------------------
void SubStr_Trim(tSubStr *item) {
   while (item->length > 0) { // get rid of leading whitespaces
      if (item->str[item->anchor] > 0x20) break;
      item->anchor++;
      item->length--;
   }

   while (item->length > 0) { // get rid of trailing whitespaces
      if (item->str[item->anchor+item->length-1] > 0x20) break;
      item->length--;
   }
}
//-------------------------------------------------------------------
int Parse_RequestURI(tChatServer *ctx) {
   // for now the only URI recognized is '/'
   return Parse_RequireToken(ctx, "/", 1);
}
//-------------------------------------------------------------------
int Parse_HeaderFieldName(tChatServer *ctx, tSubStr *name) {
   int mark = ctx->inbox_ofs;
   int length;
   //................................................................
   int Fail() {
      ctx->inbox_ofs = mark;
      return -1;
   }
   //................................................................
   while (ctx->inbox_len > ctx->inbox_ofs) {
      if ((ctx->inbox[ctx->inbox_ofs] >= 0x21) &&
          (ctx->inbox[ctx->inbox_ofs] <= 0x7F)) {
          length = (ctx->inbox_ofs - mark);
          if (ctx->inbox[ctx->inbox_ofs] == ':') {
             if (length == 0) return Fail();
             name->str = ctx->inbox;
             name->anchor = mark;
             name->length = length;
             ctx->inbox_ofs++;
             return length+1;
          }
          ctx->inbox_ofs++;
      } else return Fail();
   }
    return Fail();
}
//-------------------------------------------------------------------
int Parse_HeaderFieldValue(tChatServer *ctx, tSubStr *value) {
   int rcode;
   int mark = ctx->inbox_ofs;

   //................................................................
   int Fail() {
      ctx->inbox_ofs = mark;
      return -1;
   }
   //................................................................

   while (ctx->inbox_len > ctx->inbox_ofs) {
      rcode = Parse_RequireToken(ctx, "\x0d\x0a", 2);
      if (rcode == 2) {
         value->str = ctx->inbox;
         value->anchor = mark;
         value->length = (ctx->inbox_ofs - mark) - 2;
         return (ctx->inbox_ofs - mark);
      }
      ctx->inbox_ofs++;
   }

   return Fail();
}
//-------------------------------------------------------------------
int Parse_HeaderField(tChatServer *ctx){
   int rcode;
   tSubStr name;
   tSubStr value;

   int mark = ctx->inbox_ofs;

   //................................................................
   int Fail() {
      ctx->inbox_ofs = mark;
      return -1;
   }
   //................................................................

   rcode = Parse_HeaderFieldName(ctx, &name);
   if (rcode < 0) return Fail();

   rcode = Parse_HeaderFieldValue(ctx, &value);
   if (rcode < 0) return Fail();

   ctx->FieldHandler(ctx, &name, &value);

   return (ctx->inbox_ofs - mark);
}
//-------------------------------------------------------------------
int Parse_ClearSpaces(tChatServer *ctx) {
   int mark = ctx->inbox_ofs;
   while (ctx->inbox_len > ctx->inbox_ofs) {
      if (ctx->inbox[ctx->inbox_ofs] == 0x20) {
         ctx->inbox_ofs++;
      } else break;
   }
   return (ctx->inbox_ofs - mark);
}
//-------------------------------------------------------------------
int Parse_RequestLine(tChatServer *ctx) {
   // "GET"_(endpt)_"HTTP/1.1"
   int rcode;
   int mark = ctx->inbox_ofs;
   //................................................................
   int Fail() {
      ctx->inbox_ofs = mark;
      return -1;
   }
   //................................................................

   rcode = Parse_RequireToken(ctx, "GET", 3);
   if (rcode != 3) return Fail();

   rcode = Parse_ClearSpaces(ctx);
   if (rcode < 1) return Fail();
   
   rcode = Parse_RequestURI(ctx);
   if (rcode < 0) return Fail();

   rcode = Parse_ClearSpaces(ctx);
   if (rcode < 1) return Fail();

   rcode = Parse_RequireToken(ctx, "HTTP/1.1", 8);
   if (rcode != 8) return Fail();

   rcode = Parse_RequireToken(ctx, "\x0d\x0a", 2);
   if (rcode != 2) return Fail();

   return ctx->inbox_ofs - mark;

}
//-------------------------------------------------------------------
int Parse_WebSocketClientHeader(tChatServer *ctx) {
   int rcode = Parse_RequestLine(ctx);
   if (rcode < 0) return -1;

   do {
      rcode = Parse_RequireToken(ctx, "\x0d\x0a", 2);
      if (rcode == 2) return 1;
      rcode = Parse_HeaderField(ctx);
   } while(rcode >= 0);
   return -1;
}
//-------------------------------------------------------------------
int Parse_RequireToken(tChatServer *ctx, char *st, int len) {
   int rcode;
   int mark = ctx->inbox_ofs;
   //................................................................
   int Fail() {
      ctx->inbox_ofs = mark;
      return -1;
   }
   //................................................................

   if (ctx->inbox_len - ctx->inbox_ofs < len) return Fail();

   rcode = memcmp(ctx->inbox+ctx->inbox_ofs, st, len);
   if (rcode != 0) return Fail();
   ctx->inbox_ofs += len;
   return len;
}
//-------------------------------------------------------------------

