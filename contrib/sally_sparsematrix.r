# R function to load vectors extracted by Sally. The function takes a 
# file generated by the output module 'list' and returns a sparse matrix.  
# Copyright (c) 2010 Tammo Krueger (t.krueger@tu-berlin.de)
#
sally_sparsematrix = function(path) {
  require(Matrix)

  # Read in data generated by output module 'list'
  f = file(path)
  data = scan(f, what="char", sep=" ", quiet=TRUE, comment.char="", skip=3)
  close(f)

  # Extract basic fields
  rawfeats = data[c(TRUE, FALSE)]
  origin = data[c(FALSE, TRUE)]

  # Extract features 
  processFeat = function(cv) {
    ret = cv[3]
    names(ret) = cv[2]
    return(ret)
  }
  feats = lapply(strsplit(rawfeats, ",", fixed=TRUE), function(obj) 
                  sapply(strsplit(obj, ":", fixed=TRUE), processFeat))

  # Create list of all features and their indices
  allFeats = unique(unlist(lapply(feats, function(feat) names(feat)), 
    	                   use.names=FALSE))
  indices = unlist(lapply(feats, function(feat) match(names(feat), allFeats)), 
			  use.names=FALSE)

  # Generate a matrix in ml-style: rows are features, cols are samples
  mat = sparseMatrix(indices, rep(1:length(feats), sapply(feats, length)), 
                     x=as.numeric(unlist(feats, use.names=FALSE)), 
                     dims=c(length(allFeats), length(feats)), 
                     dimnames=list(allFeats, origin))

  return(mat)
}