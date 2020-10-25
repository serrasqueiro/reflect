/* reflector.c -- TCP Reflect with logging

   (c)2020 Henrique Moreira

*/
/* fork-based version */

#include <signal.h>
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
#include "reflog.h"

#define PROGRAM_NAME "reflector"

/* Define (below) if you want to kill the client connection using Ctrl-C */
/* #define USE_TELNET_CTRL_C */

#define LISTENQUEUESIZE 5
#define MAXBUFFER 32768

#define PROXY_HOST_ADDR_STR "192.168.1.254"
#define SERVER_PORT 8088
#define PROXY_PORT 8080

#define LOG_A(args...) REFLOG_USER(PROGRAM_NAME, LOG_INFO, args)

#define SWRITE(args...) custom_write(__LINE__, args, 0)
#define SREAD(args...) custom_read(__LINE__, args, 0)


static char m_proxyHostAddr[512];
static int serverSocketId = -1;
static int serverPort = SERVER_PORT;
static int proxyPort = PROXY_PORT;
static t_stats_net netStats;

char buf_shown[4096];
const char* errorMsg="<BODY><H2>You are not allowed to use this \
service...</H2></BODY>\n\n";


void usage(void)
{
    printf("basic_reflect [[proxy-host [remote-proxy-port] [bind-port]]]\n\
\n\
Examples:\n\
    192.168.1.254 80 8088\n\
       -> redirects to port 80 of 192.168.1.254, binds on local port 8088\n \
");
    exit(0);
}


ssize_t custom_write(int line, int sockfd, const void *buf, size_t len, int flags)
{
   ssize_t bytes;
   dprint("About to send %u byte(s)\n", (t_uint)len);
   bytes = send(sockfd, buf, len, flags);
   dprint("Wrote (%d) %u byte(s)\n", sockfd, (t_uint)len);
   return bytes;
}

ssize_t custom_read(int line, int sockfd, void *buf, size_t len, int flags)
{
   ssize_t bytes;
   dprint("About to read %u byte(s)\n", (t_uint)len);
   bytes = recv(sockfd, buf, len, flags);
   dprint("Read (%d) %u byte(s)\n", sockfd, (t_uint)len);
   return bytes;
}


void faultHandler(int data)
{
   close(serverSocketId);
   fprintf(stderr, "Server [%d]-%s:%d bailing out\n",
           serverPort,
           m_proxyHostAddr,
           proxyPort);
   LOG_A("Bye-bye [%d]-%s:%d: accs/rejs.=%d/%d\n",
         serverPort,
         m_proxyHostAddr,
         proxyPort,
         netStats.accs, netStats.rejs);
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
         rc = SWRITE(sock, ptr, bytesleft);
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
   const int nfds = FD_SETSIZE;
   struct timeval timeVal;
   timeVal.tv_sec = 5;  // 5 secs.
   timeVal.tv_usec = 0;

   dprint("new connectionManager for socket %d\n", clientSocketId);
   {
      int selectResult;
      int proxySocketId;
      int readByteCount;
      t_uint8 buffer[MAXBUFFER+1];
      struct sockaddr_in proxyAddressData;
      fd_set readfds;
      fd_set rx_base;
      fd_set writefds;
      fd_set tx_base;

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
         LOG_A("Can't connect to proxy %s: %s\n",
               m_proxyHostAddr,
               strerror(errno));
         perror("thread: can't connect to proxy");
         close(clientSocketId);
         close(proxySocketId);
         return;
      }

      dprint("thread: connected to proxy\n");

      FD_ZERO(&rx_base);
      FD_ZERO(&writefds);
      FD_SET(clientSocketId, &rx_base);
      FD_SET(proxySocketId, &rx_base);
      FD_SET(clientSocketId, &writefds);
      FD_SET(proxySocketId, &writefds);

      while (1)
      {
         memcpy(&readfds, &rx_base, sizeof(readfds));
         memcpy(&writefds, &tx_base, sizeof(writefds));
         selectResult = select(nfds, &readfds, &writefds, 0, &timeVal);
         if (selectResult < 0)
         {
            /* Oops... select failed */
            dprint("select timed-out\n");
            break;
         }
         netStats.sels += 1;

         if (FD_ISSET(clientSocketId, &writefds))
         {
            dprint("write to client\n");
	    break;
         }

         if (FD_ISSET(proxySocketId, &writefds))
         {
            dprint("write to proxy\n");
	    break;
         }

         if (FD_ISSET(clientSocketId, &readfds))
         {
            dprint("reading from client, writing to proxy\n");
            //FD_CLR(clientSocketId, &rx_fds);
            readByteCount = SREAD(clientSocketId, (char*)buffer, MAXBUFFER);
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
               perror("thread: read error from client");
               break;
            }
            netStats.byteCountIngress += readByteCount;
            if (writebuffer(proxySocketId, (char*)buffer, readByteCount) != readByteCount)
            {
               perror("thread: write error to proxy");
               break;
            }
         }

         if (FD_ISSET(proxySocketId, &readfds))
         {
            dprint("reading from proxy, writing to client\n");
            //FD_CLR(proxySocketId, &rx_fds);
            readByteCount = SREAD(proxySocketId, buffer, MAXBUFFER);
            netStats.byteCountEgress += readByteCount;
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
   LOG_A("Ends fd=%d (sels=%d) %ld/ %ld bytes",
         clientSocketId,
         netStats.sels,
         netStats.byteCountIngress,
         netStats.byteCountEgress);
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
       LOG_A("%s %s [%d] for %s:%d",
             result ? "Accepted" : "Rejected",
             str,
             serverPort,
             m_proxyHostAddr, proxyPort);
   }
   return result;
}


