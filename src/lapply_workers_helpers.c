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
#include "compiler_directives.h"


/**
   Receive the serialized function from the supervisor.

   @return  R function language object
*/
SEXP findFunction() {
   int length;
   SEXP function;

   MPI_Bcast(&length, 1, MPI_INT, 0, MPI_COMM_WORLD);
   PROTECT(function = allocVector(RAWSXP, length));
   MPI_Bcast((void*) RAW(function), length, 
      MPI_BYTE, 0, MPI_COMM_WORLD);

   return(function);
}

/**
   Receive the "..." arguments from the supervisor.

   @return  R serialized object storing the "..." arguments
*/
SEXP workerGetRemainder() {
   int length;
   SEXP remainder;

   MPI_Bcast(&length, 1, MPI_INT, 0, MPI_COMM_WORLD);
   PROTECT(remainder = allocVector(RAWSXP, length));
   MPI_Bcast((void*) RAW(remainder), length, 
      MPI_BYTE, 0, MPI_COMM_WORLD);

   return(remainder);
}

/**
   Receive the task arguments from the supervisor.

   @return  R serialized object storing the arguments to lapply.
*/
SEXP workerGetArgs() {
   int length;
   SEXP args;
   unsigned char *buffer;

   MPI_Scatter(NULL, 0, MPI_INT, &length, 1, MPI_INT, 0, MPI_COMM_WORLD);

   buffer = Calloc(length, unsigned char);

   MPI_Scatterv(NULL, NULL, NULL, MPI_BYTE, buffer, length, 
      MPI_BYTE, 0, MPI_COMM_WORLD);

   PROTECT(args = allocVector(RAWSXP, length));
   memcpy(RAW(args), buffer, length);

   return(args);
}

/**
   Evaluate the function and generate return list.

   @param[in] serializedFunction   R serialized function language object
   @param[in] serializeRemainder   R serialized "..." arguments to lapply
   @param[in] serializeArgs        R serialized arguments to lapply
   @return                         R serialized object storing the return list.
*/
SEXP generateReturnList(SEXP serializedFunction, SEXP serializeRemainder, 
                        SEXP serializeArgs) {

   SEXP serializeCall, unserializeCall, theFunction, functionCall;
   SEXP args, remainder;
   SEXP returnList;

   PROTECT(unserializeCall = lang2(readonly_unserialize, R_NilValue));
   PROTECT(serializeCall   = lang3(readonly_serialize, R_NilValue, R_NilValue));

   SETCADR(unserializeCall, serializeRemainder);
   PROTECT(remainder = eval(unserializeCall, R_GlobalEnv));

   SETCADR(unserializeCall, serializedFunction);
   PROTECT(theFunction = eval(unserializeCall, R_GlobalEnv));

   SETCADR(unserializeCall, serializeArgs);
   PROTECT(args = eval(unserializeCall, R_GlobalEnv));

   functionCall = Rf_VectorToPairList(remainder);

   PROTECT(functionCall = LCONS(readonly_lapply, 
                             LCONS(args, LCONS(theFunction, functionCall))));

   SETCADR(serializeCall, eval(functionCall, R_GlobalEnv));

   returnList = eval(serializeCall, R_GlobalEnv);
   UNPROTECT(6);
   
   PROTECT(returnList);

   return(returnList);
}


/**
   Send the serialized return list to the supervisor process.

   @param[in] returnList       R serialized object storing the return list.
*/
void sendReturnList(SEXP returnList) {
   int length;

   length = LENGTH(returnList);

   MPI_Gather(&length, 1, MPI_INT, NULL, 0, MPI_INT, 0, MPI_COMM_WORLD);

   MPI_Gatherv(RAW(returnList), length, MPI_BYTE, 
      NULL, 0, NULL, MPI_BYTE, 0, MPI_COMM_WORLD);
}


/**
   Cleanup any variables that live on the protect stack.

   @param[in] serializeFunction   R serialized function language object
   @param[in] serializeRemainder  R serialized "..." arguments to lapply
   @param[in] serializeArgs       R serialized arguments to lapply
   @param[in] returnList          R serialized object storing the return list.
*/
void workerCleanup(SEXP serializeFunction COMPILER_DIRECTIVE_UNUSED,
                   SEXP serializeRemainder COMPILER_DIRECTIVE_UNUSED, 
                   SEXP serializeArgs COMPILER_DIRECTIVE_UNUSED, 
                   SEXP returnList COMPILER_DIRECTIVE_UNUSED) {
   UNPROTECT(4);
}


