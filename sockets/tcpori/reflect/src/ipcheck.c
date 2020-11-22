/* ipcheck.c -- part of reflect/src

   (c)2020 Henrique Moreira

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>

#include "ipcheck.h"

#define MAX_IPCHECK_ENTRIES	4096


/* --------------------------------
   main function -- for tests
   --------------------------------

   use:
	gcc -Wall ipcheck.c -g -o ipcheck_test -DDEBUG_TEST -DDEBUG
*/

int new_ip_cidr (const char* ipcheck, char** new_ip, char** new_cidr) {
   char* ptr;
   char* str;
   char* ip;
   char* cidr;
   assert(ipcheck);
   str = strdup(ipcheck);
   ptr = strchr(str, '/');
   if (ptr) {
       cidr = strdup(ptr+1);
       ptr[0] = 0;
   }
   else {
       cidr = strdup("32");
   }
   ip = strdup(str);
   *new_ip = ip;
   *new_cidr = cidr;
   free(str);
   return 0;
}


/* t_allowed ops.
   --------------
*/
void init_allowed (t_allowed* dbcheck)
{
   dbcheck->n = 0;
   dbcheck->list.start = dbcheck->list.last = NULL;
}

void delete_allowed (t_allowed* dbcheck)
{
   auth_node* ptr;
   auth_node* stt = dbcheck->list.start;
   auth_node* now;
   char* comment = NULL;

   assert(comment == NULL);
   if (stt == NULL) {
       return;
   }
   for (ptr=stt; ptr; ) {
       now = ptr->next;
       comment = ptr->item.comment;
       dprint("Debug: deleting %s/%u%s%s\n",
	      ptr->item.str_ip,
	      ptr->item.CIDR,
	      comment[0] ? " comment: " : "",
	      comment);
       free(ptr);
       ptr = now;
   }
   dbcheck->n = 0;
}

void dump_allowed (FILE* fOut, t_allowed* dbcheck)
{
   char* comment;
   auth_node* ptr = dbcheck->list.start;
   for ( ; ptr; ptr=ptr->next) {
       comment = ptr->item.comment;
       fprintf(fOut, "%s/%u%s%s\n",
	       ptr->item.str_ip,
	       ptr->item.CIDR,
	       comment[0] ? " #comment: " : "",
	       comment);
   }
}


