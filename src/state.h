#ifndef _state_h
#define _state_h

#include "R.h"
#include <Rinternals.h>
#include <Rdefines.h>

// Global state. This variables should be read-only.
extern int readonly_rank, readonly_nproc;
extern int readonly_initialized;

extern SEXP readonly_serialize, readonly_unserialize;
extern SEXP readonly_lapply;

#endif // #define _state_h
