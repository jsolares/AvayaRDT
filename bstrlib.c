/*
 * This source file is part of the bstring string library.  This code was
 * written by Paul Hsieh in 2002-2004, and is covered by the BSD open source 
 * license. Refer to the accompanying documentation for details on usage and 
 * license.
 */

/*
 * bstrlib.c
 *
 * This file is the core module for implementing the bstring functions.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "bstrlib.h"

/* Optionally include a mechanism for debugging memory */

#if defined(MEMORY_DEBUG) || defined(BSTRLIB_MEMORY_DEBUG)
#include "memdbg.h"
#endif

/* Just a length safe wrapper for memmove. */

#define bBlockCopy(D,S,L) { if ((L) > 0) memmove ((D),(S),(L)); }

/* Compute the snapped size for a given requested size.  By snapping to powers
   of 2 like this, repeated reallocations are avoided. */
static int snapUpSize (int i) {
	if (i < 8) {
		i = 8;
	} else {
		unsigned int j;
		j = i;

		/* Assumes your system is at least 32 bits, and your string is
		   at most 4GB is size. */
		j |= (j >>  1);
		j |= (j >>  2);
		j |= (j >>  4);
		j |= (j >>  8);
		j |= (j >> 16);
		i = j + 1;		/* Least power of two greater than i */
	}
	return i;
}

/*  int balloc (bstring b, int len)
 *
 *  Increase the size of the memory backing the bstring b to at least len.
 */
int balloc (bstring b, int len) {
	if (b == NULL || b->data == NULL || b->slen < 0 || b->mlen < 0 || 
	    b->mlen < b->slen || len <= 0) {
		return BSTR_ERR;
	}

	if (len >= b->mlen) {
		unsigned char * x;

		len = snapUpSize (len);

		/* Assume probability of a non-moving realloc is 0.125 */
		if (7 * b->mlen < 8 * b->slen) {

			/* If slen is close to mlen in size then use realloc to reduce
			   the memory defragmentation */

			x = (unsigned char *) realloc (b->data, len);
			if (x == NULL) return BSTR_ERR;
		} else {

			/* If slen is not close to mlen then avoid the penalty of copying
			   the extra bytes that are allocated, but not considered part of
			   the string */

			x = (unsigned char *) malloc (len);
			if (x == NULL) {
				x = (unsigned char *) realloc (b->data, len);
				if (x == NULL) return BSTR_ERR;
			} else {
				if (b->slen) memcpy ((char *) x, (char *) b->data, b->slen);
				free (b->data);
			}
		}
		b->data = x;
		b->mlen = len;
		b->data[b->slen] = '\0';
	}

	return BSTR_OK;
}

/*  bstring bfromcstr (const char * str)
 *
 *  Create a bstring which contains the contents of the '\0' terminated char *
 *  buffer str.
 */
bstring bfromcstr (const char * str) {
bstring b;
int i;
size_t j;

	if (str == NULL) return NULL;
	j = (strlen) (str);
	i = snapUpSize ((int) (j + (2 - (j != 0))));
	if (i <= (int) j) return NULL;

	b = (bstring) malloc (sizeof (struct tagbstring));
	if (NULL == b) return NULL;
	b->slen = j;
	if (NULL == (b->data = (unsigned char *) malloc (b->mlen = i))) {
		free (b);
		return NULL;
	}

	memcpy (b->data, str, j+1);
	return b;
}

/*  bstring bfromcstralloc (int len, const char * str)
 *
 *  Create a bstring which contains the contents of the '\0' terminated char *
 *  buffer str.  The memory buffer backing the string is at least len 
 *  characters in length.
 */
bstring bfromcstralloc (int len, const char * str) {
bstring b;
int i;
size_t j;

	if (str == NULL) return NULL;
	j = (strlen) (str);
	i = snapUpSize ((int) (j + (2 - (j != 0))));
	if (i <= (int) j) return NULL;

	b = (bstring) malloc (sizeof (struct tagbstring));
	if (b == NULL) return NULL;
	b->slen = j;
	if (i < len) i = len;

	if (NULL == (b->data = (unsigned char *) malloc (b->mlen = i))) {
		free (b);
		return NULL;
	}

	memcpy (b->data, str, j+1);
	return b;
}

/*  bstring blk2bstr (const void * blk, int len)
 *
 *  Create a bstring which contains the content of the block blk of length 
 *  len.
 */
bstring blk2bstr (const void * blk, int len) {
bstring b;
int i;

	if (blk == NULL || len < 0) return NULL;
	b = (bstring) malloc (sizeof (struct tagbstring));
	if (b == NULL) return NULL;
	b->slen = len;

	i = len + (2 - (len != 0));
	i = snapUpSize (i);

	b->mlen = i;

	b->data = (unsigned char *) malloc (b->mlen);
	if (b->data == NULL) {
		free (b);
		return NULL;
	}

	if (len > 0) memcpy (b->data, blk, len);
	b->data[len] = '\0';

	return b;
}

/*  char * bstr2cstr (const bstring s, char z)
 *
 *  Create a '\0' terminated char * buffer which is equal to the contents of 
 *  the bstring s, except that any contained '\0' characters are converted to 
 *  the character in z. This returned value should be freed with a free() 
 *  call, by the calling application.
 */
char * bstr2cstr (const bstring b, char z) {
int i,l;
char * r;

	if (b == NULL || b->slen < 0 || b->data == NULL) return NULL;
	l = b->slen;
	r = (char *) malloc (l + 1);
	if (r == NULL) return r;

	for (i=0; i < l; i ++) {
		r[i] = (char) ((b->data[i] == '\0') ? z : (char) (b->data[i]));
	}

	r[l] = '\0';

	return r;
}

/*  int bcstrfree (const char * s)
 *
 *  Frees a C-string generated by bstr2cstr ().  This is normally unnecessary
 *  since it just wraps a call to free (), however, if malloc () and free () 
 *  have been redefined as a macros within the bstrlib module (via defining 
 *  them in memdbg.h after defining BSTRLIB_MEMORY_DEBUG) with some 
 *  difference in behaviour from the std library functions, then this allows 
 *  a correct way of freeing the memory that allows higher level code to be 
 *  independent from these macro redefinitions.
 */
int bcstrfree (char * s) {
	if (s) {
		free (s);
		return BSTR_OK;
	}
	return BSTR_ERR;
}

/*  int bconcat (bstring b0, const bstring b1)
 *
 *  Concatenate the bstring b1 to the bstring b0.
 */
int bconcat (bstring b0, const bstring b1) {
int len, d;

	if (b0 == NULL || b1 == NULL) return BSTR_ERR;
	d = b0->slen;
	len = b1->slen;
	if ((d | (b0->mlen - d) | len) < 0) return BSTR_ERR;
	if (balloc (b0, d + len + 1) != BSTR_OK) return BSTR_ERR;
	bBlockCopy (&b0->data[d], &b1->data[0], len);
	b0->data[d + len] = '\0';
	b0->slen += len;
	return 0;
}

/*  int bconchar (bstring b, char c)
 *
 *  Concatenate the single character c to the bstring b.
 */
int bconchar (bstring b, char c) {
int d;

	if (b == NULL) return BSTR_ERR;
	d = b->slen;
	if ((d | (b->mlen - d)) < 0 || balloc (b, d + 2) != BSTR_OK) return BSTR_ERR;
	b->data[d] = (unsigned char) c;
	b->data[d + 1] = '\0';
	b->slen++;
	return 0;
}

/*  int bcatcstr (bstring b, const char * s)
 *
 *  Concatenate a char * string to a bstring.
 */
int bcatcstr (bstring b, const char * s) {
struct tagbstring t;
char * d;
int i, l;

	if (b == NULL || b->data == NULL || b->slen < 0 || s == NULL) return BSTR_ERR;

	/* Optimistically concatenate directly */
	l = b->mlen - b->slen;
	d = (char *) &b->data[b->slen];
	for (i=0; i < l; i++) {
		if ((*d++ = *s++) == '\0') {
			b->slen += i;
			return BSTR_OK;
		}
	}
	b->slen += i;

	/* Need to explicitely resize and concatenate tail */
	cstr2tbstr (t, s);
	return bconcat (b, &t);
}

/*  int bcatblk (bstring b, unsigned char * s, int len)
 *
 *  Concatenate a fixed length buffer to a bstring.
 */
int bcatblk (bstring b, const unsigned char * s, int len) {
struct tagbstring t;

	if (s == NULL || len < 0) return BSTR_ERR;
	blk2tbstr (t, s, len);
	return bconcat (b, &t);
}

/*  bstring bstrcpy (const bstring b)
 *
 *  Create a copy of the bstring b.
 */
