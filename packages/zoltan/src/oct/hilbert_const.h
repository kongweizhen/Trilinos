/*====================================================================
 * ------------------------
 * | CVS File Information |
 * ------------------------
 *
 * $RCSfile$
 *
 * $Author$
 *
 * $Date$
 *
 * $Revision$
 *
 *====================================================================*/
#ifndef __HILBERT_CONST_H
#define __HILBERT_CONST_H

#ifndef lint
static char *cvs_hilbertconsth_id = "$Id$";
#endif

/* Bits per unsigned word */

#define MaxBits ( sizeof(unsigned) * CHAR_BIT )
#define IScale  \
  ((unsigned)((MaxBits <= 32) ? ~(0u) : (0xffffffff << (MaxBits - 32))))

extern void hsfc2d(unsigned coord[], unsigned *nkey, unsigned key[]);
extern void hsfc3d(unsigned coord[], unsigned *nkey, unsigned key[]);
extern void fhsfc2d(double coord[], unsigned *nkey, unsigned key[]);
extern void fhsfc3d(double coord[], unsigned *nkey, unsigned key[]);

#endif
