#include <Rinternals.h>
#include "htslib/sam.h"

// https://stackoverflow.com/questions/26666614/how-do-i-check-if-an-externalptr-is-null-from-within-r
SEXP isnull(SEXP pointer) {
  return ScalarLogical(!R_ExternalPtrAddr(pointer));
}

// check if query position is within dist from 5' end of read
int trim_pos(bam1_t* b, int pos, int dist){
  // pos is 0-based
  if(!(b->core.flag&BAM_FPAIRED) || b->core.flag&(BAM_FREAD1)){
    if(pos < dist){
      return 1;
    }
  } else if (b->core.flag&(BAM_FREAD2)) {
    if((b->core.l_qseq - pos) <= dist){
      return 1;
    }
  } else {
    REprintf("don't believe should happen in trim_pos");
  }
  return 0;
}

int dist_to_splice(bam1_t* b, int pos, int dist){
  int n_cigar = b->core.n_cigar;
  const uint32_t *cigar = bam_get_cigar(b);

  int i, c, ip, sp, ep, idist;
  ip = 0;
  sp = pos + dist;
  ep = pos - dist;
  for (i = 0; i < n_cigar; i++){
    c = bam_cigar_op(cigar[i]);
    // is a M, I, S, =, or other query consuming operation
    if (bam_cigar_type(c)&1){
      ip += bam_cigar_oplen(cigar[i]);
      continue;
    }

    if(c == BAM_CREF_SKIP){
      if (ip >= sp && ip <= ep){
        idist = ip - pos;
        idist = (idist < 0) ? -idist : idist;
        Rprintf("edit_pos = %i; splice at pos: %i in read: %s\n", pos, ip, bam_get_qname(b)) ;
        return idist;
      }
    }
  }
  return 0;
}

int dist_to_indel(bam1_t* b, int pos, int dist){
  int n_cigar = b->core.n_cigar;
  const uint32_t *cigar = bam_get_cigar(b);

  int i, c, read_pos, indel_start, indel_end, sp, ep, ldist, rdist, idist;
  read_pos = indel_start = indel_end = 0;
  sp = pos + dist;
  ep = pos - dist;

  for (i = 0; i < n_cigar; i++){
    c = bam_cigar_op(cigar[i]);

    // is a M, I, S, =, or other query consuming operation
    if (bam_cigar_type(c)&1){
      if(c == BAM_CINS){
        indel_start = read_pos;
        indel_end = read_pos + bam_cigar_oplen(cigar[i]);
        //check range overlap
        if (sp <= indel_end && indel_start <= ep){
          ldist = pos - indel_end;
          rdist = indel_start - pos;
          idist = (ldist > rdist) ? ldist : rdist;
          Rprintf("edit_pos = %i; ins at pos: %i in read: %s\n", pos, idist, bam_get_qname(b)) ;
          return idist;
        }
      }
      read_pos += bam_cigar_oplen(cigar[i]);
    }

    if(c == BAM_CDEL){
      if (read_pos >= sp && read_pos <= ep){
        idist = read_pos - pos;
        idist = (idist < 0) ? -idist : idist;
        Rprintf("edit_pos = %i; del at pos: %i in read: %s\n", pos, read_pos, bam_get_qname(b)) ;
        return idist;
      }
    }
  }
  return 0;
}
