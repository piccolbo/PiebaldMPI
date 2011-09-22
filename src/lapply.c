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


void lapplyPiebaldMPI_doSend(SEXP functionName, SEXP serializeArgs, 
   SEXP serializeRemainder) {

   int totalLength = 0;
   int *lengths        = Calloc(readonly_nproc, int);
   int *displacements  = Calloc(readonly_nproc, int);

   int command = LAPPLY;
   MPI_Bcast(&command, 1, MPI_INT, 0, MPI_COMM_WORLD);

   sendFunctionName(functionName);

   sendRemainder(serializeRemainder);

   sendRawByteCounts(lengths, serializeArgs, &totalLength);

   generateRawByteDisplacements(displacements, lengths);
   
   sendArgRawBytes(displacements, lengths, totalLength, serializeArgs);

   Free(lengths);
   Free(displacements);

}

void lapplyPiebaldMPI_doReceive(SEXP workerResultsList, SEXP returnList) {

   int *lengths       = Calloc(readonly_nproc, int);
   int *displacements = Calloc(readonly_nproc, int);

   int totalIncomingLength = 0;
   unsigned char *buffer;

   receiveIncomingLengths(lengths, displacements, &totalIncomingLength);

   buffer = Calloc(totalIncomingLength, unsigned char);

   receiveIncomingData(buffer, lengths, displacements);
 
   processIncomingData(buffer, lengths, workerResultsList, returnList);

   Free(lengths);
   Free(displacements);
   Free(buffer);

}


SEXP lapplyPiebaldMPI(SEXP functionName, SEXP serializeArgs, 
      SEXP serializeRemainder, SEXP argLength) {

   checkPiebaldInit();

   int length = INTEGER(argLength)[0];
   SEXP workerResultsList, returnList;

   PROTECT(workerResultsList = allocVector(VECSXP, readonly_nproc));
   PROTECT(returnList = allocVector(VECSXP, length));

   lapplyPiebaldMPI_doSend(functionName, serializeArgs, serializeRemainder);

   evaluateLocalWork(functionName, serializeArgs, serializeRemainder, 
      workerResultsList);

   lapplyPiebaldMPI_doReceive(workerResultsList, returnList);

   UNPROTECT(2);

   return(returnList);
}