bstring bstrcpy (const bstring b) {
bstring b0;
int i,j;

	/* Attempted to copy an invalid string? */
	if (b == NULL || b->slen < 0 || b->data == NULL) return NULL;

	b0 = (bstring) malloc (sizeof (struct tagbstring));
	if (b0 == NULL) {
		/* Unable to allocate memory for string header */
		return NULL;
	}

	i = b->slen;
	j = snapUpSize (i + 1);

	b0->data = (unsigned char *) malloc (j);
	if (b0->data == NULL) {
		j = i + 1;
		b0->data = (unsigned char *) malloc (j);
		if (b0->data == NULL) {
			/* Unable to allocate memory for string data */
			free (b0);
			return NULL;
		}
	}

	b0->mlen = j;
	b0->slen = i;

	if (i) memcpy ((char *) b0->data, (char *) b->data, i);
	b0->data[b0->slen] = '\0';

	return b0;
}

/*  int bassign (bstring a, const bstring b)
 *
 *  Overwrite the string a with the contents of string b.
 */
int bassign (bstring a, const bstring b) {
	if (b == NULL || b->data == NULL || b->slen < 0 ) 
		return BSTR_ERR;
	if (b->slen != 0) {
		if (balloc (a, b->slen) != BSTR_OK) return BSTR_ERR;
		memmove (a->data, b->data, b->slen);
	} else {
		if (a == NULL || a->data == NULL || a->mlen < a->slen || 
		    a->slen < 0 || a->mlen == 0) 
			return BSTR_ERR;
	}
	a->data[b->slen] = '\0';
	a->slen = b->slen;
	return BSTR_OK;
}

#define    ascii(c) ((unsigned char)(c) < 128)
#define   upcase(c) (ascii(c) ? toupper(c) : (c))
#define downcase(c) (ascii(c) ? tolower(c) : (c))

/*  int btoupper (bstring b)
 *
 *  Convert contents of bstring to upper case.
 */
int btoupper (bstring b) {
int i, len;
	if (b == NULL || b->data == NULL || b->mlen < b->slen ||
	    b->slen < 0) return BSTR_ERR;
	for (i=0, len = b->slen; i < len; i++) {
		b->data[i] = (unsigned char) upcase (b->data[i]);
	}
	return BSTR_OK;
}

/*  int btolower (bstring b)
 *
 *  Convert contents of bstring to lower case.
 */
int btolower (bstring b) {
int i, len;
	if (b == NULL || b->data == NULL || b->mlen < b->slen ||
	    b->slen < 0) return BSTR_ERR;
	for (i=0, len = b->slen; i < len; i++) {
		b->data[i] = (unsigned char) downcase (b->data[i]);
	}
	return 0;
}

/*  int bstricmp (const bstring b0, const bstring b1)
 *
 *  Compare two strings without differentiating between case.  The return 
 *  value is the difference of the values of the characters where the two 
 *  strings first differ, (taking a '\0' to be the character at the end of 
 *  any bstring) otherwise 0 is returned indicating that the strings are
 *  equal.
 */
int bstricmp (const bstring b0, const bstring b1) {
int i, v, n;

	if (bdata (b0) == NULL || b0->slen < 0 || 
	    bdata (b1) == NULL || b1->slen < 0) return SHRT_MIN;
	if ((n = b0->slen) > b1->slen) n = b1->slen;
	if (b0->slen == b1->slen && b0->data == b1->data) return 0;

	for (i = 0; i < n; i ++) {
		v  = (char) downcase (b0->data[i]);
		v -= (char) downcase (b1->data[i]);
		if (v != 0) return v;
		if (b0->data[i] == '\0') return 0;
	}

	if (b0->slen > n) return   (char) downcase (b0->data[n]);
	if (b1->slen > n) return - (char) downcase (b1->data[n]);
	return 0;
}

/*  int bstrnicmp (const bstring b0, const bstring b1, int n)
 *
 *  Compare two strings without differentiating between case for at most n
 *  characters.  If the position where the two strings first differ is
 *  before the nth position, the return value is the difference of the values
 *  of the characters, (taking a '\0' to be the character at the end of any
 *  bstring) otherwise 0 is returned.
 */
int bstrnicmp (const bstring b0, const bstring b1, int n) {
int i, v, m;

	if (bdata (b0) == NULL || b0->slen < 0 || 
	    bdata (b1) == NULL || b1->slen < 0 || n < 0) return SHRT_MIN;
	m = n;
	if (m > b0->slen) m = b0->slen;
	if (m > b1->slen) m = b1->slen;

	if (b0->data != b1->data) {
		for (i = 0; i < m; i ++) {
			v  = (char) downcase (b0->data[i]);
			v -= (char) downcase (b1->data[i]);
			if (v != 0) return v;
			if (b0->data[i] == '\0') return 0;
		}
	}

	if (n == m || b0->slen == b1->slen) return 0;

	if (b0->slen > m) return (char) downcase (b0->data[m]);
	return                 - (char) downcase (b1->data[m]);
}

/*  int biseqcaseless (const bstring b0, const bstring b1)
 *
 *  Compare two strings for equality without differentiating between case.  
 *  If the strings differ other than in case, 0 is returned, if the strings 
 *  are the same, 1 is returned, if there is an error, -1 is returned.  If 
 *  the length of the strings are different, this function is O(1).  '\0' 
 *  termination characters are not treated in any special way.
 */
int biseqcaseless (const bstring b0, const bstring b1) {
int i, n;

	if (bdata (b0) == NULL || b0->slen < 0 || 
	    bdata (b1) == NULL || b1->slen < 0) return BSTR_ERR;
	if (b0->slen != b1->slen) return 0;
	if (b0->data == b1->data || b0->slen == 0) return 1;
	for (i=0, n=b0->slen; i < n; i++) {
		if (b0->data[i] != b1->data[i]) {
			unsigned char c = (unsigned char) downcase (b0->data[i]);
			if (c != (unsigned char) downcase (b1->data[i])) return 0;
		}
	}
	return 1;
}

/*
 * int bltrimws (bstring b)
 *
 * Delete whitespace contiguous from the left end of the string.
 */
int bltrimws (bstring b) {
int i, len;

	if (b == NULL || b->data == NULL || b->mlen < b->slen ||
	    b->slen < 0) return BSTR_ERR;

	for (len = b->slen, i = 0; i < len; i++) {
		if (!isspace (b->data[i])) {
			return bdelete (b, 0, i);
		}
	}

	b->data[0] = '\0';
	b->slen = 0;
	return 0;
}

/*
 * int brtrimws (bstring b)
 *
 * Delete whitespace contiguous from the right end of the string.
 */
int brtrimws (bstring b) {
int i;

	if (b == NULL || b->data == NULL || b->mlen < b->slen ||
	    b->slen < 0) return BSTR_ERR;

	for (i = b->slen - 1; i >= 0; i--) {
		if (!isspace (b->data[i])) {
			if (b->mlen > i) b->data[i+1] = '\0';
			b->slen = i + 1;
			return 0;
		}
	}

	b->data[0] = '\0';
	b->slen = 0;
	return 0;
}

/*
 * int btrimws (bstring b)
 *
 * Delete whitespace contiguous from both ends of the string.
 */
int btrimws (bstring b) {
int i, j;

	if (b == NULL || b->data == NULL || b->mlen < b->slen ||
	    b->slen < 0) return BSTR_ERR;

	for (i = b->slen - 1; i >= 0; i--) {
		if (!isspace (b->data[i])) {
			if (b->mlen > i) b->data[i+1] = '\0';
			b->slen = i + 1;
			for (j = 0; isspace (b->data[j]); j++) {}
			return bdelete (b, 0, j);
		}
	}

	b->data[0] = '\0';
	b->slen = 0;
	return 0;
}

/*  int biseq (const bstring b0, const bstring b1)
 *
 *  Compare the string b0 and b1.  If the strings differ, 0 is returned, if 
 *  the strings are the same, 1 is returned, if there is an error, -1 is 
 *  returned.  If the length of the strings are different, this function is
 *  O(1).  '\0' termination characters are not treated in any special way.
 */
int biseq (const bstring b0, const bstring b1) {
	if (b0 == NULL || b1 == NULL || b0->data == NULL || b1->data == NULL ||
		b0->slen < 0 || b1->slen < 0) return BSTR_ERR;
	if (b0->slen != b1->slen) return 0;
	if (b0->data == b1->data || b0->slen == 0) return 1;
	return !memcmp (b0->data, b1->data, b0->slen);
}

/*  int biseqcstr (const bstring b, const char *s)
 *
 *  Compare the bstring b and char * string s.  The C string s must be '\0' 
 *  terminated at exactly the length of the bstring b, and the contents 
 *  between the two must be identical with the bstring b with no '\0' 
 *  characters for the two contents to be considered equal.  This is 
 *  equivalent to the condition that their current contents will be always be 
 *  equal when comparing them in the same format after converting one or the 
 *  other.  If the strings are equal 1 is returned, if they are unequal 0 is 
 *  returned and if there is a detectable error BSTR_ERR is returned.
 */
int biseqcstr (const bstring b, const char * s) {
int i;
	if (b == NULL || s == NULL || b->data == NULL || b->slen < 0) return BSTR_ERR;
	for (i=0; i < b->slen; i++) {
		if (s[i] == '\0' || b->data[i] != s[i]) return 0;
	}
	return s[i] == '\0';
}