int add_node (list_auth_nodes* nodes, auth_node* aNode) {
   if (!aNode) {
       return -1;
   }
   dprint("add_node() str_ip='%s', CIDR=%d; start=%p, last=%p\n",
	  aNode->item.str_ip,
	  aNode->item.CIDR,
	  nodes->start,
	  nodes->last);
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

in_addr_t netmask (int prefix) {
   if (prefix<=0)
       return ~((in_addr_t)-1);
    return ~((1 << (32 - prefix)) - 1);
}

in_addr_t whatmask (int prefix) {
   if (prefix<=0)
       return (in_addr_t)-1;
    return (1 << (32 - prefix)) - 1;
}

int read_ipcheck (FILE* fErr, const char* path, t_allowed* dbcheck) {
   const char* cidr;
   struct sockaddr_in sa;
   FILE* fd;
   char buf[256];
   char comment[256];
   char* ptr;
   char* ip;
   int line = 0, errors = 0;
   int len, code;
   int iCIDR = -1;
   t_uint size = 0;
   auth_node* aNode;
   t_allowed local;

   init_allowed(&local);

   if (!dbcheck) return -1;
   fd = fopen(path, "r");
   if (!fd) {
       return 2;
   }

   local.n = 0;
   while (fgets(buf, sizeof(buf), fd)) {
       errors = 0;
       comment[0] = 0;
       line++;
       len = strlen(buf);
       if (len <= 0 || buf[0] == '#') {
	   continue;
       }
       if (buf[len-1]<=' ') {
	   buf[len-1] = 0;
       }
       /* Check line char is not weird! */
       for (ptr=buf; *ptr; ptr++) {
	   if (*ptr == '\t') {
	       *ptr = ' ';
	   }
	   else {
	       if (*ptr < ' ' || *ptr > '~') {
		   eprint(fErr, "Line %d: Invalid char (0x%08x)\n",
			  line,
			  (unsigned)*ptr);
		   errors++;
		   break;
	       }
	   }
       }
       ptr = strchr(buf, '#');
       if (ptr) {
	   *ptr = '\0';
	   for (ptr++; *ptr; ptr++) {
	       if (*ptr > ' ')
		   break;
	   }
	   snprintf(comment, sizeof(comment), ptr);
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
       iCIDR = atoi(cidr);
       if (code >= 1) {
	   aNode = (auth_node*)calloc(1, sizeof(auth_node));
	   snprintf(aNode->item.str_ip, sizeof(ip_string), ip);
	   aNode->item.CIDR = iCIDR;
	   strcpy(aNode->item.comment, comment);
	   add_node(&(local.list), aNode);
	   local.n++;
	   dprint("Node #%d: str_ip='%s', CIDR=%d\n",
		  local.n,
		  aNode->item.str_ip,
		  aNode->item.CIDR);
	   if (iCIDR < 1 || iCIDR > 32) {
	       eprint(fErr, "Invalid CIDR: %s\n", cidr);
	       errors++;
	   }
       }
       else {
	   eprint(fErr, "Invalid IP: %s\n", ip);
	   errors++;
       }
   }
   fclose(fd);
   if (errors) {
       eprint(fErr, "Errors found (%d): %s\n", errors, path);
       return 3;
   }

   /* Calculate number of entries */
   for (aNode=local.list.start; aNode; aNode=aNode->next) {
       t_uint this = aNode->item.CIDR;
       t_uint idx = this, pwr = 1;
       for ( ; idx < 32; idx++) {
	   pwr *= 2;
       }
       size += pwr;
       dprint("aNode %p (n=%d), pwr=%d (size=%u): %s/%u\n",
	      aNode, local.n, pwr, size,
	      aNode->item.str_ip, this);
   }
   if (size >= MAX_IPCHECK_ENTRIES) {
       eprint(fErr, "Too many IPs defined: %u, max. is: %u\n",
	      size,
	      MAX_IPCHECK_ENTRIES);
       return 4;
   }
   /* Add all IPs to dbcheck */
   for (aNode=local.list.start; aNode; aNode=aNode->next) {
       char* nowIP;
       t_uint this = aNode->item.CIDR;
       t_uint pfix = 32;
       struct in_addr in;
       for ( ; pfix >= this; pfix--) {
	   auth_node* newNode = (auth_node*)calloc(1, sizeof(auth_node));
	   ip = aNode->item.str_ip;
	   strcpy(newNode->item.str_ip, ip);
	   strcpy(newNode->item.comment, aNode->item.comment);
	   inet_pton(AF_INET, ip, &in);
	   nowIP = inet_ntoa(in);
	   printf("IP=%s/%d - %s, prefix %d (n#=%d): %08X\n",
		  ip, this,
		  nowIP,
		  pfix, dbcheck->n,
		  (unsigned)whatmask(pfix));
	   add_node(&(dbcheck->list), newNode);
	   dbcheck->n++;
       }
   }
   delete_allowed(&local);
   return 0;
}


#ifdef DEBUG_TEST

int test_ip_iterate (const char* ipcheck)
{
   char* ip;
   char* cidr;

   new_ip_cidr(ipcheck, &ip, &cidr);
   dprint("ip=%s, CIDR=%s\n", ip, cidr);
   free(ip);
   free(cidr);
   return 0;
}

int main(int argc, char* argv[]) {
   const char* def_ipcheck_path = "/tmp/root/ipcheck.txu";
   char* ipcheck = (char*)def_ipcheck_path;
   FILE* fErr = stderr;
   int code;
   t_allowed aDbCheck;

   init_allowed(&aDbCheck);

   if (argv[1]) {
       ipcheck = argv[1];
       if (ipcheck[0] == '@') {
	   ipcheck++;
	   code = test_ip_iterate(ipcheck);
	   printf("test_ip_iterate(%s) returned %d\n", ipcheck, code);
	   return code;
       }
   }
   printf("ipcheck file: %s\n", ipcheck);
   code = read_ipcheck(fErr, ipcheck, &aDbCheck);
   printf("read_ipcheck(%s, &aDbCheck) returned %d\n", ipcheck, code);
   dump_allowed(stdout, &aDbCheck);
   delete_allowed(&aDbCheck);
   return 0;
}
#endif /* DEBUG_TEST */
