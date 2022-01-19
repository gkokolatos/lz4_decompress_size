#ifndef _CFP_H_
#define _CFP_H_

#include <sys/types.h>

/* the struct */
typedef struct Cfp Cfp;

/* the API */
Cfp *cfopen(const char *);
int cfclose(Cfp *);

ssize_t cfread(char * const, size_t, Cfp *);
char * cfgets(char * const, size_t, Cfp *);
int cfgetc(Cfp *);

#endif
