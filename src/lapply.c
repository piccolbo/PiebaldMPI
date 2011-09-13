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

// Helper functions




SEXP lapplyPiebaldMPI(SEXP functionName, SEXP serializeArgs, 
      SEXP serializeRemainder) {

   checkPiebaldInit();


   int numArgs = LENGTH(serializeArgs);
   size_t totalLength;
   int *argcounts = Calloc(readonly_nproc, int);
   int *rawByteDisplacements = Calloc(readonly_nproc, int);
   int *rawByteCounts = Calloc(readonly_nproc, int);
   unsigned char *argRawBytes = NULL, *receiveBuffer = NULL;

   SEXP arg;
   int supervisorWorkCount, supervisorByteCount;

   int command = LAPPLY;
   MPI_Bcast(&command, 1, MPI_INT, 0, MPI_COMM_WORLD);

   sendFunctionName(functionName);

   sendRemainder(serializeRemainder);

   supervisorWorkCount = numArgs / readonly_nproc;

   sendArgCounts(argcounts, supervisorWorkCount, numArgs);

   sendRawByteCounts(rawByteCounts, argcounts, serializeArgs, &totalLength);

   generateRawByteDisplacements(rawByteDisplacements, rawByteCounts);
   
   argRawBytes = Calloc(totalLength, unsigned char);
   receiveBuffer = Calloc(rawByteCounts[0], unsigned char);

   sendArgRawBytes(argRawBytes, rawByteDisplacements, rawByteCounts,
    receiveBuffer, serializeArgs);

   Free(rawByteDisplacements);
   Free(argRawBytes);  
   Free(receiveBuffer);
   Free(rawByteCounts);
   Free(argcounts);

   return(R_NilValue);
}


void lapplyWorkerPiebaldMPI() {
   int nameLength, remainderLength;
   int workCount, bytesCount;
   char *functionName;
   SEXP serializeRemainder;

   MPI_Bcast(&nameLength, 1, MPI_INT, 0, MPI_COMM_WORLD);
   functionName = Calloc(nameLength, char);
   MPI_Bcast((void*) functionName, nameLength, MPI_CHAR, 0, MPI_COMM_WORLD);

   MPI_Bcast(&remainderLength, 1, MPI_INT, 0, MPI_COMM_WORLD);
   PROTECT(serializeRemainder = allocVector(RAWSXP, remainderLength));
   MPI_Bcast((void*) RAW(serializeRemainder), remainderLength, MPI_BYTE, 0, MPI_COMM_WORLD);

   MPI_Scatter(NULL, 0, MPI_INT, &workCount, 1, MPI_INT, 0, MPI_COMM_WORLD);

   MPI_Scatter(NULL, 0, MPI_INT, &bytesCount, 1, MPI_INT, 0, MPI_COMM_WORLD);

   Rprintf("Work count %d, bytes count %d\n", workCount, bytesCount);

   UNPROTECT(1);
   Free(functionName);  
}
