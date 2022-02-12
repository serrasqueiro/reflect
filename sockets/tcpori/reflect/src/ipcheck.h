/* ipcheck.h -- part of reflect/src

   (c)2020 Henrique Moreira

*/

#ifndef IPCHECK_X_H
#define IPCHECK_X_H

#include "debug.h"
#include "reflog.h"

#define eprint(fErr, args...) if (fErr) { fprintf(fErr, args); }


typedef struct {
    char str_host[256];
    ip_string str_ip;
    char auth_file[256];
} t_auth_where;

typedef struct {
    ip_string str_ip;
    t_uint8 CIDR;
    char comment[128];
} t_item;

typedef struct _p_node {
    t_item item;
    struct _p_node* next;
} auth_node;

typedef struct {
    auth_node* start;
    auth_node* last;
} list_auth_nodes;

typedef struct {
    int n;
    list_auth_nodes list;
} t_allowed;

#endif /* IPCHECK_X_H */
