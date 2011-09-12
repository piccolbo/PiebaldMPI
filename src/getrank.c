/*
 *  Copyright 2011 The OpenMx Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "R.h"
#include <Rinternals.h>
#include <Rdefines.h>

#include "init_finalize.h"
#include "state.h"

SEXP getrankPiebaldMPI() {
   SEXP retval;

   checkPiebaldInit();

   PROTECT(retval = allocVector(INTSXP, 1));
   INTEGER(retval)[0] = readonly_rank;
   UNPROTECT(1);

   return(retval);
}