/*  int bstrcmp (const bstring b0, const bstring b1)
 *
 *  Compare the string b0 and b1.  If there is an error, SHRT_MIN is returned, 
 *  otherwise a value less than or greater than zero, indicating that the 
 *  string pointed to by b0 is lexicographically less than or greater than 
 *  the string pointed to by b1 is returned.  If the the string lengths are 
 *  unequal but the characters up until the length of the shorter are equal 
 *  then a value less than, or greater than zero, indicating that the string 
 *  pointed to by b0 is shorter or longer than the string pointed to by b1 is 
 *  returned.  0 is returned if and only if the two strings are the same.  If 
 *  the length of the strings are different, this function is O(n).  Like its
 *  standard C library counter part strcmp, the comparison does not proceed 
 *  past any '\0' termination characters encountered.
 */
int bstrcmp (const bstring b0, const bstring b1) {
int i, v, n;

	if (b0 == NULL || b1 == NULL || b0->data == NULL || b1->data == NULL ||
		b0->slen < 0 || b1->slen < 0) return SHRT_MIN;
	n = b0->slen; if (n > b1->slen) n = b1->slen;
	if (b0->slen == b1->slen && (b0->data == b1->data || b0->slen == 0))
		return 0;

	for (i = 0; i < n; i ++) {
		v = ((char) b0->data[i]) - ((char) b1->data[i]);
		if (v != 0) return v;
		if (b0->data[i] == '\0') return 0;
	}

	if (b0->slen > n) return 1;
	if (b1->slen > n) return -1;
	return 0;
}

/*  int bstrncmp (const bstring b0, const bstring b1, int n)
 *
 *  Compare the string b0 and b1 for at most n characters.  If there is an 
 *  error, SHRT_MIN is returned, otherwise a value is returned as if b0 and 
 *  b1 were first truncated to at most n characters then bstrcmp was called
 *  with these new strings are paremeters.  If the length of the strings are 
 *  different, this function is O(n).  Like its standard C library counter 
 *  part strcmp, the comparison does not proceed past any '\0' termination 
 *  characters encountered.
 */
int bstrncmp (const bstring b0, const bstring b1, int n) {
int i, v, m;

	if (b0 == NULL || b1 == NULL || b0->data == NULL || b1->data == NULL ||
		b0->slen < 0 || b1->slen < 0) return SHRT_MIN;
	m = n;
	if (m > b0->slen) m = b0->slen;
	if (m > b1->slen) m = b1->slen;

	if (b0->data != b1->data) {
		for (i = 0; i < m; i ++) {
			v = ((char) b0->data[i]) - ((char) b1->data[i]);
			if (v != 0) return v;
			if (b0->data[i] == '\0') return 0;
		}
	}

	if (n == m || b0->slen == b1->slen) return 0;

	if (b0->slen > m) return 1;
	return -1;
}

/*  bstring bmidstr (const bstring b, int left, int len)
 *
 *  Create a bstring which is the substring of b starting from position left
 *  and running for a length len (clamped by the end of the bstring b.)  If
 *  b is detectably invalid, then NULL is returned.  The section described 
 *  by (left, len) is clamped to the boundaries of b.
 */
bstring bmidstr (const bstring b, int left, int len) {

	if (b == NULL || b->slen < 0 || b->data == NULL) return NULL;

	if (left < 0) {
		len += left;
		left = 0;
	}

	if (len > b->slen - left) len = b->slen - left;

	if (len <= 0) return bfromcstr ("");
	return blk2bstr (b->data + left, len);
}

/*  int bdelete (bstring b, int pos, int len)
 *
 *  Removes characters from pos to pos+len-1 inclusive and shifts the tail of 
 *  the bstring starting from pos+len to pos.  len must be positive for this 
 *  call to have any effect.  The section of the string described by (pos, 
 *  len) is clamped to boundaries of the bstring b.
 */
int bdelete (bstring b, int pos, int len) {
	/* Clamp to left side of bstring */
	if (pos < 0) {
		len += pos;
		pos = 0;
	}

	if (len < 0 || b == NULL || b->data == NULL || b->slen < 0 || b->mlen < b->slen) 
		return BSTR_ERR;
	if (len > 0 && pos < b->slen) {
		if (pos + len >= b->slen) {
			b->slen = pos;
		} else {
			bBlockCopy ((char *) (b->data + pos),
			            (char *) (b->data + pos + len), 
			            b->slen - (pos+len));
			b->slen -= len;
		}
		b->data[b->slen] = '\0';
	}
	return BSTR_OK;
}

/*  int bdestroy (bstring b)
 *
 *  free up the bstring.  Note that if b is detectably invalid or not writable
 *  then no action is performed and BSTR_ERR is returned.  Like a freed memory
 *  allocation, dereferences, writes or any other action on b after it has 
 *  been bdestroyed is undefined.
 */
int bdestroy (bstring b) {
	if (b == NULL || b->slen < 0 || b->mlen <= 0 || b->data == NULL)
		return BSTR_ERR;

	free (b->data);

	/* In case there is any stale usage, there is one more chance to 
	   notice this error. */

	b->slen = -1;
	b->mlen = -__LINE__;
	b->data = NULL;

	free (b);
	return 0;
}

/*  int binstr (const bstring b1, int pos, const bstring b2)
 *
 *  Search for the bstring b2 in b1 starting from position pos, and searching 
 *  forward.  If it is found then return with the first position where it is 
 *  found, otherwise return BSTR_ERR.  Note that this is just a brute force 
 *  string searcher that does not attempt clever things like the Boyer-Moore 
 *  search algorithm.  Because of this there are many degenerate cases where 
 *  this can take much longer than it needs to.
 */
int binstr (const bstring b1, int pos, const bstring b2) {
int j, i, l, ll;
unsigned char * d0, * d1;

	if (b1 == NULL || b1->data == NULL || b1->slen < 0 ||
	    b2 == NULL || b2->data == NULL || b2->slen < 0) return BSTR_ERR;
	if (b1->slen == pos) return (b2->slen == 0)?pos:BSTR_ERR;
	if (b1->slen < pos || pos < 0) return BSTR_ERR;
	if (b2->slen == 0) return pos;

	l = b1->slen - b2->slen + 1;

	/* No space to find such a string? */
	if (l <= pos) return BSTR_ERR;

	/* An obvious alias case */
	if (b1->data == b2->data && pos == 0) return 0;

	i = pos;
	j = 0;

	d0 = b2->data;
	d1 = b1->data;
	ll = b2->slen;

	for (;;) {
		if (d0[j] == d1[i + j]) {
			j ++;
			if (j >= ll) return i;
		} else {
			i ++;
			if (i >= l) break;
			j=0;
		}
	}

	return BSTR_ERR;
}

/*  int binstrr (const bstring b1, int pos, const bstring b2)
 *
 *  Search for the bstring b2 in b1 starting from position pos, and searching 
 *  backward.  If it is found then return with the first position where it is 
 *  found, otherwise return BSTR_ERR.  Note that this is just a brute force 
 *  string searcher that does not attempt clever things like the Boyer-Moore 
 *  search algorithm.  Because of this there are many degenerate cases where 
 *  this can take much longer than it needs to.
 */
int binstrr (const bstring b1, int pos, const bstring b2) {
int j, i, l;
unsigned char * d0, * d1;

	if (b1 == NULL || b1->data == NULL || b1->slen < 0 ||
	    b2 == NULL || b2->data == NULL || b2->slen < 0) return BSTR_ERR;
	if (b1->slen == pos && b2->slen == 0) return pos;
	if (b1->slen < pos || pos < 0) return BSTR_ERR;
	if (b2->slen == 0) return pos;

	/* Obvious alias case */
	if (b1->data == b2->data && pos == 0 && b2->slen <= b1->slen) return 0;

	i = pos;
	if ((l = b1->slen - b2->slen) < 0) return BSTR_ERR;

	/* If no space to find such a string then snap back */
	if (l + 1 <= i) i = l;
	j = 0;

	d0 = b2->data;
	d1 = b1->data;
	l  = b2->slen;

	for (;;) {
		if (d0[j] == d1[i + j]) {
			j ++;
			if (j >= l) return i;
		} else {
			i --;
			if (i < 0) break;
			j=0;
		}
	}

	return BSTR_ERR;
}

/*  int bstrchr (const bstring b, int c)
 *
 *  Search for the character c in b forwards from the start of the string.
 */
int bstrchr (const bstring b, int c) {
unsigned char * p;

	if (b == NULL || b->data == NULL || b->slen <= 0) return BSTR_ERR;
	p = (unsigned char *) memchr (b->data, (unsigned char) c, b->slen);
	if (p) return (int) (p - b->data);
	return BSTR_ERR;
}

/*  int bstrrchr (const bstring b, int c)
 *
 *  Search for the character c in b backwards from the end of the string.
 */
int bstrrchr (const bstring b, int c) {
int i;

	if (b == NULL || b->data == NULL || b->slen <= 0) return BSTR_ERR;
	for (i=b->slen-1; i >= 0; i--) {
		if (b->data[i] == (unsigned char) c) return i;
	}
	return BSTR_ERR;
}

#define LONG_LOG_BITS_QTY (5)
#define LONG_BITS_QTY (1 << LONG_LOG_BITS_QTY)
#define LONG_TYPE unsigned long

