/* ipcheck.c -- part of reflect/src

   (c)2020 Henrique Moreira

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "ipcheck.h"


/* --------------------------------
   main function -- for tests
   --------------------------------

   use:
	gcc -Wall ipcheck.c -g -o ipcheck_test -DDEBUG_TEST -DDEBUG
*/


int add_node (list_auth_nodes* nodes, auth_node* aNode) {
   if (!aNode) {
       return -1;
   }
   if (aNode->next) {
       return -2;
   }
   if (aNode->item.CIDR <= 0) {
       return -3;
   }
   if (nodes->start) {
       nodes->last->next = aNode;
       nodes->last = aNode;
   }
   else {
       nodes->start = aNode;
       nodes->last = aNode;
   }
   return 0;
}


int read_ipcheck (const char* path, t_allowed* dbcheck) {
   const char* cidr;
   struct sockaddr_in sa;
   FILE* fd;
   char buf[256];
   char* ptr;
   char* ip;
   int len, code;
   t_uint size = 0;
   auth_node* aNode;

   if (!dbcheck) return -1;
   fd = fopen(path, "r");
   if (!fd) {
       return 2;
   }

   dbcheck->n = 0;
   while (fgets(buf, sizeof(buf), fd)) {
       len = strlen(buf);
       if (len <= 0 || buf[0] == '#') {
	   continue;
       }
       if (buf[len-1]<=' ') {
	   buf[len-1] = 0;
       }
       ip = buf;
       ptr = strchr(buf, '/');
       if (ptr) {
	   cidr = ptr+1;
	   ip[ptr-buf] = '\0';
       }
       else {
	   cidr = "32";
       }
       code = inet_pton(AF_INET, ip, &sa.sin_addr);
       dprint("code=%d, buf: '%s', ip=%s, cidr=/'%s', ntoa: %s\n",
	      code,
	      buf, ip, cidr,
	      inet_ntoa(sa.sin_addr));
       if (code >= 1) {
	   aNode = (auth_node*)calloc(1, sizeof(auth_node));
	   snprintf(aNode->item.str_ip, sizeof(ip_string), ip);
	   aNode->item.CIDR = atoi(cidr);
	   code = add_node(&(dbcheck->list), aNode);
	   dbcheck->n++;
       }
   }
   fclose(fd);
   /* Calculate number of entries */
   for (aNode=dbcheck->list.start; aNode; aNode=aNode->next) {
       t_uint this = aNode->item.CIDR;
       t_uint idx = this, pwr = 1;
       for ( ; idx < 32; idx++) {
	   pwr *= 2;
       }
       size += pwr;
       dprint("aNode %p (n=%d), pwr=%d (size=%u): %s/%u\n",
	      aNode, dbcheck->n, pwr, size,
	      aNode->item.str_ip, this);
   }
   return 0;
}


#ifdef DEBUG_TEST
int main(int argc, char* argv[]) {
   const char* def_ipcheck_path = "/tmp/root/ipcheck.txu";
   char* ipcheck = (char*)def_ipcheck_path;
   int code;
   t_allowed aDbCheck;

   if (argv[1]) {
       ipcheck = argv[1];
   }
   printf("ipcheck file: %s\n", ipcheck);
   code = read_ipcheck(ipcheck, &aDbCheck);
   printf("read_ipcheck(%s, &aDbCheck) returned %d\n", ipcheck, code);
   return 0;
}
#endif /* DEBUG_TEST */
