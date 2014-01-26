/*
  This code has been introduced by Bruce Moore <Bruce.Moore@akamail.com>
*/

/* Copyright (C) 1991 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

#include "ansidecl.h"
/*
#include <stdlib.h>
*/

#undef	rand
#undef __random

long int DEFUN_VOID(__random);


/* Return a random integer between 0 and RAND_MAX.  */
/* int
DEFUN_VOID(gnurand)
{
  return (int) __random();
}
*/


int gnurand () {
   return (int) __random(); }


