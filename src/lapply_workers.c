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
   int currentLength;
   int i, offset;
   int workCount, bytesCount;
   int *workSizes;
   SEXP serializeRemainder, serializeArgs;
   SEXP serializeReturnList, returnList, theFunction;
   SEXP remainder;
   SEXP serializeCall, unserializeCall, functionCall;
   unsigned char *receiveBuffer = NULL;
   unsigned char *sendBuffer = NULL;

   theFunction = findFunction();

   MPI_Bcast(&remainderLength, 1, MPI_INT, 0, MPI_COMM_WORLD);
   PROTECT(serializeRemainder = allocVector(RAWSXP, remainderLength));
   MPI_Bcast((void*) RAW(serializeRemainder), remainderLength, MPI_BYTE, 0, MPI_COMM_WORLD);

   MPI_Scatter(NULL, 0, MPI_INT, &workCount, 1, MPI_INT, 0, MPI_COMM_WORLD);

   MPI_Scatter(NULL, 0, MPI_INT, &bytesCount, 1, MPI_INT, 0, MPI_COMM_WORLD);

   receiveBuffer = Calloc(bytesCount, unsigned char);
   workSizes = Calloc(workCount, int);

   MPI_Scatterv(NULL, NULL, NULL, MPI_INT, workSizes, workCount, MPI_INT, 0, MPI_COMM_WORLD);

   MPI_Scatterv(NULL, NULL, NULL, MPI_BYTE, receiveBuffer, bytesCount, MPI_BYTE, 0, MPI_COMM_WORLD);

   PROTECT(serializeArgs = allocVector(VECSXP, workCount));
   offset = 0;
   for(i = 0; i < workCount; i++) {
      SEXP argument;
      PROTECT(argument = allocVector(RAWSXP, workSizes[i]));
      memcpy(RAW(argument), receiveBuffer + offset, workSizes[i]);
      offset += workSizes[i];
      SET_VECTOR_ELT(serializeArgs, i, argument);
      UNPROTECT(1);
   }

   PROTECT(serializeReturnList = allocVector(VECSXP, workCount));
   PROTECT(returnList = allocVector(VECSXP, workCount));

   PROTECT(unserializeCall = lang2(readonly_unserialize, R_NilValue));

   SETCADR(unserializeCall, serializeRemainder);
   PROTECT(remainder = eval(unserializeCall, R_GlobalEnv));

   functionCall = Rf_VectorToPairList(remainder);

   PROTECT(functionCall = LCONS(theFunction, LCONS(R_NilValue, functionCall)));

   for(i = 0; i < workCount; i++) {
      SEXP serialArg = VECTOR_ELT(serializeArgs, i);
      SETCADR(unserializeCall, serialArg);
      SEXP arg = eval(unserializeCall, R_GlobalEnv);
      SETCADR(functionCall, arg); 
      SET_VECTOR_ELT(serializeReturnList, i, eval(functionCall, R_GlobalEnv));
   }
  
   UNPROTECT(3);

   PROTECT(serializeCall = lang3(readonly_serialize, R_NilValue, R_NilValue));
   for(i = 0; i < workCount; i++) {
      SEXP arg = VECTOR_ELT(serializeReturnList, i);
      SETCADR(serializeCall, arg);
      SET_VECTOR_ELT(returnList, i, eval(serializeCall, R_GlobalEnv));
   }

   int *answerLengths = Calloc(workCount, int);

   bytesCount = 0;
   for(i = 0; i < workCount; i++) {
      offset = LENGTH(VECTOR_ELT(returnList, i));
      answerLengths[i] = offset;
      bytesCount += offset;
   }

   MPI_Gather(&bytesCount, 1, MPI_INT, NULL, 0, MPI_INT, 0, MPI_COMM_WORLD);

   MPI_Gatherv(answerLengths, workCount, MPI_INT, 
      NULL, NULL, NULL, MPI_INT, 0, MPI_COMM_WORLD);   

   sendBuffer = Calloc(bytesCount, unsigned char);

   offset = 0;
   for(i = 0; i < workCount; i++) {
      SEXP arg = VECTOR_ELT(returnList, i);
      currentLength = LENGTH(arg);      
      memcpy(sendBuffer + offset, RAW(arg), currentLength);
      offset += currentLength;
   }

   MPI_Gatherv(sendBuffer, bytesCount, MPI_BYTE, 
      NULL, 0, NULL, MPI_BYTE, 0, MPI_COMM_WORLD);

   UNPROTECT(5);
   Free(sendBuffer);
   Free(answerLengths);
   Free(receiveBuffer); 
   Free(workSizes);
}
