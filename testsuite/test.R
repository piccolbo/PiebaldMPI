#
#   Copyright 2011 The OpenMx Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
# 
#        http://www.apache.org/licenses/LICENSE-2.0
# 
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

source('unitTestUtilities.R')

library(PiebaldMPI)

plus1 <- function(x) { x + 1 }

plus1WithAnonymous <- function(x, ...) {
   args <- list(...)
   return (x + args[[1]])
}

plus1WithNamed <- function(x, ...) {
   args <- list(...)
   return (x + args$inc)
}

pbInit()

tryCatch(
   {
      checkTrue(pbSize() > 1)

      checkIdentical(lapply(1:15, plus1), pbLapply(1:15, plus1))

      checkIdentical(lapply(1:15, plus1WithAnonymous, 5), 
                     pbLapply(1:15, plus1WithAnonymous, 5))

      checkIdentical(lapply(1:15, plus1WithNamed, inc = 5), 
                     pbLapply(1:15, plus1WithNamed, inc = 5))

   }, error = function(e) {
      cat("\n")
      cat(paste("The following error was detected:",
         e$message, "\n"))
      cat("\n")
   }, finally = pbFinalize())
