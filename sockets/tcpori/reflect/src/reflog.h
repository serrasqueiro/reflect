/* reflog.h -- part of reflect/src

   (c)2000 Henrique Moreira

*/

#ifndef REFLOG_X_H
#define REFLOG_X_H

#define REFLOG_USER(program, level, args...) {	\
    openlog(program, LOG_PID, LOG_USER); \
    syslog(level, args); \
    closelog(); \
    }


typedef unsigned char t_uint8;
typedef unsigned int t_uint;
typedef unsigned long int t_ulong;

typedef struct {
    int accs;
    int rejs;
    long byteCountIngress;
    long byteCountEgress;
} t_stats_net;


#endif /* REFLOG_X_H */
