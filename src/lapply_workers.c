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

#include "lapply_workers_helpers.h"


void lapplyWorkerPiebaldMPI() {
   SEXP serializeRemainder, serializeArgs;
   SEXP returnList, serializeFunction;

   serializeFunction = findFunction();
   
   serializeRemainder = workerGetRemainder();

   serializeArgs = workerGetArgs();
   
   returnList = generateReturnList(serializeFunction, 
      serializeRemainder, serializeArgs);

   sendReturnList(returnList);

   workerCleanup(serializeFunction, serializeRemainder, serializeArgs, returnList);
}


