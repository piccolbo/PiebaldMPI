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
#include <R_ext/Rdynload.h>
#include <mpi.h>

#include "init_finalize.h"
#include "commands.h"
#include "state.h"
#include "lapply_helpers.h"

SEXP findFunction() {
   int nameLength;
   char *functionName;
   SEXP retval;

   MPI_Bcast(&nameLength, 1, MPI_INT, 0, MPI_COMM_WORLD);
   functionName = Calloc(nameLength, char);
   MPI_Bcast((void*) functionName, nameLength, MPI_CHAR, 0, MPI_COMM_WORLD);
   retval = findVar(install(functionName), R_GlobalEnv);
   Free(functionName);
   return(retval);
}

void lapplyWorkerPiebaldMPI() {
   int remainderLength;
   int bytesCount;
   SEXP serializeRemainder, serializeArgs;
   SEXP returnList, theFunction;
   SEXP args, remainder;
   SEXP serializeCall, unserializeCall, functionCall;
   unsigned char *receiveBuffer = NULL;

   theFunction = findFunction();

   MPI_Bcast(&remainderLength, 1, MPI_INT, 0, MPI_COMM_WORLD);
   PROTECT(serializeRemainder = allocVector(RAWSXP, remainderLength));
   MPI_Bcast((void*) RAW(serializeRemainder), remainderLength, 
      MPI_BYTE, 0, MPI_COMM_WORLD);

   MPI_Scatter(NULL, 0, MPI_INT, &bytesCount, 1, MPI_INT, 0, MPI_COMM_WORLD);

   receiveBuffer = Calloc(bytesCount, unsigned char);

   MPI_Scatterv(NULL, NULL, NULL, MPI_BYTE, receiveBuffer, bytesCount, 
      MPI_BYTE, 0, MPI_COMM_WORLD);

   PROTECT(serializeArgs = allocVector(RAWSXP, bytesCount));
   memcpy(RAW(serializeArgs), receiveBuffer, bytesCount);

   PROTECT(unserializeCall = lang2(readonly_unserialize, R_NilValue));
   PROTECT(serializeCall   = lang3(readonly_serialize, R_NilValue, R_NilValue));

   SETCADR(unserializeCall, serializeRemainder);
   PROTECT(remainder = eval(unserializeCall, R_GlobalEnv));

   SETCADR(unserializeCall, serializeArgs);
   PROTECT(args = eval(unserializeCall, R_GlobalEnv));

   functionCall = Rf_VectorToPairList(remainder);

   PROTECT(functionCall = LCONS(readonly_lapply, 
                             LCONS(args, LCONS(theFunction, functionCall))));

   SETCADR(serializeCall, eval(functionCall, R_GlobalEnv));

   PROTECT(returnList = eval(serializeCall, R_GlobalEnv));
  
   bytesCount = LENGTH(returnList);

   MPI_Gather(&bytesCount, 1, MPI_INT, NULL, 0, MPI_INT, 0, MPI_COMM_WORLD);

   MPI_Gatherv(RAW(returnList), bytesCount, MPI_BYTE, 
      NULL, 0, NULL, MPI_BYTE, 0, MPI_COMM_WORLD);

   UNPROTECT(8);
   Free(receiveBuffer); 
}
