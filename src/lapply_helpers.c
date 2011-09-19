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


void sendFunctionName(SEXP functionName) {
   SEXP functionNameExpression = PROTECT(STRING_ELT(functionName, 0));
   char* functionString = (char*) CHAR(functionNameExpression);
   int nameLength = strlen(functionString) + 1;

   MPI_Bcast(&nameLength, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast((void*) functionString, nameLength, MPI_CHAR, 0, MPI_COMM_WORLD);

   UNPROTECT(1); // STRING_ELT(functionName, 0)
}

void sendRemainder(SEXP serializeRemainder) {
   int remainderLength = LENGTH(serializeRemainder);
   MPI_Bcast(&remainderLength, 1, MPI_INT, 0, MPI_COMM_WORLD);
   MPI_Bcast((void*) RAW(serializeRemainder), remainderLength, MPI_BYTE, 0, MPI_COMM_WORLD);
}

void sendArgCounts(int *argcounts, int supervisorWorkCount, int numArgs) {
   int i, div, mod, offset;

   argcounts[0] = supervisorWorkCount;
   div = (numArgs - supervisorWorkCount) / (readonly_nproc - 1);
   mod = (numArgs - supervisorWorkCount) % (readonly_nproc - 1);
   offset = 1 + mod;
   for(i = 1; i < offset; i++) {
      argcounts[i] = div + 1;
   }
   for(i = offset; i < readonly_nproc; i++) {
      argcounts[i] = div;
   }

   MPI_Scatter(argcounts, 1, MPI_INT, &supervisorWorkCount, 1, MPI_INT, 0, MPI_COMM_WORLD);
}

void sendRawByteCounts(int *rawByteCounts, int *argcounts, SEXP serializeArgs, size_t *totalLength) {
   size_t currentLength = 0, offset = 0;
   int supervisorByteCount;
   int i, proc, current = 1;
   int numArgs = LENGTH(serializeArgs);
   int supervisorWorkCount = argcounts[0];
   SEXP arg;

   *totalLength = 0;
   proc = (supervisorWorkCount == 0) ? 1 : 0;
   for(i = 0; i < numArgs; i++) {
      PROTECT(arg = VECTOR_ELT(serializeArgs, i));
      offset = LENGTH(arg);
      *totalLength += offset;
      currentLength += offset;
      if(argcounts[proc] == current) {
         rawByteCounts[proc] = currentLength;
         currentLength = 0;
         current = 1;
         proc++;
      } else {
         current++;
      }
      UNPROTECT(1); // VECTOR_ELT(serializeArgs, i)
   }

   MPI_Scatter(rawByteCounts, 1, MPI_INT, &supervisorByteCount, 1, MPI_INT, 0, MPI_COMM_WORLD);
}

void generateRawByteDisplacements(int *rawByteDisplacements, int *rawByteCounts) {
   int i;
   for(i = 1; i < readonly_nproc; i++) {
      rawByteDisplacements[i] = rawByteCounts[i - 1] + rawByteDisplacements[i - 1];
   }
}

void sendArgDisplacements(int *argcounts, int *supervisorSizes, SEXP serializeArgs) {
   int numArgs = LENGTH(serializeArgs);
   int *sizes = Calloc(numArgs, int);
   int *displacements = Calloc(readonly_nproc, int);
   int supervisorWorkCount = argcounts[0];
   int i, offset;
   SEXP arg;

   for(i = 0; i < numArgs; i++) {
      PROTECT(arg = VECTOR_ELT(serializeArgs, i));
      offset = LENGTH(arg);
      sizes[i] = offset;
      UNPROTECT(1); // VECTOR_ELT(serializeArgs, i)
   } 

   for(i = 1; i < readonly_nproc; i++) {
      displacements[i] = displacements[i - 1] + argcounts[i - 1];
   }

   MPI_Scatterv(sizes, argcounts, displacements, MPI_INT,
      supervisorSizes, supervisorWorkCount, MPI_INT, 0, MPI_COMM_WORLD);

   Free(sizes);
   Free(displacements);
}

void sendArgRawBytes(unsigned char *argRawBytes, int *rawByteDisplacements, int *rawByteCounts,
    unsigned char *receiveBuffer, SEXP serializeArgs) {

   int i, numArgs = LENGTH(serializeArgs);
   size_t offset, currentLength = 0;
   SEXP arg;

   for(i = 0; i < numArgs; i++) {
      PROTECT(arg = VECTOR_ELT(serializeArgs, i));
      offset = LENGTH(arg);
      memcpy(argRawBytes + currentLength, RAW(arg), offset);
      currentLength += offset;
      UNPROTECT(1); // VECTOR_ELT(serializeArgs, i)
   }

   MPI_Scatterv(argRawBytes, rawByteCounts, rawByteDisplacements, MPI_BYTE,
      receiveBuffer, rawByteCounts[0], MPI_BYTE, 0, MPI_COMM_WORLD);
}


void evaluateLocalWork(SEXP functionName, SEXP serializeArgs, SEXP serializeRemainder, SEXP returnList, int count) {
   int i;
   SEXP unserializeCall, functionCall;
   SEXP remainder;

   PROTECT(unserializeCall = lang2(readonly_unserialize, R_NilValue));

   SETCADR(unserializeCall, serializeRemainder);
   PROTECT(remainder = eval(unserializeCall, R_GlobalEnv));

   functionCall = Rf_VectorToPairList(remainder);

   PROTECT(functionCall = LCONS(findVar(
       install(CHAR(STRING_ELT(functionName, 0))), R_GlobalEnv),
          LCONS(R_NilValue, functionCall)));

   for(i = 0; i < count; i++) {
      SEXP serialArg = VECTOR_ELT(serializeArgs, i);
      SETCADR(unserializeCall, serialArg);
      SEXP arg = eval(unserializeCall, R_GlobalEnv);
      SETCADR(functionCall, arg); 
      SET_VECTOR_ELT(returnList, i, eval(functionCall, R_GlobalEnv));
   }

   UNPROTECT(3);

}


void receiveIncomingLengths(int *lengths) {
   const int empty = 0;

   MPI_Gather((void*) &empty, 1, MPI_INT, lengths, 
      1, MPI_INT, 0, MPI_COMM_WORLD);
}


void receiveIncomingSizes(int *lengths, int *sizes, 
                          int *displacements, int *argcounts, int *total) {

   int i, *sizeDisplacements = Calloc(readonly_nproc, int);

   for(i = 0; i < readonly_nproc; i++) {
      *total += lengths[i];
      if (i > 0) displacements[i] = lengths[i - 1] + displacements[i - 1];
   }

   for(i = 1; i < readonly_nproc; i++) {
      sizeDisplacements[i] = sizeDisplacements[i - 1] + argcounts[i - 1];
   }

   MPI_Gatherv(NULL, 0, MPI_INT, sizes, argcounts, sizeDisplacements,
      MPI_INT, 0, MPI_COMM_WORLD);

   free(sizeDisplacements);
}

void receiveIncomingData(unsigned char *buffer, int *lengths, int *displacements) {

   MPI_Gatherv(NULL, 0, MPI_BYTE, buffer, lengths, displacements,
      MPI_BYTE, 0, MPI_COMM_WORLD);

}

void processIncomingData(unsigned char *buffer, SEXP returnList, 
   int *sizes, int supervisorWorkCount, int numArgs) {

   int element = supervisorWorkCount;
   int offset = 0;
   SEXP unserializeCall;

   PROTECT(unserializeCall = lang2(readonly_unserialize, R_NilValue));

   while(element < numArgs) {
      int nextSize = sizes[element];
      SEXP serialArg;
      PROTECT(serialArg = allocVector(RAWSXP, nextSize));
      memcpy(RAW(serialArg), buffer + offset, nextSize);
      SETCADR(unserializeCall, serialArg);
      SET_VECTOR_ELT(returnList, element, eval(unserializeCall, R_GlobalEnv));
      UNPROTECT(1);
      offset += nextSize;
      element++;
   }

   UNPROTECT(1);
}
