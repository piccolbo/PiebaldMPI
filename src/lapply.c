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
      SEXP serializeRemainder, int *argcounts, int *supervisorWorkCount) {

   size_t totalLength;
   int  numArgs        = LENGTH(serializeArgs);
   int *lengths        = Calloc(readonly_nproc, int);
   int *displacements  = Calloc(readonly_nproc, int);

   int *supervisorSizes;

   unsigned char *sendBuffer = NULL, *receiveBuffer = NULL;

   int command = LAPPLY;
   MPI_Bcast(&command, 1, MPI_INT, 0, MPI_COMM_WORLD);

   sendFunctionName(functionName);

   sendRemainder(serializeRemainder);

   *supervisorWorkCount = numArgs / readonly_nproc;
   supervisorSizes = Calloc(*supervisorWorkCount, int);

   sendArgCounts(argcounts, *supervisorWorkCount, numArgs);

   sendRawByteCounts(lengths, argcounts, serializeArgs, &totalLength);

   generateRawByteDisplacements(displacements, lengths);
   
   sendArgDisplacements(argcounts, supervisorSizes, serializeArgs);

   sendBuffer = Calloc(totalLength, unsigned char);
   receiveBuffer = Calloc(lengths[0], unsigned char);

   sendArgRawBytes(sendBuffer, displacements, lengths,
    receiveBuffer, serializeArgs);

   Free(lengths);
   Free(displacements);
   Free(supervisorSizes);
   Free(sendBuffer);
   Free(receiveBuffer);

}

void lapplyPiebaldMPI_doReceive(int *argcounts, SEXP returnList, int numArgs, int supervisorWorkCount) {

   int *lengths       = Calloc(readonly_nproc, int);
   int *displacements = Calloc(readonly_nproc, int);
   int *sizes         = Calloc(numArgs, int);   

   int totalIncomingLength = 0;
   unsigned char *buffer;

   receiveIncomingLengths(lengths);
   receiveIncomingSizes(lengths, sizes, displacements, 
                        argcounts, &totalIncomingLength);

   buffer = Calloc(totalIncomingLength, unsigned char);

   receiveIncomingData(buffer, lengths, displacements);
 
   processIncomingData(buffer, returnList, sizes, supervisorWorkCount, numArgs);

   Free(lengths);
   Free(displacements);
   Free(sizes);
   Free(buffer);

}


SEXP lapplyPiebaldMPI(SEXP functionName, SEXP serializeArgs, 
      SEXP serializeRemainder) {

   checkPiebaldInit();

   SEXP returnList;

   int *argcounts           = Calloc(readonly_nproc, int);
   int supervisorWorkCount  = 0;
   int numArgs              = LENGTH(serializeArgs);

   PROTECT(returnList = allocVector(VECSXP, numArgs));

   lapplyPiebaldMPI_doSend(functionName, serializeArgs, 
      serializeRemainder, argcounts, &supervisorWorkCount);

   evaluateLocalWork(functionName, serializeArgs, returnList, supervisorWorkCount);

   lapplyPiebaldMPI_doReceive(argcounts, returnList, numArgs, supervisorWorkCount);

   Free(argcounts);

   UNPROTECT(1);

   return(returnList);
}

