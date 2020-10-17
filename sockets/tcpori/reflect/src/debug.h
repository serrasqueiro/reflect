/* debug.h -- part of reflect/src

   (c)2000 Henrique Moreira

*/

#ifndef DEBUG_X_H
#define DEBUG_X_H

#ifdef DEBUG
#define dprint(args...) printf(args)
#else
#define dprint(args...) ;
#endif /* DEBUG */

#endif /* DEBUG_X_H */
