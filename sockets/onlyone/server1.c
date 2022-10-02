/*************************************************************************
SERVER1: 	Servidor de ficheiros e de tempo (WIN32) - version 0.1
/*************************************************************************/

#define DOS_SPEC

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef DOS_SPEC
#include <winsock.h>
#include <io.h>
#include <fcntl.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif DOS_SPEC

void ErroFatal (char *mensagem)
{
 fprintf( stderr, "ERRO DE SERVIDOR: %s\n", mensagem );
 exit(1);
}// Erro Fatal

void show_HostEnt (struct hostent *hp)
{
 printf("Hostname:%s, AddressType:%d, Length=%d\n",
	hp->h_name,
	hp->h_addrtype,
	hp->h_length );
}

void show_SockAddrIn (struct sockaddr_in *sock)
{
 printf("Family:%d, Port:%d, HostIdIP:%ld\n",
	sock->sin_family,
	sock->sin_port,
	sock->sin_addr.s_addr );
}

int file_reader (int s, char* name)
{
    int fd, len, nlen;
    char *buffer = 0;

    if ( name==0 || name[0]==0 ) {
	fprintf(stderr,"Empty/null name\n");
	return -1;
    }
    if ( (fd = open(name, O_RDONLY)) < 0 ) {
	fprintf(stderr, "Error opening file '%s'\n", name);
	return -1;
    } else if ( (len = lseek(fd, 0L, SEEK_END)) < 0 ) {
	fprintf(stderr, "Error placing file pointer to EOF\n");
    } else if ( lseek(fd, 0L, SEEK_SET) < 0 ) {
	fprintf(stderr, "Error placing file pointer to begin of File\n");
    } else if ( (buffer = (char *)malloc(len+1))==NULL ) {
	fprintf(stderr, "Error in allocating file buffer (%d bytes)\n", len+1);
    } else if ( (len = read(fd, buffer, len)) < 0 ) {
	fprintf(stderr, "Error while reading file %s\n", name);
	return -1;
    }

    buffer[len] = 0;

    //nlen = htonl(len);
    nlen = len;
    fprintf(stderr,"len=%d, htonl(len)=%d\n",len,nlen);

/*
    if (write(s, &nlen, sizeof(int)) < 0) {
	fprintf(stderr, "Error writing file length in File Socket\n");
	exit(1);
    }
*/
{
int nBytes = send(s,&nlen,sizeof(int),0);
fprintf(stderr,"nBytes(send)=%d <nlen=%d>\n",nBytes,nlen);
if ( nBytes!=sizeof(int) ) {
	fprintf(stdout,"Error writing file length in file Socket\n");
	exit(1);
}
}

    if ( len>0 ) {
/*
	if (write(s, buffer, len) < 0) {
	    fprintf(stderr, "Error writing File in server\n");
	    exit(1);
	}
*/
	{
	int nBytes = send(s,buffer,len,0);
	fprintf(stderr,"nBytes(send)=%d buffer=<%s>\n",nBytes,buffer);
	if ( nBytes!=len ) {
		fprintf(stderr,"Error sending file!\n");
	}
	}
    }
    if (fd >= 0) close(fd);
    if (buffer) free(buffer);
    return len;
}

int liga_Servidor (fd_set* maskInPtr, unsigned short aPortNr)
{
// struct hostent *hp;
 struct sockaddr_in nome;
 int len = sizeof(struct sockaddr_in);
 int sk_fich;

// bzero( &nome, sizeof(nome) );
 sk_fich = socket( AF_INET, SOCK_STREAM, 0 );
 if ( sk_fich < 0 ) ErroFatal("Creating TCP socket");

 nome.sin_family = AF_INET;
 nome.sin_addr.s_addr = INADDR_ANY;
 nome.sin_port = aPortNr;
 if ( bind(sk_fich,(struct sockaddr*)&nome,len) ) ErroFatal("Bind TCP failed");

 listen( sk_fich, 1 );
 if ( getsockname( sk_fich,(struct sockaddr*)&nome,&len) ) ErroFatal("Getting socket name");

 fprintf(stderr,"Port used:\t%u [%u]\n",
	(unsigned)ntohs(nome.sin_port),
	(unsigned)nome.sin_port);

 // Mascaras
 FD_ZERO(maskInPtr);
 FD_SET(sk_fich,maskInPtr);

 return sk_fich;
}// liga_Servidor

