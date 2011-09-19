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

struct membuf_st {
    size_t size;
    size_t count;
    unsigned char *buf;
};




SEXP lapplyPiebaldMPI(SEXP functionName, SEXP serializeArgs, 
      SEXP serializeRemainder) {

   checkPiebaldInit();

   int i, numArgs = LENGTH(serializeArgs);
   size_t totalLength;
   int *argcounts = Calloc(readonly_nproc, int);
   int *rawByteDisplacements = Calloc(readonly_nproc, int);
   int *rawByteCounts = Calloc(readonly_nproc, int);
   unsigned char *argRawBytes = NULL, *receiveBuffer = NULL;

   SEXP arg, returnList, serialReturnList;
   SEXP unserializeCall, functionCall;
   int supervisorWorkCount, supervisorByteCount;
   int *supervisorSizes;
   int argOffset, byteBufferOffset;

   int command = LAPPLY;
   MPI_Bcast(&command, 1, MPI_INT, 0, MPI_COMM_WORLD);

   sendFunctionName(functionName);

   sendRemainder(serializeRemainder);

   supervisorWorkCount = numArgs / readonly_nproc;
   supervisorSizes = Calloc(supervisorWorkCount, int);

   sendArgCounts(argcounts, supervisorWorkCount, numArgs);

   sendRawByteCounts(rawByteCounts, argcounts, serializeArgs, &totalLength);

   generateRawByteDisplacements(rawByteDisplacements, rawByteCounts);
   
   sendArgDisplacements(argcounts, supervisorSizes, serializeArgs);

   argRawBytes = Calloc(totalLength, unsigned char);
   receiveBuffer = Calloc(rawByteCounts[0], unsigned char);

   sendArgRawBytes(argRawBytes, rawByteDisplacements, rawByteCounts,
    receiveBuffer, serializeArgs);

   PROTECT(returnList = allocVector(VECSXP, numArgs));

   PROTECT(unserializeCall = lang2(readonly_unserialize, R_NilValue));
   PROTECT(functionCall = lang2(findVar(
       install(CHAR(STRING_ELT(functionName, 0))), R_GlobalEnv), R_NilValue));

   for(i = 0; i < supervisorWorkCount; i++) {
      SEXP serialArg = VECTOR_ELT(serializeArgs, i);
      SETCADR(unserializeCall, serialArg);
      SEXP arg = eval(unserializeCall, R_GlobalEnv);
      SETCADR(functionCall, arg); 
      SET_VECTOR_ELT(returnList, i, eval(functionCall, R_GlobalEnv));
   }

   UNPROTECT(1);

   int *incomingLengths = Calloc(readonly_nproc, int);
   int *incomingDisplacements = Calloc(readonly_nproc, int);
   int empty = 0;

   MPI_Gather(&empty, 1, MPI_INT, incomingLengths, 1, MPI_INT, 0, MPI_COMM_WORLD);

   int totalIncomingLength = 0;
   for(i = 0; i < readonly_nproc; i++) {
      totalIncomingLength += incomingLengths[i];
      if (i > 0) incomingDisplacements[i] = incomingLengths[i - 1] + incomingDisplacements[i - 1];
   }

   int *incomingSizes = Calloc(numArgs, int);
   int *incomingSizeDisplacements = Calloc(readonly_nproc, int);
   for(i = 1; i < readonly_nproc; i++) {
      incomingSizeDisplacements[i] = incomingSizeDisplacements[i - 1] + argcounts[i - 1];
   }

   MPI_Gatherv(NULL, 0, MPI_INT, incomingSizes, argcounts, incomingSizeDisplacements,
      MPI_INT, 0, MPI_COMM_WORLD);

   unsigned char *serializedBytesBuffer = Calloc(totalIncomingLength, unsigned char);
   MPI_Gatherv(NULL, 0, MPI_BYTE, serializedBytesBuffer, incomingLengths, incomingDisplacements,
      MPI_BYTE, 0, MPI_COMM_WORLD);
 
   argOffset = supervisorWorkCount;
   byteBufferOffset = 0;
   while(argOffset < numArgs) {
      SEXP serialArg;
      PROTECT(serialArg = allocVector(RAWSXP, incomingSizes[argOffset]));
      memcpy(RAW(serialArg), serializedBytesBuffer + byteBufferOffset, incomingSizes[argOffset]);
      SETCADR(unserializeCall, serialArg);
      SET_VECTOR_ELT(returnList, argOffset, eval(unserializeCall, R_GlobalEnv));
      UNPROTECT(1);
      byteBufferOffset += incomingSizes[argOffset];
      argOffset++;
   }

   UNPROTECT(2);

   Free(supervisorSizes);
   Free(rawByteDisplacements);
   Free(argRawBytes);  
   Free(receiveBuffer);
   Free(rawByteCounts);
   Free(argcounts);
   Free(incomingLengths);
   Free(incomingSizes);
   Free(incomingSizeDisplacements);
   Free(serializedBytesBuffer);

   return(returnList);
}

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
   PROTECT(functionCall = lang2(theFunction, R_NilValue));

   for(i = 0; i < workCount; i++) {
      SEXP serialArg = VECTOR_ELT(serializeArgs, i);
      SETCADR(unserializeCall, serialArg);
      SEXP arg = eval(unserializeCall, R_GlobalEnv);
      SETCADR(functionCall, arg); 
      SET_VECTOR_ELT(serializeReturnList, i, eval(functionCall, R_GlobalEnv));
   }
  
   UNPROTECT(2);

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
