
#ifndef COMMON_H_
#define COMMON_H_

#include <base64.h>
#include <auth.h>
#include <config.h>
#include <mqtt_packet.h>
#include <list.h>
#include <set.h>

#include <client.h>
#include <client_tls.h>
#include <client_ws.h>
#include <keep_alive.h>
#include <server.h>
#include <worker.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <netdb.h>
#include <time.h>
#include <stdlib.h>
#ifdef HAVE_KQUEUE
#include <sys/event.h>
#endif
#ifdef HAVE_EPOLL
#include <sys/epoll.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <string.h>

#ifndef MAX
#define MAX(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
#endif
#ifndef MIN
#define MIN(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })
#endif

#endif  /* COMMON_H_ */
