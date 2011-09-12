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

   MPI_Init(NULL, NULL);
   MPI_Comm_size( MPI_COMM_WORLD, &readonly_nproc );
   MPI_Comm_rank( MPI_COMM_WORLD, &readonly_rank );   

   if (readonly_rank == 0) {      
      return(R_NilValue);
   } else {
      int done = 0;
      int command;
      while(!done) {
         MPI_Bcast(&command, 1, MPI_INT, 0, MPI_COMM_WORLD);
         switch(command) {
            case TERMINATE:
               MPI_Finalize();
               done = 1;
               break;
            default:
               break;
         }
      }
   }
   return(R_NilValue);
}


SEXP finalizePiebaldMPI() {
   if (readonly_rank == 0) {
      int command = TERMINATE;
      MPI_Bcast(&command, 1, MPI_INT, 0, MPI_COMM_WORLD);
   }
   MPI_Finalize();
   return(R_NilValue);
}

