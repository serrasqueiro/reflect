/* basic_reflect.c -- Basic Reflect for HTTP

   (c)2000 Henrique Moreira

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

/*#define DEBUG */

#define LISTENQUEUESIZE 5

#define PROXY_HOST_ADDR "192.168.1.254"
#define SERVER_TCP_PORT 8088
#define PROXY_TCP_PORT 8080

#define MAXBUFFER 32768

typedef unsigned int t_int;
typedef unsigned long int t_long;

t_long allowedClient[]=
{
   3029217081UL, // me
   3029225250UL, // hm
   0
};

static char m_proxyHostAddr[512];

const char* errorMsg="<BODY><H2>You are not allowed to use this \
service...</H2></BODY>\n\n";

int serverSocketId;


void usage(void)
{
    printf("basic_reflect proxy-host [port]\n");
    exit(0);
}


void faultHandler(int data)
{
    close(serverSocketId);

#ifdef DEBUG
    printf("Server bailing out\n");
#endif

    exit(0);
}

int writebuffer(int sock, char* buffer, int count)
{
   register const char *ptr = (const char *) buffer;
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
#ifdef DEBUG
   printf("new connectionManager for socket %d\n", clientSocketId);
#endif

   {
      int proxySocketId;
      int readByteCount;
      char buffer[MAXBUFFER+1];
      struct sockaddr_in proxyAddressData;
      fd_set readfds;
      fd_set writefds;
      int selectResult;

      memset((char*)&proxyAddressData, 0, sizeof(proxyAddressData));
      proxyAddressData.sin_family = AF_INET;
      proxyAddressData.sin_addr.s_addr = inet_addr(PROXY_HOST_ADDR);
      /* proxyAddressData.sin_addr.s_addr = 3263864451UL; */
      proxyAddressData.sin_port = htons(PROXY_TCP_PORT);
      if ((proxySocketId = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      {
         perror("thread: can't open stream socket to proxy");
         close(clientSocketId);
         return;
      }

#ifdef DEBUG
      printf("proxy socket OK.\n");
#endif

      if (connect(proxySocketId, (struct sockaddr*) &proxyAddressData, 
sizeof(proxyAddressData)) < 0)
      {
         perror("thread: can't connect to proxy");
         close(clientSocketId);
         close(proxySocketId);
         return;
      }

#ifdef DEBUG
      printf("thread: connected to proxy\n");
#endif

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
            break;
         }

         if (FD_ISSET(clientSocketId, &readfds))
         {
#ifdef DEBUG
            printf("reading from client, writing to proxy\n");
#endif

            readByteCount = read(clientSocketId, buffer, MAXBUFFER);
            if (readByteCount <= 0)
            {
               /*perror("thread: read error from client");*/
               break;
            }

            if (writebuffer(proxySocketId, buffer, readByteCount) != 
readByteCount)
            {
               perror("thread: write error to proxy");
               break;
            }
         }

         if (FD_ISSET(proxySocketId, &readfds))
         {
#ifdef DEBUG
            printf("reading from proxy, writing to client\n");
#endif

            readByteCount = read(proxySocketId, buffer, MAXBUFFER);
            if (readByteCount <= 0)
            {
               /*perror("thread: read error from proxy");*/
               break;
            }

            if (writebuffer(clientSocketId, buffer, readByteCount) != 
readByteCount)
            {
               perror("thread: write error to client");
               break;
            }
         }
      }

      close(proxySocketId);
      close(clientSocketId);
   }

#ifdef DEBUG
   printf("connectionManager for socket %d ended\n", clientSocketId);
#endif
}

int check_client(unsigned char b1, unsigned char b2, unsigned char b3, 
unsigned char b4)
{
   int i;
   t_long clientAddr = b1<<24|b2<<16|b3<<8|b4;
   int lastidx = sizeof(allowedClient)/sizeof(t_long);

   for(i=0; i<lastidx; i++)
   {
      if (clientAddr == allowedClient[i])
         return 1;
   }

   return 0;
}

void setup_suicide()
{
   int timeToDie;
   int mult;
   char* aString = getpass("Suicide timeout (Xs-secs, Xm-mins, Xh-hours): \n\
");

   switch(aString[strlen(aString)-1])
   {
      case 's':
         mult = 1;
         break;
      case 'm':
         mult = 60;
         break;
      case 'h':
         mult = 3600;
         break;
      default:
         mult = 0;
         break;
   }

   aString[strlen(aString)-1] = 0;
   timeToDie = atoi(aString) * mult;
   printf("Dying in %d seconds...\n", timeToDie);
   alarm(timeToDie);
}

/* main function */
int main(int argc, char* argv[])
{
   struct sockaddr_in serverAddressData;
   int serverPort = SERVER_TCP_PORT;

   strcpy(m_proxyHostAddr, PROXY_HOST_ADDR);

   if (argc > 1)
   {
       if (argc > 3)
       {
	   usage();
       }

       strcpy(m_proxyHostAddr, argv[1]);
       
       if (argv[2])
       {
	   serverPort = atoi(argv[2]);
	   if (serverPort <= 0 || serverPort > 65535)
	   {
	       printf("Invalid server port: %d ('%s')\n\n",
		      serverPort,
		      argv[2]);
	       usage();
	   }
       }
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

   {
      int serverSocketOptional = 1;
      setsockopt(serverSocketId, SOL_SOCKET, SO_REUSEADDR, 
(char*)&serverSocketOptional, sizeof(int));
   }

   if (bind(serverSocketId, (struct sockaddr *)&serverAddressData, 
sizeof(serverAddressData)) < 0)
   {
      perror("server: can't bind local address");
      close(serverSocketId);
      return 1;
   }

#ifdef DEBUG
   printf("bind OK.\n");
#endif

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
				  (t_int*)&clientAddressDataLength);

#ifdef CHECK_CLIENT
#ifdef DEBUG
      printf("accept OK.(%d.%d.%d.%d)\n",
               clientAddressData.sin_addr.S_un.S_un_b.s_b1,
               clientAddressData.sin_addr.S_un.S_un_b.s_b2,
               clientAddressData.sin_addr.S_un.S_un_b.s_b3,
               clientAddressData.sin_addr.S_un.S_un_b.s_b4);
#endif

      if (!check_client(clientAddressData.sin_addr.S_un.S_un_b.s_b1,
			clientAddressData.sin_addr.S_un.S_un_b.s_b2,
			clientAddressData.sin_addr.S_un.S_un_b.s_b3,
			clientAddressData.sin_addr.S_un.S_un_b.s_b4))
      {
         write(clientSocketId, errorMsg, strlen(errorMsg));
         close(clientSocketId);
         continue;
      }
#endif /* CHECK_CLIENT */

      if (clientSocketId < 0) // oops
      {
         continue;
      }

#ifdef DEBUG
         printf("Creating child for socket %d\n", clientSocketId);
#endif

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

