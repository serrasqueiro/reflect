/* reflector.c -- TCP Reflect with logging

   (c)2020 Henrique Moreira

*/
/* fork-based version */

#include <signal.h>
#include <crypt.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <syslog.h> /* For logging */

/*#define DEBUG */
#include "debug.h"

#define PROGRAM_NAME "reflector"

/* Define (below) if you want to kill the client connection using Ctrl-C */
/* #define USE_TELNET_CTRL_C */

#define LISTENQUEUESIZE 5
#define MAXBUFFER 32768

#define PROXY_HOST_ADDR_STR "192.168.1.254"
#define SERVER_PORT 8088
#define PROXY_PORT 8080

typedef unsigned char t_uint8;
typedef unsigned int t_uint;
typedef unsigned long int t_ulong;

static char m_proxyHostAddr[512];
static int serverSocketId = -1;
static int serverPort = SERVER_PORT;
static int proxyPort = PROXY_PORT;

char buf_shown[4096];
const char* errorMsg="<BODY><H2>You are not allowed to use this \
service...</H2></BODY>\n\n";


void usage(void)
{
    printf("basic_reflect [[proxy-host [remote-proxy-port] [bind-port]]]\n\
\n\
Examples:\n\
	192.168.1.254 80 8088\n\
		-> redirects to port 80 of 192.168.1.254, binds on local port 8088\n\
");
    exit(0);
}


void faultHandler(int data)
{
    close(serverSocketId);
    fprintf(stderr, "Server bailing out\n");
    exit(0);
}

int writebuffer(int sock, char* buffer, int count)
{
   register const char *ptr = (const char *)buffer;
   register int bytesleft = count;

   do
   {
      register int rc;
      do
      {
         rc = write(sock, ptr, bytesleft);
      }
      while (rc < 0 && errno == EINTR);

      if (rc < 0)
         return -1;

      bytesleft -= rc;

      ptr += rc;
   }
   while (bytesleft);

   return count;
}