struct charField { LONG_TYPE content[(1 << CHAR_BIT) / LONG_BITS_QTY]; };
#define testInCharField(cf,c) ((cf)->content[(c) >> LONG_LOG_BITS_QTY] & (((long)1) << ((c) & (LONG_BITS_QTY-1))))

/* Convert a bstring to charField */
static int buildCharField (struct charField * cf, const bstring b1) {
int i;
	if (b1 == NULL || b1->data == NULL || b1->slen <= 0) return BSTR_ERR;
	memset ((void *) cf->content, 0, sizeof (struct charField));
	for (i=0; i < b1->slen; i++) {
		unsigned int c = (unsigned int) b1->data[i];
		cf->content[c >> LONG_LOG_BITS_QTY] |= ((LONG_TYPE) 1) << (c & (LONG_BITS_QTY-1));
	}
	return 0;
}

static void invertCharField (struct charField * cf) {
int i;
	for (i=0; i < ((1 << CHAR_BIT) / LONG_BITS_QTY); i++) cf->content[i] = ~cf->content[i];
}

/* Inner engine for binchr */
static int binchrCF (const unsigned char * data, int len, int pos, const struct charField * cf) {
int i;
	for (i=pos; i < len; i++) {
		unsigned int c = (unsigned int) data[i];
		if (testInCharField (cf, c)) return i;
	}
	return BSTR_ERR;
}

/*  int binchr (const bstring b0, int pos, const bstring b1);
 *
 *  Search for the first position in b0 starting from pos or after, in which 
 *  one of the characters in b1 is found and return it.  If such a position 
 *  does not exist in b0, then BSTR_ERR is returned.
 */
int binchr (const bstring b0, int pos, const bstring b1) {
struct charField chrs;
	if (pos < 0 || b0 == NULL || b0->data == NULL ||
	    b0->slen <= pos) return BSTR_ERR;
	if (buildCharField (&chrs, b1) < 0) return BSTR_ERR;
	return binchrCF (b0->data, b0->slen, pos, &chrs);
}

/* Inner engine for binchrr */
static int binchrrCF (const unsigned char * data, int pos, const struct charField * cf) {
int i;
	for (i=pos; i >= 0; i--) {
		unsigned int c = (unsigned int) data[i];
		if (testInCharField (cf, c)) return i;
	}
	return BSTR_ERR;
}

/*  int binchrr (const bstring b0, int pos, const bstring b1);
 *
 *  Search for the last position in b0 no greater than pos, in which one of 
 *  the characters in b1 is found and return it.  If such a position does not 
 *  exist in b0, then BSTR_ERR is returned.
 */
int binchrr (const bstring b0, int pos, const bstring b1) {
struct charField chrs;
	if (pos < 0 || b0 == NULL || b0->data == NULL ||
	    b0->slen < pos) return BSTR_ERR;
	if (pos == b0->slen) pos--;
	if (buildCharField (&chrs, b1) < 0) return BSTR_ERR;
	return binchrrCF (b0->data, pos, &chrs);
}

/*  int bninchr (const bstring b0, int pos, const bstring b1);
 *
 *  Search for the first position in b0 starting from pos or after, in which 
 *  none of the characters in b1 is found and return it.  If such a position 
 *  does not exist in b0, then BSTR_ERR is returned.
 */
int bninchr (const bstring b0, int pos, const bstring b1) {
struct charField chrs;
	if (pos < 0 || b0 == NULL || b0->data == NULL || 
	    b0->slen <= pos) return BSTR_ERR;
	if (buildCharField (&chrs, b1) < 0) return BSTR_ERR;
	invertCharField (&chrs);
	return binchrCF (b0->data, b0->slen, pos, &chrs);
}

/*  int bninchrr (const bstring b0, int pos, const bstring b1);
 *
 *  Search for the last position in b0 no greater than pos, in which none of 
 *  the characters in b1 is found and return it.  If such a position does not 
 *  exist in b0, then BSTR_ERR is returned.
 */
int bninchrr (const bstring b0, int pos, const bstring b1) {
struct charField chrs;
	if (pos < 0 || b0 == NULL || b0->data == NULL || 
	    b0->slen < pos) return BSTR_ERR;
	if (pos == b0->slen) pos--;
	if (buildCharField (&chrs, b1) < 0) return BSTR_ERR;
	invertCharField (&chrs);
	return binchrrCF (b0->data, pos, &chrs);
}

/*  int bsetstr (bstring b0, int pos, bstring b1, unsigned char fill)
 *
 *  Overwrite the string b0 starting at position pos with the string b1. If 
 *  the position pos is past the end of b0, then the character "fill" is 
 *  appended as necessary to make up the gap between the end of b0 and pos.
 *  If b1 is NULL, it behaves as if it were a 0-length string.
 */
int bsetstr (bstring b0, int pos, const bstring b1, unsigned char fill) {
int i, d, newlen;
	if (pos < 0 || b0 == NULL || b0->slen < 0 || b0->mlen < b0->slen) return BSTR_ERR;
	if (b1 != NULL && (b1->slen < 0 || b1->data == NULL)) return BSTR_ERR;

	/* Aliasing case */
	if ((b1 != NULL) && (b0->data == b1->data) && (pos < b0->slen)) {
		bstring tmp = bstrcpy (b1);
		if (tmp == NULL) return BSTR_ERR;
		d = bsetstr (b0, pos, tmp, fill);
		bdestroy (tmp);
		return d;
	}

	/* Increase memory size if necessary */
	d = pos + blength (b1);
	if (balloc (b0, d + 1) != BSTR_OK) return BSTR_ERR;

	newlen = b0->slen;

	/* Fill in "fill" character as necessary */
	if (pos > newlen) {
		for (i = b0->slen; i < pos; i ++) b0->data[i] = fill;
		newlen = pos;
	}

	/* Copy b1 to position pos in b0. */
	if (b1 != NULL) {
		bBlockCopy ((char *) (b0->data + pos), (char *) b1->data, b1->slen);
	}

	/* Indicate the potentially increased size of b0 */
	if (d > newlen) newlen = d;

	b0->slen = newlen;
	b0->data[newlen] = '\0';

	return BSTR_OK;
}

/*  int binsert (bstring b1, int pos, bstring b2, unsigned char fill)
 *
 *  Inserts the string b2 into b1 at position pos.  If the position pos is 
 *  past the end of b1, then the character "fill" is appended as necessary to 
 *  make up the gap between the end of b1 and pos.  Unlike bsetstr, binsert
 *  does not allow b2 to be NULL.
 */
int binsert (bstring b1, int pos, const bstring b2, unsigned char fill) {
int d, i, l;
ptrdiff_t pd;

	if (pos < 0 || b1 == NULL || b2 == NULL || b1->slen < 0 || 
	    b2->slen < 0 || b1->mlen < b1->slen) return BSTR_ERR;

	/* Aliasing case */
	if ((pd = (ptrdiff_t) (b2->data - b1->data)) >= 0 && pd < b1->slen) {
		bstring tmp;
		d = binsert (b1, pos, tmp = bstrcpy (b2), fill);
		bdestroy (tmp);
		return d;
	}

	/* Compute the two possible end pointers */
	d = b1->slen + b2->slen;
	l = pos + b2->slen;
	if ((d|l) < 0) return BSTR_ERR;

	if (l > d) {
		/* Inserting past the end of the string */
		if (balloc (b1, l + 1) != BSTR_OK) return BSTR_ERR;
		for (d = b1->slen; d < pos; d++) b1->data[d] = fill;
		b1->slen = l;
	} else {
		/* Inserting in the middle of the string */
		if (balloc (b1, d + 1) != BSTR_OK) return BSTR_ERR;
		for (i = d - 1; i >= l; i--) {
			b1->data[i] = b1->data[i - b2->slen];
		}
		b1->slen = d;
	}
	bBlockCopy (b1->data + pos, b2->data, b2->slen);
	b1->data[b1->slen] = '\0';
	return 0;
}

/*  int breplace (bstring b1, int pos, int len, bstring b2, 
 *                unsigned char fill)
 *
 *  Replace a section of a string from pos for a length len with the string b2.
 *  fill is used is pos > b1->slen.
 */
int breplace (bstring b1, int pos, int len, const bstring b2, 
			  unsigned char fill) {
int pl, ret;
ptrdiff_t pd;

	if (pos < 0 || len < 0 || (pl = pos + len) < 0 || b1 == NULL || 
		b2 == NULL || b1->data == NULL || b2->data == NULL || 
		b1->slen < 0 || b2->slen < 0 || b1->mlen < b1->slen) return BSTR_ERR;

	/* Straddles the end? */
	if (pl >= b1->slen) {
		if ((ret = bsetstr (b1, pos, b2, fill)) < 0) return ret;
		if (pos + b2->slen < b1->slen) {
			b1->slen = pos + b2->slen;
			b1->data[b1->slen] = '\0';
		}
		return ret;
	}

	/* Aliasing case */
	if ((pd = (ptrdiff_t) (b2->data - b1->data)) >= 0 && pd < b1->slen) {
		bstring tmp;
		ret = breplace (b1, pos, len, tmp = bstrcpy (b2), fill);
		bdestroy (tmp);
		return ret;
	}

	if (b2->slen > len) {
		if (balloc (b1, b1->slen + b2->slen - len) != BSTR_OK) return BSTR_ERR;
	}

	if (b2->slen != len) memmove (b1->data + pos + b2->slen, b1->data + pos + len, b1->slen - (pos + len));
	memcpy (b1->data + pos, b2->data, b2->slen);
	b1->slen += b2->slen - len;
	b1->data[b1->slen] = '\0';
	return BSTR_OK;
}

