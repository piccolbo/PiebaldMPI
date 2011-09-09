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

// Global state. This variables should be read-only.
int readonly_rank, readonly_nproc;


// Forward declaractions
SEXP initPiebaldMPI();
SEXP finalizePiebaldMPI();


/* Set up R .Call info */
R_CallMethodDef callMethods[] = {
{"initPiebaldMPI", (void*(*)())&initPiebaldMPI, 0},
{"finalizePiebaldMPI", (void*(*)())&initPiebaldMPI, 0},
{NULL, NULL, 0}
};

void R_init_mylib(DllInfo *info) {
/* Register routines, allocate resources. */
R_registerRoutines(info, NULL, callMethods, NULL, NULL);
}

void R_unload_mylib(DllInfo *info) {
/* Release resources. */
}

SEXP initPiebaldMPI() {

   MPI_Init(NULL, NULL);
   MPI_Comm_size( MPI_COMM_WORLD, &readonly_nproc );
   MPI_Comm_rank( MPI_COMM_WORLD, &readonly_rank );   

   if (readonly_rank == 0) {      
      return(R_NilValue);
   } else {
      while(1) {}
   }
}

SEXP finalizePiebaldMPI() {
   MPI_Finalize();
}