void manageConnection(int clientSocketId)
{
   dprint("new connectionManager for socket %d\n", clientSocketId);
   {
      int selectResult;
      int proxySocketId;
      int readByteCount;
      t_uint8 buffer[MAXBUFFER+1];
      struct sockaddr_in proxyAddressData;
      fd_set readfds;
      fd_set writefds;

      memset((char*)&proxyAddressData, 0, sizeof(proxyAddressData));
      proxyAddressData.sin_family = AF_INET;
      proxyAddressData.sin_addr.s_addr = inet_addr(m_proxyHostAddr);
      proxyAddressData.sin_port = htons(proxyPort);
      if ((proxySocketId = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      {
         perror("thread: can't open stream socket to proxy");
         close(clientSocketId);
         return;
      }

      dprint("proxy socket OK.\n");

      if (connect(proxySocketId, (struct sockaddr*) &proxyAddressData, sizeof(proxyAddressData)) < 0)
      {
         perror("thread: can't connect to proxy");
         close(clientSocketId);
         close(proxySocketId);
         return;
      }

      dprint("thread: connected to proxy\n");

      while (1)
      {
         FD_ZERO(&readfds);
         FD_ZERO(&writefds);
         FD_SET(clientSocketId, &readfds);
         FD_SET(proxySocketId, &readfds);
         FD_SET(clientSocketId, &writefds);
         FD_SET(proxySocketId, &writefds);

         selectResult = select(FD_SETSIZE, &readfds, &writefds, 0, 0);

         if (selectResult < 0)   // oops
         {
	    dprint("select failed\n");
	    break;
         }

         if (FD_ISSET(clientSocketId, &readfds))
         {
            dprint("reading from client, writing to proxy\n");
            readByteCount = read(clientSocketId, (char*)buffer, MAXBUFFER);
	    if (readByteCount == 5)
	    {
		sprintf(buf_shown, "0x%02x%02x%02x%02x%02x",
			buffer[0],
			buffer[1],
			buffer[2],
			buffer[3],
			buffer[4]);
		dprint("did read from client (%d): %s, writing to proxy\n",
		       readByteCount,
		       buf_shown);
	    }
	    else
	    {
		buf_shown[0] = 0;
		dprint("did read from client (%d), writing to proxy\n",
		       readByteCount);
	    }
#ifdef USE_TELNET_CTRL_C
	    if (strcmp(buf_shown, "0xfff4fffd06")==0)
	    {
	       /* Detected Ctrl-C on telnet */
	       break;
	    }
#endif
            if (readByteCount <= 0)
            {
               /*perror("thread: read error from client");*/
               break;
            }
            if (writebuffer(proxySocketId, (char*)buffer, readByteCount) != readByteCount)
            {
               perror("thread: write error to proxy");
               break;
            }
         }

         if (FD_ISSET(proxySocketId, &readfds))
         {
	    dprint("reading from proxy, writing to client\n");
            readByteCount = read(proxySocketId, buffer, MAXBUFFER);
            dprint("did read from proxy, writing to client (%d), writing to proxy\n", readByteCount);
            if (readByteCount <= 0)
            {
               /*perror("thread: read error from proxy");*/
               break;
            }

            if (writebuffer(clientSocketId, (char*)buffer, readByteCount) != readByteCount)
            {
               perror("thread: write error to client");
               break;
            }
         }
      }

      close(proxySocketId);
      close(clientSocketId);
   }

   dprint("connectionManager for socket %d ended\n", clientSocketId);
}


int check_client(struct sockaddr_in* sa, int log_mask)
{
   int result = 0;  /* 0 = unauthorized */
   char str[32];
   inet_ntop(AF_INET, &(sa->sin_addr), str, INET_ADDRSTRLEN);
   dprint("Accessed %s:%d from: %s (log_mask=%d)\n",
	  m_proxyHostAddr, proxyPort, str,
	  log_mask);
   result = 1;
   if (log_mask)
   {
       openlog(PROGRAM_NAME, LOG_PID, LOG_USER);
       syslog(LOG_INFO, "%s %s [%d] for %s:%d",
	      result ? "Accepted" : "Rejected",
	      str,
	      serverPort,
	      m_proxyHostAddr, proxyPort);
       closelog();
   }
   return result;
}


/* --------------------------------
   main function
   --------------------------------
*/
int main(int argc, char* argv[])
{
   const int log_mask = 1;
   struct sockaddr_in serverAddressData;

   strcpy(m_proxyHostAddr, PROXY_HOST_ADDR_STR);

   if (argc > 4)
   {
       usage();
   }
   if (argc > 1)
   {
       strcpy(m_proxyHostAddr, argv[1]);
       if (m_proxyHostAddr[0] <= '-')
       {
	   usage();
       }

       if (argv[2])
       {
	   proxyPort = atoi(argv[2]);
	   if (argv[3])
	   {
	       serverPort = atoi(argv[3]);
	   }
       }
   }
   if (serverPort <= 0 || serverPort > 65535)
   {
       printf("Invalid bind (server) port: %d ('%s')\n\n",
	      serverPort,
	      argv[3]);
       usage();
   }

   if ((serverSocketId = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
      perror("server: can't open stream socket");
      return 1;
   }

   memset((char*)&serverAddressData, 0, sizeof(serverAddressData));
   serverAddressData.sin_family = AF_INET;
   serverAddressData.sin_addr.s_addr = htonl(INADDR_ANY);
   serverAddressData.sin_port = htons(serverPort);

   dprint("serverPort=%d, proxy host: %s, proxyPort=%d\n",
	  serverPort,
	  m_proxyHostAddr,
	  proxyPort);
   {
      int serverSocketOptional = 1;
      setsockopt(serverSocketId,
		 SOL_SOCKET,
		 SO_REUSEADDR,
		 (char*)&serverSocketOptional, sizeof(int));
   }

   if (bind(serverSocketId, (struct sockaddr *)&serverAddressData, sizeof(serverAddressData)) < 0)
   {
      perror("server: can't bind local address");
      close(serverSocketId);
      return 1;
   }

   dprint("bind OK.\n");
   if (listen(serverSocketId, LISTENQUEUESIZE))
   {
      close(serverSocketId);
      return 1;
   }

   signal(SIGINT, faultHandler);
   signal(SIGQUIT, faultHandler);
   signal(SIGUSR1, faultHandler);
   signal(SIGALRM, faultHandler);
   signal(SIGSEGV, faultHandler);
   signal(SIGCLD, SIG_IGN);

#ifdef SETUP_SUICIDE
   setup_suicide();
#endif

   while (1)
   {
      struct sockaddr_in clientAddressData;
      int clientAddressDataLength = sizeof(clientAddressData);
      int clientSocketId = accept(serverSocketId,
				  (struct sockaddr*)&clientAddressData,
				  (t_uint*)&clientAddressDataLength);

      if (check_client(&clientAddressData, log_mask))
      {
      }
      else
      {
         write(clientSocketId, errorMsg, strlen(errorMsg));
         close(clientSocketId);
         continue;
      }

      if (clientSocketId < 0)
      {
         continue;
      }

      dprint("Creating child for socket %d\n", clientSocketId);
      {
         int childPid;

         if ((childPid = fork()) < 0)   // oops
         {
            continue;
         }

         if (childPid == 0) // child
         {
            close(serverSocketId);
            manageConnection(clientSocketId);
            return 0;
         }

         // parent
         close(clientSocketId);
      }
   }
}