/*  int bfindreplace (bstring b, const bstring find, const bstring repl, 
 *                    int pos)
 *
 *  Replace all occurrences of a find string with a replace string after a
 *  given point in a bstring.
 */
int bfindreplace (bstring b, const bstring find, const bstring repl, int pos) {
int i, ret, slen, mlen, delta, acc;
int * d;
ptrdiff_t pd;

	if (b == NULL || b->data == NULL || find == NULL ||
	    find->data == NULL || repl == NULL || repl->data == NULL || 
	    pos < 0 || find->slen <= 0 || b->mlen < 0 || b->slen > b->mlen || 
	    b->slen < 0 || repl->slen < 0) return BSTR_ERR;
	if (pos > b->slen - find->slen) return 0;

	/* Alias with find string */
	pd = (ptrdiff_t) (find->data - b->data);
	if (pos - find->slen < pd && pd < b->slen) {
		bstring t;
		ret = bfindreplace (b, t = bstrcpy (find), repl, pos);
		bdestroy (t);
		return ret;
	}

	/* Alias with repl string */
	pd = (ptrdiff_t) (repl->data - b->data);
	if (pos - repl->slen < pd && pd < b->slen) {
		bstring t;
		ret = bfindreplace (b, find, t = bstrcpy (repl), pos);
		bdestroy (t);
		return ret;
	}

	delta = find->slen - repl->slen;

	/* in-place replacement since find and replace strings are of equal 
	   length */
	if (delta == 0) {
		while ((pos = binstr (b, pos, find)) >= 0) {
			memcpy (b->data + pos, repl->data, repl->slen);
			pos += find->slen;
		}
		return 0;
	}

	/* shrinking replacement since find->slen > repl->slen */
	if (delta > 0) {
		acc = 0;

		while ((i = binstr (b, pos, find)) >= 0) {
			if (acc && i > pos)
				memmove (b->data + pos - acc, b->data + pos, i - pos);
			if (repl->slen)
				memmove (b->data + i - acc, repl->data, repl->slen);
			acc += delta;
			pos = i + find->slen;
		}

		if (acc) {
			i = b->slen;
			if (i > pos)
				memmove (b->data + pos - acc, b->data + pos, i - pos);
			b->slen -= acc;
			b->data[b->slen] = '\0';
		}

		return 0;
	}

	/* expanding replacement since find->slen < repl->slen.  Its a lot 
	   more complicated. */

	mlen = 8;
	d = (int *) malloc (sizeof (int *) * mlen);
	if (d == NULL) return BSTR_ERR;
	acc = slen = 0;

	while ((pos = binstr (b, pos, find)) >= 0) {
		int * t, sl;
		if (slen + 1 >= mlen) {
			mlen += mlen;
			sl = sizeof (int *) * mlen;
			if (sl < mlen || NULL == (t = (int *) realloc (d, sl))) {
				free (d);
				return BSTR_ERR;
			}
			d = t;
		}
		d[slen] = pos;
		slen++;
		acc -= delta;
		pos += find->slen;
		if (pos < 0 || acc < 0) {
			free (d);
			return BSTR_ERR;
		}
	}
	d[slen] = b->slen;

	if (BSTR_OK == (ret = balloc (b, b->slen + acc + 1))) {
		b->slen += acc;
		for (i = slen-1; i >= 0; i--) {
			int s, l;
			s = d[i] + find->slen;
			l = d[i+1] - s;
			if (l) {
				memmove (b->data + s + acc, b->data + s, l);
			}
			if (repl->slen) {
				memmove (b->data + s + acc - repl->slen, 
				         repl->data, repl->slen);
			}
			acc += delta;		
		}
		b->data[b->slen] = '\0';
	}

	free (d);
	return ret;
}

/*  int binsertch (bstring b, int pos, int len, unsigned char fill)
 *
 *  Inserts the character fill repeatedly into b at position pos for a 
 *  length len.  If the position pos is past the end of b, then the 
 *  character "fill" is appended as necessary to make up the gap between the 
 *  end of b and the position pos + len.
 */
int binsertch (bstring b, int pos, int len, unsigned char fill) {
int d, l, i;

	if (pos < 0 || b == NULL || b->slen < 0 || b->mlen < b->slen ||
	    len < 0) return BSTR_ERR;

	/* Compute the two possible end pointers */
	d = b->slen + len;
	l = pos + len;
	if ((d|l) < 0) return BSTR_ERR;

	if (l > d) {
		/* Inserting past the end of the string */
		if (balloc (b, l + 1) != BSTR_OK) return BSTR_ERR;
		pos = b->slen;
		b->slen = l;
	} else {
		/* Inserting in the middle of the string */
		if (balloc (b, d + 1) != BSTR_OK) return BSTR_ERR;
		for (i = d - 1; i >= l; i--) {
			b->data[i] = b->data[i - len];
		}
		b->slen = d;
	}

	for (i=pos; i < l; i++) b->data[i] = fill;
	b->data[b->slen] = '\0';
	return 0;
}

/*  int bpattern (bstring b, int len)
 *
 *  Replicate the bstring, b in place, end to end repeatedly until it 
 *  surpasses len characters, then chop the result to exactly len characters. 
 *  This function operates in-place.  The function will return with BSTR_ERR 
 *  if b is NULL or of length 0, otherwise BSTR_OK is returned.
 */
int bpattern (bstring b, int len) {
int i, d;

	d = blength (b);
	if (d <= 0 || len < 0 || balloc (b, len + 1) != BSTR_OK) return BSTR_ERR;
	if (len > 0) {
		if (d == 1) return bsetstr (b, len, NULL, b->data[0]);
		for (i = d; i < len; i++) b->data[i] = b->data[i - d];
	}
	b->data[len] = '\0';
	b->slen = len;
	return 0;
}

#define BS_BUFF_SZ (1024)

/*  int breada (bstring b, bNread readPtr, void * parm)
 *
 *  Use a finite buffer fread-like function readPtr to concatenate to the 
 *  bstring b the entire contents of file-like source data in a roughly 
 *  efficient way.
 */
int breada (bstring b, bNread readPtr, void * parm) {
int i, l, n;

	if (b == NULL || b->mlen <= 0 || b->slen < 0 || b->mlen < b->slen ||
	    readPtr == NULL) return BSTR_ERR;

	i = b->slen;
	for (n=i+16; ; n += ((n < BS_BUFF_SZ) ? n : BS_BUFF_SZ)) {
		if (BSTR_OK != balloc (b, n + 1)) return BSTR_ERR;
		l = (int) readPtr ((void *) (b->data + i), 1, n - i, parm);
		i += l;
		b->slen = i;
		if (i < n) break;
	}

	b->data[i] = '\0';
	return 0;
}

/*  bstring bread (bNread readPtr, void * parm)
 *
 *  Use a finite buffer fread-like function readPtr to create a bstring 
 *  filled with the entire contents of file-like source data in a roughly 
 *  efficient way.
 */
bstring bread (bNread readPtr, void * parm) {
int ret;
bstring buff;

	if (readPtr == NULL) return NULL;
	buff = bfromcstr ("");
	if (buff == NULL) return NULL;
	ret = breada (buff, readPtr, parm);
	if (ret < 0) {
		bdestroy (buff);
		return NULL;
	}
	return buff;
}

/*  int bgetsa (bstring b, bNgetc getcPtr, void * parm, char terminator)
 *
 *  Use an fgetc-like single character stream reading function (getcPtr) to 
 *  obtain a sequence of characters which are concatenated to the end of the
 *  bstring b.  The stream read is terminated by the passed in terminator 
 *  function.
 *
 *  If getcPtr returns with a negative number, or the terminator character 
 *  (which is appended) is read, then the stream reading is halted and the 
 *  function returns with a partial result concatentated to b.  If no 
 *  characters are read, or there is some other detectable error, BSTR_ERR 
 *  is returned.
 */
int bgetsa (bstring b, bNgetc getcPtr, void * parm, char terminator) {
int c, d, e;

	if (b == NULL || b->mlen <= 0 || b->slen < 0 || b->mlen < b->slen ||
	    getcPtr == NULL) return BSTR_ERR;
	d = b->slen;
	e = b->mlen - 2;

	while ((c = getcPtr (parm)) >= 0) {
		if (d > e) {
			b->slen = d;
			if (balloc (b, d + 2) != BSTR_OK) return BSTR_ERR;
			e = b->mlen - 2;
		}
		b->data[d] = (unsigned char) c;
		d++;
		if (c == terminator) break;
	}

	b->data[d] = '\0';
	b->slen = d;

	return 0;
}