int convert_to_ip(char* sHost, t_uint size)
{
   struct hostent *hostEntry;
   char* ipBuf;

   if (sHost==NULL || strcmp(sHost, ".")==0)
   {
       strncpy(sHost, "localhost", size);
       return 1;
   }

   hostEntry = gethostbyname(sHost);
   if (hostEntry == NULL)
   {
       return -1;
   }
   ipBuf = inet_ntoa(*((struct in_addr*)hostEntry->h_addr_list[0]));
   if (ipBuf && ipBuf[0])
   {
       dprint("Using IP from host %s: %s\n", sHost, ipBuf);
       snprintf(sHost, size, "%s", ipBuf);
       return 0;
   }
   return 1;
}


/* --------------------------------
   main function
   --------------------------------
*/
int main(int argc, char* argv[])
{
   const int log_mask = 1;
   int code = -1;
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

       code = convert_to_ip(m_proxyHostAddr, sizeof(m_proxyHostAddr));
       dprint("convert_to_ip(%s): code=%d\n", m_proxyHostAddr, code);
       if (code)
       {
           if (code < 0)
           {
               fprintf(stderr, "Could not use '%s'\n", m_proxyHostAddr);
               return 2;
           }
           fprintf(stderr, "Could not use '%s', assuming %s\n", argv[1], m_proxyHostAddr);
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
         netStats.accs++;
      }
      else
      {
         netStats.rejs++;
         SWRITE(clientSocketId, errorMsg, strlen(errorMsg));
         close(clientSocketId);
         continue;
      }

      if (clientSocketId < 0)
      {
         continue;
      }

      netStats.id += 1;
      dprint("Creating child for socket %d (id=%d)\n",
	     clientSocketId,
	     netStats.id);
      {
         int childPid;

         if ((childPid = fork()) < 0)
         {
            /* Something got wrong */
            continue;
         }

         if (childPid == 0)
         {
            /* Child part */
            char* prg = (char*)argv[0];
            char* fnd = strrchr(prg, '/');
            char what[3];
            strcpy(what, ".A");
            what[1] = 'A' - 1 + (netStats.id % 26);
            if (fnd)
            {
                strcpy(fnd+strlen(fnd)-2, what);
                dprint("Forked to: %s\n", fnd);
	    }
            close(serverSocketId);
            manageConnection(clientSocketId);
            return 0;
         }

         /* Here at the parent */
         close(clientSocketId);
      }
   }
}
