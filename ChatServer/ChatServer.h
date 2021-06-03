#ifndef __ChatServer_H__
#define __ChatServer_H__

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <openssl/sha.h>

#define MAX_CONNECTIONS       256
#define MAX_RUNWAYS           32

#define BUFF_SIZE_LIMIT       (64*1024)
#define HIST_SIZE_LIMIT       (8*1024)
#define INVALID_SOCKET         -1

typedef struct pollfd         tPollfd;
typedef struct _tChannel      tChannel;
typedef struct _tChatServer   tChatServer;
typedef struct _tDataRunway   tDataRunway;
typedef struct _tChatRoom     tChatRoom;

#include "tPacket.h"
#include "ParsingUtils.h"
#include "WebSocketUtils.h"

struct _tDataRunway {
   char data[BUFF_SIZE_LIMIT];
   int length;
   int owner;
   int opcode;
   tDataRunway *next;
};

struct _tChatRoom {
   char      *name;
   tChannel  *users;
   tChatRoom *next;
   char hist[HIST_SIZE_LIMIT];
   int hist_head;
   int hist_tail;
   int hist_count;
};

struct _tChannel {
   int            trust_level;
   int            uid;
   int            ch;
   char          *nick;
   tChatRoom     *room;
   tDataRunway   *runway;
   tChannel      *next;     // used to chain users within a room. to go thru all users go thru the channel array.
};

struct _tChatServer {
   int           next_uid;
   int           exit_signal;
   int           server_socket;
   tPollfd       fds[MAX_CONNECTIONS+1];
   tChannel      channels[MAX_CONNECTIONS];
   tDataRunway  *runway;
   tChatRoom    *rooms;
   char          inbox[BUFF_SIZE_LIMIT];
   int           inbox_len;
   int           inbox_ofs;
   char          outbox[BUFF_SIZE_LIMIT];
   int           outbox_len;
   char          accept_key[32];
   void        (*FieldHandler)(tChatServer *, tSubStr *, tSubStr *);
};

#endif