/*  bstring bgets (bNgetc getcPtr, void * parm, char terminator)
 *
 *  Use an fgetc-like single character stream reading function (getcPtr) to 
 *  obtain a sequence of characters which are concatenated into a bstring.  
 *  The stream read is terminated by the passed in terminator function.
 *
 *  If getcPtr returns with a negative number, or the terminator character 
 *  (which is appended) is read, then the stream reading is halted and the 
 *  result obtained thus far is returned.  If no characters are read, or 
 *  there is some other detectable error, NULL is returned.
 */
bstring bgets (bNgetc getcPtr, void * parm, char terminator) {
int ret;
bstring buff;

	if (getcPtr == NULL) return NULL;
	buff = bfromcstr ("");
	if (buff == NULL) return NULL;

	ret = bgetsa (buff, getcPtr, parm, terminator);
	if (ret < 0 || buff->slen <= 0) {
		bdestroy (buff);
		buff = NULL;
	}
	return buff;
}

struct bStream {
	bstring buff;		/* Buffer for over-reads */
	void * parm;		/* The stream handle for core stream */
	bNread readFnPtr;	/* fread compatible fnptr for core stream */
	int isEOF;		/* track file's EOF state */
	int maxBuffSz;
};

/*  struct bStream * bsopen (bNread readPtr, void * parm)
 *
 *  Wrap a given open stream (described by a fread compatible function 
 *  pointer and stream handle) into an open bStream suitable for the bstring 
 *  library streaming functions.
 */
struct bStream * bsopen (bNread readPtr, void * parm) {
struct bStream * s;

	if (readPtr == NULL) return NULL;
	s = (struct bStream *) malloc (sizeof (struct bStream));
	if (s == NULL) return NULL;
	s->parm = parm;
	s->buff = bfromcstr ("");
	s->readFnPtr = readPtr;
	s->maxBuffSz = BS_BUFF_SZ;
	s->isEOF = 0;
	return s;
}

/*  int bsbufflength (struct bStream * s, int sz)
 *
 *  Set the length of the buffer used by the bsStream.  If sz is zero, the 
 *  length is not set.  This function returns with the previous length.
 */
int bsbufflength (struct bStream * s, int sz) {
int oldSz;
	if (s == NULL || sz < 0) return BSTR_ERR;
	oldSz = s->maxBuffSz;
	if (sz > 0) s->maxBuffSz = sz;
	return oldSz;
}

int bseof (const struct bStream * s) {
	if (s == NULL || s->readFnPtr == NULL) return BSTR_ERR;
	return s->isEOF && (s->buff->slen == 0);
}

/*  void * bsclose (struct bStream * s)
 *
 *  Close the bStream, and return the handle to the stream that was originally
 *  used to open the given stream.
 */
void * bsclose (struct bStream * s) {
void * parm;
	if (s == NULL) return NULL;
	s->readFnPtr = NULL;
	if (s->buff) bdestroy (s->buff);
	s->buff = NULL;
	parm = s->parm;
	s->parm = NULL;
	s->isEOF = 1;
	free (s);
	return parm;
}

/*  int bsreadlna (bstring r, struct bStream * s, char terminator)
 *
 *  Read a bstring terminated by the terminator character or the end of the
 *  stream from the bStream (s) and return it into the parameter r.  This 
 *  function may read additional characters from the core stream that are not 
 *  returned, but will be retained for subsequent read operations.
 */
int bsreadlna (bstring r, struct bStream * s, char terminator) {
int i, l, ret;
char * b;
struct tagbstring x;

	if (s == NULL || s->buff == NULL || r == NULL || r->mlen <= 0)
		return BSTR_ERR;
	l = s->buff->slen;
	if (BSTR_OK != balloc (s->buff, s->maxBuffSz + 1)) return BSTR_ERR;
	b = (char *) s->buff->data;
	x.data = (unsigned char *) b;

	/* First check if the current buffer holds the terminator */
	b[l] = terminator;
	for (i=0; b[i] != terminator; i++) ;
	if (i < l) {
		x.slen = i + 1;
		ret = bconcat (r, &x);
		s->buff->slen = l;
		if (BSTR_OK == ret) bdelete (s->buff, 0, i + 1);
		return BSTR_ERR & -(r->slen == 0);
	}

	/* If not then just concatenate the entire buffer to the output */
	x.slen = l;
	if (BSTR_OK != bconcat (r, &x)) return BSTR_ERR;

	/* Perform direct in-place reads into the destination to allow for
	   the minimum of data-copies */
	for (;;) {
		if (BSTR_OK != balloc (r, r->slen + s->maxBuffSz + 1)) return BSTR_ERR;
		b = (char *) (r->data + r->slen);
		l = (int) s->readFnPtr (b, 1, s->maxBuffSz, s->parm);
		if (l <= 0) {
			r->data[r->slen] = '\0';
			s->buff->slen = 0;
			s->isEOF = 1;
			/* If nothing was read return with an error message */
			return BSTR_ERR & -(r->slen == 0);
		}
		b[l] = terminator;
		for (i=0; b[i] != terminator; i++) ;
		if (i < l) {
			/* Terminator found, push over-read back to buffer */
			i++;
			r->slen += i;
			s->buff->slen = l - i;
			memcpy (s->buff->data, b + i, l - i);
			r->data[r->slen] = '\0';
			return BSTR_OK;
		}
		r->slen += l;
	}
}

/*  int bsreadlnsa (bstring r, struct bStream * s, bstring term)
 *
 *  Read a bstring terminated by any character in the term string or the end 
 *  of the stream from the bStream (s) and return it into the parameter r.  
 *  This function may read additional characters from the core stream that 
 *  are not returned, but will be retained for subsequent read operations.
 */
int bsreadlnsa (bstring r, struct bStream * s, const bstring term) {
int i, l, ret;
unsigned char * b;
struct tagbstring x;
struct charField cf;

	if (s == NULL || s->buff == NULL || r == NULL || term == NULL 
	 || term->data == NULL || r->mlen <= 0) return BSTR_ERR;
	if (term->slen == 1) return bsreadlna (r, s, term->data[0]);
	if (term->slen < 1 || buildCharField (&cf, term)) return BSTR_ERR;

	l = s->buff->slen;
	if (BSTR_OK != balloc (s->buff, s->maxBuffSz + 1)) return BSTR_ERR;
	b = (unsigned char *) s->buff->data;
	x.data = b;

	/* First check if the current buffer holds the terminator */
	b[l] = term->data[0];
	for (i=0; !testInCharField (&cf, b[i]); i++) ;
	if (i < l) {
		x.slen = i + 1;
		ret = bconcat (r, &x);
		s->buff->slen = l;
		if (BSTR_OK == ret) bdelete (s->buff, 0, i + 1);
		return BSTR_ERR & -(r->slen == 0);
	}

	/* If not then just concatenate the entire buffer to the output */
	x.slen = l;
	if (BSTR_OK != bconcat (r, &x)) return BSTR_ERR;

	/* Perform direct in-place reads into the destination to allow for
	   the minimum of data-copies */
	for (;;) {
		if (BSTR_OK != balloc (r, r->slen + s->maxBuffSz + 1)) return BSTR_ERR;
		b = (unsigned char *) (r->data + r->slen);
		l = (int) s->readFnPtr (b, 1, s->maxBuffSz, s->parm);
		if (l <= 0) {
			r->data[r->slen] = '\0';
			s->buff->slen = 0;
			s->isEOF = 1;
			/* If nothing was read return with an error message */
			return BSTR_ERR & -(r->slen == 0);
		}

		b[l] = term->data[0];
		for (i=0; !testInCharField (&cf, b[i]); i++) ;
		if (i < l) {
			/* Terminator found, push over-read back to buffer */
			i++;
			r->slen += i;
			s->buff->slen = l - i;
			memcpy (s->buff->data, b + i, l - i);
			r->data[r->slen] = '\0';
			return BSTR_OK;
		}
		r->slen += l;
	}
}

/*  int bsreada (bstring r, struct bStream * s, int n)
 *
 *  Read a bstring of length n (or, if it is fewer, as many bytes as is 
 *  remaining) from the bStream.  This function may read additional 
 *  characters from the core stream that are not returned, but will be 
 *  retained for subsequent read operations.  This function will not read
 *  additional characters from the core stream beyond virtual stream pointer.
 */
int bsreada (bstring r, struct bStream * s, int n) {
int l, ret;
char * b;
struct tagbstring x;

	if (s == NULL || s->buff == NULL || r == NULL || r->mlen <= 0
	 || r->slen < 0 || n <= 0) return BSTR_ERR;

	n += r->slen;
	if (n <= 0) return BSTR_ERR;

	l = s->buff->slen;
	if (BSTR_OK != balloc (s->buff, s->maxBuffSz + 1)) return BSTR_ERR;
	b = (char *) s->buff->data;
	x.data = (unsigned char *) b;

	do {
		if (l + r->slen >= n) {
			x.slen = n - r->slen;
			ret = bconcat (r, &x);
			s->buff->slen = l;
			if (BSTR_OK == ret) bdelete (s->buff, 0, x.slen);
			return BSTR_ERR & -(r->slen == 0);
		}

		x.slen = l;
		if (BSTR_OK != bconcat (r, &x)) break;

		l = n - r->slen;
		if (l > s->maxBuffSz) l = s->maxBuffSz;

		l = (int) s->readFnPtr (b, 1, l, s->parm);

	} while (l > 0);
	if (l < 0) l = 0;
	if (l == 0) s->isEOF = 1;
	s->buff->slen = l;
	return BSTR_ERR & -(r->slen == 0);
}

