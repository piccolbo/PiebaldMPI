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

addQuotes <- function(name) {
   listTerms <- sapply(name, function(x) {paste("'", x, "'", sep = '')} )
   if (length(listTerms) == 2) {
      return(paste(listTerms[1], ' and ', listTerms[2], sep = ''))
   } else if (length(listTerms) > 2) {
      return(paste(paste(listTerms[1:length(listTerms) - 1], collapse=', '),
         ', and ', listTerms[[length(listTerms)]], sep = ''))
   } else {
      return(listTerms)
   }
}

checkZeroDimensions <- function(a, b) {
   if ((length(a) == 0 && length(b) > 0) ||
      (length(a) > 0 && length(b) == 0)) {
      stop(paste("One of these has zero length:",
         addQuotes(paste(a, collapse = ' ')), 
         "and", addQuotes(paste(b, collapse = ' ')))) 	
   } else if (length(a) == 0 && length(b) == 0) {
      warning("Both values have zero length.  That's weird.")
   }
}

checkEqualDimensions <- function(a, b) {
   checkZeroDimensions(a, b)
   if((is.vector(a) && length(a) > 1 && !is.vector(b)) || 
      (is.vector(b) && length(b) > 1 && !is.vector(a))) {
      stop(paste(addQuotes(paste(a, collapse = ' ')), 
         "and", addQuotes(paste(b, collapse = ' ')), 
         "are not both vectors")) 	
   }
   if((is.matrix(a) && (nrow(a) > 1 || ncol(a) > 1) && !is.matrix(b)) || 
      (is.matrix(b) && (nrow(b) > 1 || ncol(b) > 1) && !is.matrix(a))) {
      stop(paste(addQuotes(paste(a, collapse = ' ')), 
      "and", addQuotes(paste(b, collapse = ' ')), 
      "are not both matrices")) 	
   }	
   if (is.vector(a) && (length(a) != length(b))) {
      stop(paste(addQuotes(paste(a, collapse = ' ')), 
         "and", addQuotes(paste(b, collapse = ' ')), 
         "do not have equal length :",
         length(a), 'and', length(b)))
   }
   if (is.matrix(a) && (nrow(a) > 1 || ncol(a) > 1) && any(dim(a) != dim(b))) {
      stop(paste(addQuotes(paste(a, collapse = ' ')), 
         "and", addQuotes(paste(b, collapse = ' ')), 
         "are not of equal dimension :", 
         paste(dim(a), collapse = ' x '), 'and', 
         paste(dim(b), collapse = ' x ')))
   }
}

checkIdentical <- function(a, b) {
   checkEqualDimensions(a, b)	
   if (any(!identical(a, b))) {
      stop(paste(addQuotes(paste(a, collapse = ' ')), 
         "and", addQuotes(paste(b, collapse = ' ')), 
         "are not identical"))
   }
   cat(paste(deparse(match.call()$a), "and", 
      deparse(match.call()$b),
      "are identical.\n"))	
}

checkTrue <- function(a) {	
   call <- deparse(match.call()$a)
   if (any(!a)) {
      stop(paste(call, "is not true"))
   }
   cat(paste(call, "is true.", '\n'))
}
