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

/**
   Broadcast the name of a function from the supervisor to the worker processes.

   @param[in] functionName  R character vector storing name
*/
void sendFunctionName(SEXP functionName) {
   SEXP functionNameExpression = PROTECT(STRING_ELT(functionName, 0));
   char* functionString = (char*) CHAR(functionNameExpression);
   int nameLength = strlen(functionString) + 1;

   MPI_Bcast(&nameLength, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast((void*) functionString, nameLength, MPI_CHAR, 0, MPI_COMM_WORLD);

   UNPROTECT(1);
}



/**
   Broadcast the "..." arguments from the supervisor to the worker processes.

   @param[in] serializeRemainder R raw vector storing serialized "..." args
*/
void sendRemainder(SEXP serializeRemainder) {
   int remainderLength = LENGTH(serializeRemainder);
   MPI_Bcast(&remainderLength, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast((void*) RAW(serializeRemainder), remainderLength, 
      MPI_BYTE, 0, MPI_COMM_WORLD);
}


/**
   Populate the raw byte counts and broadcast them to worker processes.

   This function will determine the length (the byte count) of 
   each serialized list of tasks to be sent to a worker process, 
   and then broadcast the byte counts to the worker processes.

   @param[out] lengths          array of byte counts
   @param[in]  serializeArgs    R list of raw vectors with serialized input
   @param[out] totalLength      sum of rawByteCounts
*/
void sendRawByteCounts(int *lengths, SEXP serializeArgs, 
                       int *totalLength) {
   int length = 0;
   int i, supervisorByteCount;
   SEXP arg;

   *totalLength = 0;
   for(i = 0; i < readonly_nproc; i++) {
      PROTECT(arg = VECTOR_ELT(serializeArgs, i));
      length = LENGTH(arg);
      *totalLength += length;
      lengths[i] = length;
      UNPROTECT(1);
   }

   MPI_Scatter(lengths, 1, MPI_INT, &supervisorByteCount, 
      1, MPI_INT, 0, MPI_COMM_WORLD);
}

/**
   Use the raw byte counts to generate raw byte displacements.

   Given the array of raw byte counts, generate the array of raw
   byte displacements. MPI variable length broadcasts require an array
   of counts and an array of displacements. Since we have zero bytes
   padded in between elements of the output buffer, it is trivial
   to determine the diplacement array given the byte count array.

   @param[out]  displacements     array of byte displacements
   @param[in]   lengths           array of byte counts
*/
void generateRawByteDisplacements(int *displacements, int *lengths) {
   int i;
   for(i = 1; i < readonly_nproc; i++) {
      displacements[i] = lengths[i - 1] + displacements[i - 1];
   }
}

/**
   Broadcast the tasks to the worker processes.

   @param[in]  displacements     array of byte displacements
   @param[in]  lengths           array of byte counts
   @param[in]  totalLength       sum of lengths
   @param[in]  serializeArgs     R list of raw vectors with serialized input
*/
void sendArgRawBytes(int *displacements, int *lengths, int totalLength, 
   SEXP serializeArgs) {

   int i, numArgs = LENGTH(serializeArgs);
   int offset, currentLength = 0;
   SEXP arg;
  
   unsigned char *sendBuffer = NULL, *receiveBuffer = NULL;

   sendBuffer = Calloc(totalLength, unsigned char);
   receiveBuffer = Calloc(lengths[0], unsigned char);

   for(i = 0; i < numArgs; i++) {
      PROTECT(arg = VECTOR_ELT(serializeArgs, i));
      offset = LENGTH(arg);
      memcpy(sendBuffer + currentLength, RAW(arg), offset);
      currentLength += offset;
      UNPROTECT(1);
   }

   MPI_Scatterv(sendBuffer, lengths, displacements, MPI_BYTE,
      receiveBuffer, lengths[0], MPI_BYTE, 0, MPI_COMM_WORLD);

   Free(sendBuffer);
   Free(receiveBuffer);
}

/**
   The supervisor task processes its share of the work.

   After broadcasting the data for each task to the workers,
   the supervisor task now processes its share of the work.

   @param[in]  functionName        R character vector storing name.
   @param[in]  serializeArgs       R list of raw vectors with serialized input
   @param[in]  serializeRemainder  R raw vector storing serialized "..." args
   @param[out] returnList          R list storing results of evaluation
*/
void evaluateLocalWork(SEXP functionName, SEXP serializeArgs, 
   SEXP serializeRemainder, SEXP returnList) {

   SEXP unserializeCall, functionCall;
   SEXP theFunction;
   SEXP args, remainder;

   PROTECT(theFunction = findVar(
       install(CHAR(STRING_ELT(functionName, 0))), R_GlobalEnv));

   PROTECT(unserializeCall = lang2(readonly_unserialize, R_NilValue));

   SETCADR(unserializeCall, serializeRemainder);
   PROTECT(remainder = eval(unserializeCall, R_GlobalEnv));

   SETCADR(unserializeCall, VECTOR_ELT(serializeArgs, 0));
   PROTECT(args = eval(unserializeCall, R_GlobalEnv));

   functionCall = Rf_VectorToPairList(remainder);

   PROTECT(functionCall = LCONS(readonly_lapply, 
             LCONS(args, LCONS(theFunction, functionCall))));

   SET_VECTOR_ELT(returnList, 0, eval(functionCall, R_GlobalEnv));

   UNPROTECT(5);
}



/**
   Receive the total byte counts of the generated results from the workers.

   After processing its local tasks, each worker informs the supervisor
   of the total number of bytes the worker has generated, so that the 
   supervisor can allocate enough memory in the receive buffer.

   @param[in]   lengths         total byte count per worker
   @param[out]  displacements   displacement buffer per worker
   @param[out]  total           total number of bytes to receive

*/
void receiveIncomingLengths(int *lengths, int *displacements, int *total) {
   int i, empty = 0;

   MPI_Gather(&empty, 1, MPI_INT, lengths, 
      1, MPI_INT, 0, MPI_COMM_WORLD);

   for(i = 0; i < readonly_nproc; i++) {
      *total += lengths[i];
      if (i > 0) displacements[i] = lengths[i - 1] + displacements[i - 1];
   }
}



/**
   Receive the return values from the workers.

   @param[out]  buffer          receiving buffer
   @param[in]   lengths         total byte count per worker
   @param[in]   displacements   displacement buffer per worker
*/
void receiveIncomingData(unsigned char *buffer, int *lengths, int *displacements) {

   MPI_Gatherv(NULL, 0, MPI_BYTE, buffer, lengths, displacements,
      MPI_BYTE, 0, MPI_COMM_WORLD);

}


/**
   Process the return values from the workers.

   Unserialize the return values from the workers and 
   populate the R list with the values. The tasks processed by the
   supervisor have already been populated into the returnList, and
   they do not appear inside the data buffer.

   @param[in]  buffer              data buffer
   @param[in]  lengths             number of bytes per worker
   @param[out] returnList          R list storing results of evaluation

*/
void processIncomingData(unsigned char *buffer, int *lengths, SEXP returnList) {

   int i, offset = 0;
   SEXP unserializeCall;

   PROTECT(unserializeCall = lang2(readonly_unserialize, R_NilValue));

   for(i = 1; i < readonly_nproc; i++) {
      int nextSize = lengths[i];
      SEXP serialList;
      PROTECT(serialList = allocVector(RAWSXP, nextSize));
      memcpy(RAW(serialList), buffer + offset, nextSize);
      SETCADR(unserializeCall, serialList);
      SET_VECTOR_ELT(returnList, i, eval(unserializeCall, R_GlobalEnv));
      UNPROTECT(1);
      offset += nextSize;
   }

   UNPROTECT(1);
}