/*  int bsreadln (bstring r, struct bStream * s, char terminator)
 *
 *  Read a bstring terminated by the terminator character or the end of the
 *  stream from the bStream (s) and return it into the parameter r.  This 
 *  function may read additional characters from the core stream that are not 
 *  returned, but will be retained for subsequent read operations.
 */
int bsreadln (bstring r, struct bStream * s, char terminator) {
	if (s == NULL || s->buff == NULL || r == NULL || r->mlen <= 0)
		return BSTR_ERR;
	if (BSTR_OK != balloc (s->buff, s->maxBuffSz + 1)) return BSTR_ERR;
	r->slen = 0;
	return bsreadlna (r, s, terminator);
}

/*  int bsreadlns (bstring r, struct bStream * s, bstring term)
 *
 *  Read a bstring terminated by any character in the term string or the end 
 *  of the stream from the bStream (s) and return it into the parameter r.  
 *  This function may read additional characters from the core stream that 
 *  are not returned, but will be retained for subsequent read operations.
 */
int bsreadlns (bstring r, struct bStream * s, const bstring term) {
	if (s == NULL || s->buff == NULL || r == NULL || term == NULL 
	 || term->data == NULL || r->mlen <= 0) return BSTR_ERR;
	if (term->slen == 1) return bsreadln (r, s, term->data[0]);
	if (term->slen < 1) return BSTR_ERR;
	if (BSTR_OK != balloc (s->buff, s->maxBuffSz + 1)) return BSTR_ERR;
	r->slen = 0;
	return bsreadlnsa (r, s, term);
}

/*  int bsread (bstring r, struct bStream * s, int n)
 *
 *  Read a bstring of length n (or, if it is fewer, as many bytes as is 
 *  remaining) from the bStream.  This function may read additional 
 *  characters from the core stream that are not returned, but will be 
 *  retained for subsequent read operations.  This function will not read
 *  additional characters from the core stream beyond virtual stream pointer.
 */
int bsread (bstring r, struct bStream * s, int n) {
	if (s == NULL || s->buff == NULL || r == NULL || r->mlen <= 0
	 || n <= 0) return BSTR_ERR;
	if (BSTR_OK != balloc (s->buff, s->maxBuffSz + 1)) return BSTR_ERR;
	r->slen = 0;
	return bsreada (r, s, n);
}

/*  int bsunread (struct bStream * s, const bstring b)
 *
 *  Insert a bstring into the bStream at the current position.  These 
 *  characters will be read prior to those that actually come from the core 
 *  stream.
 */
int bsunread (struct bStream * s, const bstring b) {
	if (s == NULL || s->buff == NULL) return BSTR_ERR;
	return binsert (s->buff, 0, b, '?');
}

/*  int bspeek (bstring r, const struct bStream * s)
 *
 *  Return the currently buffered characters from the bStream that will be 
 *  read prior to reads from the core stream.
 */
int bspeek (bstring r, const struct bStream * s) {
	if (s == NULL || s->buff == NULL || r == NULL || r->slen > r->mlen || r->slen < 0) return BSTR_ERR;
	r->slen = 0;
	return bconcat (r, s->buff);
}

/*  bstring bjoin (const struct bstrList * bl, const bstring sep);
 *
 *  Join the entries of a bstrList into one bstring by sequentially 
 *  concatenating them with the sep string in between.  If there is an error 
 *  NULL is returned, otherwise a bstring with the correct result is returned.
 */
bstring bjoin (const struct bstrList * bl, const bstring sep) {
bstring b;
int i, c, v;

	if (bl == NULL || bl->qty < 0) return NULL;
	if (sep != NULL && (sep->slen < 0 || sep->data == NULL)) return NULL;

	for (i = 0, c = 1; i < bl->qty; i++) {
		v = bl->entry[i]->slen;
		if (v < 0) return NULL;	/* Invalid input */
		c += v;
		if (c < 0) return NULL;	/* Wrap around ?? */
	}

	if (sep != NULL) c += (bl->qty - 1) * sep->slen;

	b = (bstring) malloc (sizeof (struct tagbstring));
	if (b == NULL) return NULL; /* Out of memory */
	b->data = (unsigned char *) malloc (c);
	if (b->data == NULL) {
		free (b);
		return NULL;
	}

	b->mlen = c;
	b->slen = c-1;

	for (i = 0, c = 0; i < bl->qty; i++) {
		if (i > 0 && sep != NULL) {
			memcpy (b->data + c, sep->data, sep->slen);
			c += sep->slen;
		}
		v = bl->entry[i]->slen;
		memcpy (b->data + c, bl->entry[i]->data, v);
		c += v;
	}
	b->data[c] = '\0';
	return b;
}

#define BSSSC_BUFF_LEN (256)

/*  int bssplitscb (struct bStream * s, const bstring splitStr, 
 *	int (* cb) (void * parm, int ofs, const bstring entry), void * parm)
 *
 *  Iterate the set of disjoint sequential substrings read from a stream 
 *  divided by any of the characters in splitStr.  An empty splitStr causes 
 *  the whole stream to be iterated once.
 *
 *  Note: At the point of calling the cb function, the bStream pointer is 
 *  pointed exactly at the position right after having read the split 
 *  character.  The cb function can act on the stream by causing the bStream
 *  pointer to move, and bssplitscb will continue by starting the next split
 *  at the position of the pointer after the return from cb.
 *
 *  However, if the cb causes the bStream s to be destroyed then the cb must
 *  return with a negative value, otherwise bssplitscb will continue in an 
 *  undefined manner.
 */
int bssplitscb (struct bStream * s, const bstring splitStr, 
	int (* cb) (void * parm, int ofs, const bstring entry), void * parm) {
struct charField chrs;
bstring buff;
int i, p, ret;

	if (cb == NULL || s == NULL || s->readFnPtr == NULL 
	 || splitStr == NULL || splitStr->slen < 0) return BSTR_ERR;

	if (NULL == (buff = bfromcstr (""))) return BSTR_ERR;

	if (splitStr->slen == 0) {
		while (bsreada (buff, s, BSSSC_BUFF_LEN) >= 0) ;
		if ((ret = cb (parm, 0, buff)) > 0) 
			ret = 0;
	} else {
		buildCharField (&chrs, splitStr);
		ret = p = i = 0;
		for (;;) {
			if (i >= buff->slen) {
				bsreada (buff, s, BSSSC_BUFF_LEN);
				if (i >= buff->slen) {
					if ((ret = cb (parm, p, buff)) > 0) ret = 0;
					break;
				}
			}
			if (testInCharField (&chrs, buff->data[i])) {
				struct tagbstring t;
				unsigned char c;
				blk2tbstr (t, buff->data + i + 1, buff->slen - (i + 1));
				if ((ret = bsunread (s, &t)) < 0) break;
				buff->slen = i;
				c = buff->data[i];
				buff->data[i] = '\0';
				if ((ret = cb (parm, p, buff)) < 0) break;
				buff->data[i] = c;
				buff->slen = 0;
				p += i + 1;
				i = -1;
			}
			i++;
		}
	}

	bdestroy (buff);
	return ret;
}

/*  int bstrListDestroy (struct bstrList * sl)
 *
 *  Destroy a bstrList that has been created by bsplit or bsplits.
 */
int bstrListDestroy (struct bstrList * sl) {
int i;
	if (sl == NULL || sl->qty < 0) return BSTR_ERR;
	for (i=0; i < sl->qty; i++) {
		if (sl->entry[i]) {
			bdestroy (sl->entry[i]);
			sl->entry[i] = NULL;
		}
	}
	sl->qty = -1;
	free (sl);
	return BSTR_OK;
}

/*  int bsplitcb (const bstring str, unsigned char splitChar, int pos,
 *	int (* cb) (void * parm, int ofs, int len), void * parm)
 *
 *  Iterate the set of disjoint sequential substrings over str divided by the
 *  character in splitChar.
 *
 *  Note: Non-destructive modification of str from within the cb function 
 *  while performing this split is not undefined.  bsplitcb behaves in 
 *  sequential lock step with calls to cb.  I.e., after returning from a cb 
 *  that return a non-negative integer, bsplitcb continues from the position 
 *  1 character after the last detected split character and it will halt 
 *  immediately if the length of str falls below this point.  However, if the 
 *  cb function destroys str, then it *must* return with a negative value, 
 *  otherwise bsplitcb will continue in an undefined manner.
 */
