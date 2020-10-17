#!/bin/sh


usage ()
{
 local HINTS='

// IPv4 demo of inet_ntop() and inet_pton()

struct sockaddr_in sa;
char str[INET_ADDRSTRLEN];

// store this IP address in sa:
inet_pton(AF_INET, "192.0.2.33", &(sa.sin_addr));

// now get it back and print it
inet_ntop(AF_INET, &(sa.sin_addr), str, INET_ADDRSTRLEN);

printf("%s\n", str); // prints "192.0.2.33"
'

 echo "Usage:

$0

...to compile executable basic_reflect
"
 exit 0
}


[ "$*" ] && usage

DEBUG=-DDEBUG
[ "$NO" ] && DEBUG=""

# last tested ok with:	gcc (SUSE Linux) 10.2.1 20200825
gcc -g -Wall -o basic_reflect basic_reflect.c $DEBUG

gcc -Wall -Werror -o reflector reflector.c

