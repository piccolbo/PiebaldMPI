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


createSegment <- function(base, length, input) {
   if (length == 0) {
      return(list())
   } else {
      return(input[base : (base + length - 1)])
   }
}

serializeInput <- function(input, nproc) {
   argBase <- integer(nproc)
   argLength <- integer(nproc)
   numArgs <- length(input)
   supervisorWorkCount <- numArgs %/% nproc
   div <- (numArgs - supervisorWorkCount) %/% (nproc - 1)
   mod <- (numArgs - supervisorWorkCount) %% (nproc - 1)
   argBase[[1]] <- 1
   argLength[[1]] <- supervisorWorkCount

   # This is the stupid that results from indexing arrays beginning with 1
   if (mod == 0) {
      argBase[2 : nproc] <- (supervisorWorkCount + 1) + 
                            (0 : (nproc - 2)) * div
      argLength[2 : nproc] <- div
   } else {
      extraTerminus <- 2 + mod - 1
      argLength[2 : extraTerminus] <- div + 1
      argLength[(2 + mod) : nproc] <- div
      argBase[2 : extraTerminus] <- (supervisorWorkCount + 1) + 
                                  (0 : (mod - 1)) * (div + 1)
      argBase[(2 + mod) : nproc] <- argBase[[extraTerminus]] + 
                                  argLength[[extraTerminus]] +
                                  (0 : (nproc - mod - 2)) * div
   }
   pieces <- mapply(createSegment, argBase, argLength, 
      MoreArgs = list(input = input), SIMPLIFY = FALSE)
   serializeArgs <- lapply(pieces, serialize, NULL)
   return(serializeArgs)
}
