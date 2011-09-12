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

#include "init_finalize.h"
#include "commands.h"
#include "state.h"
#include <mpi.h>


SEXP initPiebaldMPI() {
   if(readonly_initialized == TRUE) {
      error("The function pbmpi_init() has already been called.");
   }

   MPI_Init(NULL, NULL);
   MPI_Comm_size( MPI_COMM_WORLD, &readonly_nproc );
   MPI_Comm_rank( MPI_COMM_WORLD, &readonly_rank );   

   readonly_initialized = TRUE;

   if (readonly_rank == 0) {      
      return(R_NilValue);
   } else {
      int done = FALSE;
      int command;
      while(done == FALSE) {
         MPI_Bcast(&command, 1, MPI_INT, 0, MPI_COMM_WORLD);
         switch(command) {
            case TERMINATE:
               MPI_Finalize();
               done = TRUE;
               break;
            default:
               break;
         }
      }
   }
   return(R_NilValue);
}

void checkPiebaldInit() {
   if(readonly_initialized == FALSE) {
      error("The function pbmpi_init() must be invoked before this function can be called.");
   }
}


SEXP finalizePiebaldMPI() {
   checkPiebaldInit();

   if (readonly_rank == 0) {
      int command = TERMINATE;
      MPI_Bcast(&command, 1, MPI_INT, 0, MPI_COMM_WORLD);
   }
   MPI_Finalize();

   readonly_initialized = FALSE;   
   return(R_NilValue);
}