/*************************************************************************
	    MAIN
/*************************************************************************/
int main ()
{
 fd_set	mascara, mask_in;
 int	pendentes;
 int    sk_fich, sk_ns=-1;
 int    tableSize = 1024;
 char   doShutDown;

#ifdef DOS_SPEC
// INIT-STUFF (WIN32 specific)
// Startup DLL
WORD wVersionRequested;
WSADATA wsaData;
int err;
wVersionRequested = MAKEWORD( 2, 0 );
err = WSAStartup( wVersionRequested, &wsaData );
if ( err != 0 ) {
	 /* Tell the user that we couldn't find a usable */
	 /* WinSock DLL.                                  */
	 return 1;
}
/* Confirm that the WinSock DLL supports 2.0.*/
/* Note that if the DLL supports versions greater    */
/* than 2.0 in addition to 2.0, it will still return */
/* 2.0 in wVersion since that is the version we      */
/* requested.                                        */
if ( LOBYTE( wsaData.wVersion ) != 2 ||
		  HIBYTE( wsaData.wVersion ) != 0 ) {
	 /* Tell the user that we couldn't find a usable */
	 /* WinSock DLL.                                  */
	 WSACleanup( );
	 return 2;
}
// END-STUFF
#endif DOS_SPEC

 sk_fich = liga_Servidor(&mask_in,123);
 doShutDown = (sk_fich<0);

 while (doShutDown==0)
 {
    mascara = mask_in;
    //tableSize = getdtablesize();
    fprintf(stdout,"tableSize=%d\n",tableSize);
    pendentes = select( tableSize, &mascara, NULL, NULL, NULL );
    if ( pendentes < 0  )
	fprintf( stderr, "Interrompeu-se Select\n" );
    else
      {
	    if ( FD_ISSET(sk_ns, &mascara) )
	    {
		char buffer[256];
		int nameLength;
/*
		if ( read(sk_ns,&nameLength,sizeof(int)) < 0 ) {
			ErroFatal("Name length received with error");
		}
*/
		{
		unsigned char thisLen = 0;
		int nBytes = recv(sk_ns,&thisLen,1,0);
		nameLength = thisLen;
		fprintf(stderr,"nBytes(recv)=%d (nameLength=%d)\n",nBytes,nameLength);
		if ( nBytes!=1 ) ErroFatal("Name length received with error");
		}
		fprintf(stderr,"Received filename length %d\n",nameLength);
/*
		if ( read( sk_ns, buffer, nameLength ) < 0 ) {
			ErroFatal("Filename received with error\n");
		}
*/
		{
		int nBytes = recv(sk_ns,buffer,nameLength,0);
		fprintf(stderr,"nBytes(recv)=%d\n",nBytes);
		if ( nBytes < 0 )
			ErroFatal("Unable to receive filename");
		buffer[nBytes] = 0;
		fprintf(stderr,"Received filename <%s>\n",buffer);
		if ( nBytes!=nameLength )
			ErroFatal("Full filename was not received");
		}

		doShutDown = strcmp(buffer,"--quit")==0;

		if ( doShutDown==0 ) {
			int res = file_reader( sk_ns, buffer );
			if ( res<0 ) fprintf(stderr,"Could not open file %s\n",buffer);
		}

		// CLEAR AND SHUT DOWN connection
		FD_CLR(sk_ns, &mask_in);
		shutdown( sk_ns, 2 );
		close( sk_ns );
	   }
	else if ( FD_ISSET(sk_fich,&mascara) )
	    {
		struct sockaddr_in para;
		int thisLen = sizeof(para);
		sk_ns = accept( sk_fich, &para, &thisLen  );
		FD_SET(sk_ns, &mask_in);
		fprintf(stderr,"Connected server\n");
	    }
	else fprintf(stderr,"Socket out of mask!\n" );
      }
 }//WHILE INFTO
 return 0;
}

