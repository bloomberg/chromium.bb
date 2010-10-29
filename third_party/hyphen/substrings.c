//
// A utility for finding substring embeddings in patterns

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAXPATHS (256*1024)

//
//
static void die(
  const char*msg
) {
  fprintf(stderr,"%s\n",msg);
  exit(1);
}


// Finds the index of an entry, only used on xxx_key arrays
// Caveat: the table has to be sorted
static int find_in(
  char *tab[],
  int max,
  const char *pat
) {
  int left=0, right=max-1;
  while (left <= right) {
    int mid = ((right-left)/2)+left;
    int v   = strcmp(pat,tab[mid]);
    if (v>0) {
      left = mid + 1;
    } else if (v<0) {
      right = mid -1;
    } else {
      return mid;
    }
  }
  return -1;
}


// used by partition (which is used by qsort_arr)
//
static void swap2(
  char *a[],
  char *b[],
  int i,
  int j
) {
  if (i==j) return;
  char*v;
  v=a[i]; a[i]=a[j]; a[j]=v;
  v=b[i]; b[i]=b[j]; b[j]=v;
}


// used by qsort_arr
//
static int partition(
  char *a[],
  char *b[],
  int left,
  int right,
  int p
) {
  const char *pivotValue = a[p];
  int i;
  swap2(a,b,p,right); // Move pivot to end
  p = left;
  for (i=left; i<right; i++) {
    if (strcmp(a[i],pivotValue)<=0) {
      swap2(a,b,p,i);
      p++;
    }
  }
  swap2(a,b,right,p); // Move pivot to its final place
  return p;
}


//
//
static void qsort_arr(
  char *a[],
  char *b[],
  int left,
  int right
) {
  while (right > left) {
    int p = left + (right-left)/2; //select a pivot
    p = partition(a,b, left, right, p);
    if ((p-1) - left < right - (p+1)) {
      qsort_arr(a,b, left, p-1);
      left  = p+1;
    } else {
      qsort_arr(a,b, p+1, right);
      right = p-1;
    }
  }
}


// Removes extra '0' entries from the string
//
static char* compact(
  char *expr
) {
  int l=strlen(expr);
  int i,j;
  for (i=0,j=0; i<l; i++) {
    if (expr[i]!='0') expr[j++] = expr[i];
  }
  expr[j]=0;
  return expr;
}


// convert 'n1im' to 0n1i0m0 expressed as a string
//
static void expand(
  char *expr,
  const char *pat,
  int l
) {
  int  el = 0;
  char last = '.';
  int  i;
  for (i=0; i<l; i++) {
    char c = pat[i];
    if ( (last<'0' || last>'9')
      && (c   <'0' || c   >'9')
      ) {
      expr[el++] = '0';
    }
    expr[el++] = c;
    last = c;
  }
  if (last<'0' || last>'9') expr[el++] = '0';
  expr[el]=0;
}


// Combine two patterns, i.e. .ad4der + a2d becomes .a2d4der
// The second pattern needs to be a right side match of the first
// (modulo digits)
static char *combine(
  char *expr,
  const char *subexpr
) {
  int l1 = strlen(expr);
  int l2 = strlen(subexpr);
  int off = l1-l2;
  int j;
  // this works also for utf8 sequences because the substring is identical
  // to the last substring-length bytes of expr except for the (single byte)
  // hyphenation encoders
  for (j=0; j<l2; j++) {
    if (subexpr[j]>expr[off+j]) {
      expr[off+j] = subexpr[j];
    }
  }
  return expr;
}


//
//
int main(int argc, const char* argv[]) {
  FILE *in, *out;
  char *pattab_key[MAXPATHS];
  char *pattab_val[MAXPATHS];
  int   patterns = 0;
  char *newpattab_key[MAXPATHS];
  char *newpattab_val[MAXPATHS];
  int   newpatterns = 0;
  char format[132]; // 64+65+newline+zero+spare
  int p;
  if (argc!=3) die("Usage: <orig-file> <new-file>\n");
  if ((in = fopen(argv[1],"r"))==NULL) die("Could not read input");
  if ((out = fopen(argv[2],"w"))==NULL) die("Could not create output");
  // read all patterns and split in pure text (_key) & expanded patterns (_val)
  while(fgets(format,132,in) != NULL) {
    int l = strlen(format);
    if (format[l-1]=='\n') { l--; format[l]=0; } // Chomp
    if (format[0]=='%' || format[0]==0) {
      // skip
    } else {
      if (format[l-1]=='%') {
        l--;
        format[l] = 0; // remove '%'
      }
      int i,j;
      char *pat = (char*) malloc(l+1);
      char *org = (char*) malloc(l*2+1);
      if (pat==NULL || org==NULL) die("not enough memory");
      expand(org,format,l);
      // remove hyphenation encoders (digits) from pat
      for (i=0,j=0; i<l; i++) {
        // odd, but utf-8 proof
        char c = format[i];
        if (c<'0' || c>'9') pat[j++]=c;
      }
      pat[j]=0;
      p = patterns;
      pattab_key[patterns]   = pat;
      pattab_val[patterns++] = org;
      if (patterns>MAXPATHS) die("to many base patterns");
    }
  }
  fclose(in);
  // As we use binairy search, make sure it is sorted
  qsort_arr(pattab_key,pattab_val,0,patterns-1);

  for (p=0; p<patterns; p++) {
    char *pat = pattab_key[p];
    int   patsize = strlen(pat);
    int   j,l;
    for (l=1; l<=patsize; l++) {
      for (j=1; j<=l; j++) {
        int i = l-j;
        int  subpat_ndx;
        char subpat[132];
        strncpy(subpat,pat+i,j); subpat[j]=0;
        if ((subpat_ndx = find_in(pattab_key,patterns,subpat))>=0) {
          int   newpat_ndx;
          char *newpat=malloc(l+1);
          if (newpat==NULL) die("not enough memory");
      //printf("%s is embedded in %s\n",pattab_val[subpat_ndx],pattab_val[p]);
          strncpy(newpat, pat+0,l); newpat[l]=0;
          if ((newpat_ndx = find_in(newpattab_key,newpatterns,newpat))<0) {
            char *neworg = malloc(132); // TODO: compute exact length
            if (neworg==NULL) die("not enough memory");
            expand(neworg,newpat,l);
            newpattab_key[newpatterns]   = newpat;
            newpattab_val[newpatterns++] = combine(neworg,pattab_val[subpat_ndx]);
            if (newpatterns>MAXPATHS) die("to many new patterns");
    //printf("%*.*s|%*.*s[%s] (%s|%s) = %s\n",i,i,pat,j,j,pat+i,pat+i+j,pattab_val[p],pattab_val[subpat_ndx],neworg);
          } else {
            free(newpat);
            newpattab_val[newpat_ndx] = combine(
              newpattab_val[newpat_ndx], pattab_val[subpat_ndx] ); 
          }
        }
      }
    }
  }

  /* for some tiny extra speed, one could forget the free()s
   * as the memory is freed anyway on exit().
   * However, the gain is minimal and now the code can be cleanly
   * incorporated into other code */
  for (p=0; p<newpatterns; p++) {
    fprintf(out,"%s\n",compact(newpattab_val[p]));
    free(newpattab_key[p]);
    free(newpattab_val[p]);
  }
  fclose(out);

  for (p=0; p<patterns; p++) {
    free(pattab_key[p]);
    free(pattab_val[p]);
  }
  return 0;
}