int bsplitcb (const bstring str, unsigned char splitChar, int pos,
	int (* cb) (void * parm, int ofs, int len), void * parm) {
int i, p, ret;

	if (cb == NULL || str == NULL || pos < 0 || pos > str->slen) 
		return BSTR_ERR;

	p = pos;
	do {
		for (i=p; i < str->slen; i++) {
			if (str->data[i] == splitChar) break;
		}
		if ((ret = cb (parm, p, i - p)) < 0) return ret;
		p = i + 1;
	} while (p < str->slen);
	return 0;
}

/*  int bsplitscb (const bstring str, const bstring splitStr, int pos,
 *	int (* cb) (void * parm, int ofs, int len), void * parm)
 *
 *  Iterate the set of disjoint sequential substrings over str divided by any 
 *  of the characters in splitStr.  An empty splitStr causes the whole str to
 *  be iterated once.
 *
 *  Note: Non-destructive modification of str from within the cb function 
 *  while performing this split is not undefined.  bsplitscb behaves in 
 *  sequential lock step with calls to cb.  I.e., after returning from a cb 
 *  that return a non-negative integer, bsplitscb continues from the position 
 *  1 character after the last detected split character and it will halt 
 *  immediately if the length of str falls below this point.  However, if the 
 *  cb function destroys str, then it *must* return with a negative value, 
 *  otherwise bsplitscb will continue in an undefined manner.
 */
int bsplitscb (const bstring str, const bstring splitStr, int pos,
	int (* cb) (void * parm, int ofs, int len), void * parm) {
struct charField chrs;
int i, p, ret;

	if (cb == NULL || str == NULL || pos < 0 || pos > str->slen 
	 || splitStr == NULL || splitStr->slen < 0) return BSTR_ERR;
	if (splitStr->slen == 0) {
		if ((ret = cb (parm, 0, str->slen)) > 0) ret = 0;
		return ret;
	}

	if (splitStr->slen == 1) 
		return bsplitcb (str, splitStr->data[0], pos, cb, parm);

	buildCharField (&chrs, splitStr);

	p = pos;
	do {
		for (i=p; i < str->slen; i++) {
			if (testInCharField (&chrs, str->data[i])) break;
		}
		if ((ret = cb (parm, p, i - p)) < 0) return ret;
		p = i + 1;
	} while (p < str->slen);
	return 0;
}

#ifdef offsetof
#define BLE_SZ (offsetof (struct bstrList, entry))
#else
#define BLE_SZ (sizeof (struct bstrList))
#endif

struct genBstrList {
	bstring b;
	struct bstrList * bl;
	int mlen;
};

static int bscb (void * parm, int ofs, int len) {
struct genBstrList * g = (struct genBstrList *)parm;
	if (g->bl->qty >= g->mlen) {
		int mlen = g->mlen * 2;
		struct bstrList * tbl;

		while (g->bl->qty >= mlen) mlen += mlen;
		tbl = (struct bstrList *) realloc (g->bl, BLE_SZ + sizeof (bstring) * mlen);
		if (tbl == NULL) return BSTR_ERR;
		g->bl = tbl;
		g->mlen = mlen;
	}

	g->bl->entry[g->bl->qty] = bmidstr (g->b, ofs, len);
	g->bl->qty++;
	return 0;
}

/*  struct bstrList * bsplit (const bstring str, unsigned char splitChar)
 *
 *  Create an array of sequential substrings from str divided by the character
 *  splitChar.  
 */
struct bstrList * bsplit (const bstring str, unsigned char splitChar) {
struct genBstrList g;

	if (str == NULL || str->data == NULL || str->slen < 0) return NULL;

	g.mlen = 4;
	g.bl = (struct bstrList *) malloc (BLE_SZ + sizeof (bstring) * g.mlen);
	if (g.bl == NULL) return NULL;
	g.b = str;
	g.bl->qty = 0;
	if (bsplitcb (str, splitChar, 0, bscb, &g) < 0) {
		bstrListDestroy (g.bl);
		return NULL;
	}
	return g.bl;
}

/*  struct bstrList * bsplits (const bstring str, bstring splitStr)
 *
 *  Create an array of sequential substrings from str divided by any of the 
 *  characters in splitStr.  An empty splitStr causes a single entry bstrList
 *  containing a copy of str to be returned.
 */
struct bstrList * bsplits (const bstring str, const bstring splitStr) {
struct genBstrList g;

	if (     str == NULL ||      str->slen < 0 ||      str->data == NULL ||
	    splitStr == NULL || splitStr->slen < 0 || splitStr->data == NULL)
		return NULL;

	g.mlen = 4;
	g.bl = (struct bstrList *) malloc (BLE_SZ + sizeof (bstring) * g.mlen);
	if (g.bl == NULL) return NULL;
	g.b = str;
	g.bl->qty = 0;
	if (bsplitscb (str, splitStr, 0, bscb, &g) < 0) {
		bstrListDestroy (g.bl);
		return NULL;
	}
	return g.bl;
}

#if defined (__TURBOC__) && !defined (__BORLANDC__)
# ifndef BSTRLIB_NOVSNP
#  define BSTRLIB_NOVSNP
# endif
#endif

/* Give WATCOM C/C++, MSVC some latitude for their non-support of vsnprintf */
#if defined(__WATCOMC__) || defined(_MSC_VER)
#define exvsnprintf(r,b,n,f,a) {r = _vsnprintf (b,n,f,a);}
#else
#ifdef BSTRLIB_NOVSNP
/* This is just a hack.  If you are using a system without a vsnprintf, it is 
   not recommended that bformat be used at all. */
#define exvsnprintf(r,b,n,f,a) {vsprintf (b,f,a); r = -1;}
#define START_VSNBUFF (256)
#else

#ifdef __GNUC__
/* Something is making gcc complain about this prototype not being here, so 
   I've just gone ahead and put it in. */
extern int vsnprintf (char *buf, size_t count, const char *format, va_list arg);
#endif

#define exvsnprintf(r,b,n,f,a) {r = vsnprintf (b,n,f,a);}
#endif
#endif

#if !defined (BSTRLIB_NOVSNP)

#ifndef START_VSNBUFF
#define START_VSNBUFF (16)
#endif

/* On IRIX vsnprintf returns n-1 when the operation would overflow the target 
   buffer, WATCOM and MSVC both return -1, while C99 requires that the 
   returned value be exactly what the length would be if the buffer would be
   large enough.  This leads to the idea that if the return value is larger 
   than n, then changing n to the return value will reduce the number of
   iterations required. */

/*  int bformata (bstring b, const char * fmt, ...)
 *
 *  After the first parameter, it takes the same parameters as printf (), but 
 *  rather than outputting results to stdio, it forms appends the results to 
 *  a bstring which contains what would have been output. Note that if there 
 *  is an early generation of a '\0' character, the bstring will be truncated 
 *  to this end point.
 */
int bformata (bstring b, const char * fmt, ...) {
va_list arglist;
bstring buff;
int n, r;

	if (b == NULL || fmt == NULL || b->data == NULL || b->mlen <= 0 
	 || b->slen < 0 || b->slen > b->mlen) return BSTR_ERR;

	/* Since the length is not determinable beforehand, a search is
	   performed using the truncating "vsnprintf" call (to avoid buffer
	   overflows) on increasing potential sizes for the output result. */

	if ((n = 2*strlen (fmt)) < START_VSNBUFF) n = START_VSNBUFF;
	buff = bfromcstr ("");

	for (;;) {
		if (BSTR_OK != balloc (buff, n + 2)) {
			bdestroy (buff);
			return BSTR_ERR;
		}

		va_start (arglist, fmt);
		exvsnprintf (r, (char *) buff->data, n + 1, fmt, arglist);
		va_end (arglist);

		buff->data[n] = '\0';
		buff->slen = (int) (strlen) ((char *) buff->data);

		if (buff->slen < n) break;

		if (r > n) n = r; else n += n;
	}

	r = bconcat (b, buff);
	bdestroy (buff);
	return r;
}

/*  bstring bformat (const char * fmt, ...)
 *
 *  Takes the same parameters as printf (), but rather than outputting results
 *  to stdio, it forms a bstring which contains what would have been output.
 *  Note that if there is an early generation of a '\0' character, the 
 *  bstring will be truncated to this end point.
 */
bstring bformat (const char * fmt, ...) {
va_list arglist;
bstring buff;
int n, r;

	if (fmt == NULL) return NULL;

	/* Since the length is not determinable beforehand, a search is
	   performed using the truncating "vsnprintf" call (to avoid buffer
	   overflows) on increasing potential sizes for the output result. */

	if ((n = 2*strlen (fmt)) < START_VSNBUFF) n = START_VSNBUFF;
	buff = bfromcstr ("");

	for (;;) {
		if (BSTR_OK != balloc (buff, n + 2)) {
			bdestroy (buff);
			return NULL;
		}

		va_start (arglist, fmt);
		exvsnprintf (r, (char *) buff->data, n + 1, fmt, arglist);
		va_end (arglist);

		buff->data[n] = '\0';
		buff->slen = (int) (strlen) ((char *) buff->data);

		if (buff->slen < n) break;

		if (r > n) n = r; else n += n;
	}

	return buff;
}

#endif
