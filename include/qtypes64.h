/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2024 rQuake contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

/*
 * qtypes64.h - 64-bit safe type definitions for QuakeC VM compatibility
 *
 * The QuakeC VM uses 32-bit integer offsets into memory regions (pr_strings,
 * sv.edicts). The progs.dat format is fixed at 32-bit. On 64-bit systems,
 * pointer arithmetic can produce values exceeding 32-bit range, causing
 * corruption. This header defines explicit 32-bit types for VM values.
 */

#ifndef QTYPES64_H
#define QTYPES64_H

#include <stdint.h>
#include <stddef.h>

/* VM types - MUST stay 32-bit for progs.dat compatibility */
typedef int32_t  func_t;      /* function index in progs */
typedef int32_t  string_t;    /* string offset from pr_strings */
typedef int32_t  pr_int_t;    /* QuakeC int in VM globals/entity fields */

/* Detect 64-bit compilation */
#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(__amd64__) || defined(__aarch64__)
  #define QUAKE_64BIT 1
#else
  #define QUAKE_64BIT 0
#endif

#endif /* QTYPES64_H */
