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

#ifndef _lapply_helpers_h
#define _lapply_helpers_h

#include "R.h"
#include <Rinternals.h>
#include <Rdefines.h>
#include <R_ext/Rdynload.h>

void sendFunctionName(SEXP functionName);
void sendRemainder(SEXP serializeRemainder);
void sendRawByteCounts(int *lengths, SEXP serializeArgs, int *totalLength);
void generateRawByteDisplacements(int *displacements, int *lengths);
void sendArgRawBytes(int *displacements, int *lengths, int totalLength, 
   SEXP serializeArgs);

void evaluateLocalWork(SEXP functionName, SEXP serializeArgs, SEXP serializeRemainder, SEXP returnList);

void receiveIncomingLengths(int *lengths, int *displacements, int *total);
void receiveIncomingData(unsigned char *buffer, int *lengths, int *displacements);
void processIncomingData(unsigned char *buffer, int *lengths, 
   SEXP workerResultsList, SEXP returnList);

#endif //_lapply_helpers_h
