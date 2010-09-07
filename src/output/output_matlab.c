/*
 * Sally - A Tool for Embedding Strings in Vector Spaces
 * Copyright (C) 2010 Konrad Rieck (konrad@mlsec.org)
 * --
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.  This program is distributed without any
 * warranty. See the GNU General Public License for more details. 
 */

/** 
 * @addtogroup output 
 * Module 'matlab'.
 * <b>'matlab'</b>: The vectors are exported as a matlab file 
 * version 5. The vectors are stored in an 2 x n cell array, where
 * the first row holds the source of each vector and the second row
 * a sparse array containing the vector entries.
 * 
 * @author Konrad Rieck (konrad@mlsec.org)
 * @{
 */

#include "config.h"
#include "common.h"
#include "util.h"
#include "output.h"
#include "sally.h"
#include "fhash.h"
#include "output_matlab.h"

/* External variables */
extern config_t cfg;

/* Local variables */
static FILE *f = NULL;
static uint32_t bytes = 0;
static uint32_t elements = 0;
static int bits = 0;

/**
 * Pads the output stream
 * @param f File pointer
 */
static int fpad(FILE *f)
{
    int i, r = ftell(f) % 8;
    if (r != 0)
        r = 8 - r;

    for (i = 0; i < r; i++)
        fputc(0, f);
    
    return r;
}

/**
 * Writes a 16-bit integer to a file stream
 * @param i Integer value
 * @param f File pointer
 * @return number of bytes
 */
static int fwrite_uint16(uint16_t i, FILE *f)
{
    return fwrite(&i, 1, sizeof(i), f);
}

/**
 * Writes a 32-bit integer to a file stream
 * @param i Integer value
 * @param f File pointer
 * @return number of bytes
 */
static int fwrite_uint32(uint32_t i, FILE *f)
{
    return fwrite(&i, 1, sizeof(i), f);
}

/**
 * Writes a double to a file stream
 * @param i double value
 * @param f File pointer
 * @return number of bytes
 */
static int fwrite_double(double i, FILE *f)
{
    return fwrite(&i, 1, sizeof(i), f);
}

/**
 * Writes the dimensions of an array to a mat file
 * @param n Flags
 * @param c Class
 * @param z Non-zero elements
 * @param f File pointer
 * @return bytes written
 */
static int fwrite_array_flags(uint8_t n, uint8_t c, uint32_t z, FILE *f)
{
    fwrite_uint32(MAT_TYPE_UINT32, f);
    fwrite_uint32(8, f);
    fwrite_uint32(n << 16 | c, f);
    fwrite_uint32(z, f);

    return 16;
}

/**
 * Writes the dimensions of an array to a mat file
 * @param n Number of dimensions
 * @param m Number of dimensions
 * @param f File pointer
 * @return bytes written
 */
static int fwrite_array_dim(uint32_t n, uint32_t m, FILE *f)
{
    fwrite_uint32(MAT_TYPE_INT32, f);
    fwrite_uint32(8, f);
    fwrite_uint32(n, f);
    fwrite_uint32(m, f);

    return 16;
}
/**
 * Writes the name of an array to a mat file
 * @param n Name of arry
 * @param f File pointer
 * @return bytes written
 */
static int fwrite_array_name(char *n, FILE *f)
{
    int r, l = strlen(n);

    if (l <= 4) {
        /* Compressed format */
        fwrite_uint16(MAT_TYPE_INT8, f);
        fwrite_uint16(l, f);
        fwrite(n, l, 1, f);
        r = fpad(f);
        return 4 + l + r;
    } else {
        /* Regular format */
        fwrite_uint32(MAT_TYPE_INT8, f);
        fwrite_uint32(l, f);
        fwrite(n, l, 1, f);
        r = fpad(f);
        return 8 + l + r;
    }
}

/**
 * Opens a file for writing matlab format
 * @param fn File name
 * @return number of regular files
 */
int output_matlab_open(char *fn) 
{
    assert(fn);    
    int r = 0;

    config_lookup_int(&cfg, "features.hash_bits", (int *) &bits);
    if (bits > 31) {
        error("Matlab can not handle features with more than 31 bits");
        return FALSE;
    }

    f = fopen(fn, "w");
    if (!f) {
        error("Could not open output file '%s'.", fn);
        return FALSE;
    }
    
    /* Write matlab header */
    r += sally_version(f, "", "Output module for Matlab format (v5)");
    while (r < 124 && r > 0)
        r += fprintf(f, " ");

    /* Write version header */
    r += fwrite_uint16(0x0100, f);
    r += fwrite_uint16(0x4d49, f);

    if (r != 128) {
        error("Could not write header to output file '%s'.", fn);
        return FALSE;
    }

    /* Write tag of cell array */
    fwrite_uint32(MAT_TYPE_ARRAY, f);
    fwrite_uint32(0, f);   
 
    /* Here we go. Start a cell array */
    r = fwrite_array_flags(0, MAT_CLASS_CELL, 0, f);
    r += fwrite_array_dim(2, 0, f);
    r += fwrite_array_name("data", f);

    bytes = r;
    elements = 0;

    return TRUE;
}

/**
 * Writes a feature vector to a mat file
 * @param fv feature vector
 * @param f file pointer
 * @return number of bytes
 */
static int fwrite_fvec_data(fvec_t *fv, FILE *f)
{
    int r = 0, i;

    /* Tag */
    fwrite_uint32(MAT_TYPE_ARRAY, f);
    fwrite_uint32(0, f);

    /* Header */
    r += fwrite_array_flags(0, MAT_CLASS_SPARSE, fv->len, f);
    r += fwrite_array_dim(1 << bits, 1, f);
    r += fwrite_array_name("fvec", f);

    /* Row indices */
    r += fwrite_uint32(MAT_TYPE_INT32, f);
    r += fwrite_uint32(fv->len * sizeof(uint32_t), f);
    for (i = 0; i < fv->len; i++) 
        r += fwrite_uint32(fv->dim[i] & 0x7FFFFFFF, f);
    r += fpad(f);

    /* Column indices */
    r += fwrite_uint32(MAT_TYPE_INT32, f);
    r += fwrite_uint32(2 * sizeof(uint32_t), f);
    r += fwrite_uint32(0, f);
    r += fwrite_uint32(fv->len, f);

    /* Data  */
    r += fwrite_uint32(MAT_TYPE_DOUBLE, f);
    r += fwrite_uint32(fv->len * sizeof(double), f);
    for (i = 0; i < fv->len; i++) 
        r += fwrite_double(fv->val[i], f);
    r += fpad(f);
  
    /* Update size in tag */
    fseek(f, -(r + 4), SEEK_CUR);
    fwrite_uint32(r, f);
    fseek(f, r, SEEK_CUR);

    return r + 8;
}


/**
 * Writes the source of a feature vector to a mat file
 * @param fv feature vector
 * @param f file pointer
 * @return number of bytes
 */
static int fwrite_fvec_src(fvec_t *fv, FILE *f)
{
    int r = 0, l, i;
    if (fv->src)
        l = strlen(fv->src);
    else
        l = 0;

    /* Tag */
    fwrite_uint32(MAT_TYPE_ARRAY, f);
    fwrite_uint32(0, f);

    /* Header */
    r += fwrite_array_flags(0, MAT_CLASS_CHAR, 0, f);
    r += fwrite_array_dim(1, l, f);
    r += fwrite_array_name("src", f);
    r += fwrite_uint32(MAT_TYPE_UINT16, f);
    r += fwrite_uint32(l * 2, f);

    /* Write characters */
    for (i = 0; i < l ; i++)
        r += fwrite_uint16(fv->src[i], f);
    r += fpad(f);

    /* Update size in tag */
    fseek(f, -(r + 4), SEEK_CUR);
    fwrite_uint32(r, f);
    fseek(f, r, SEEK_CUR);

    return r + 8;
}

/**
 * Writes a block of files to the output
 * @param x Feature vectors
 * @param len Length of block
 * @return number of written files
 */
int output_matlab_write(fvec_t **x, int len)
{
    assert(x && len >= 0);
    int j;

    for (j = 0; j < len; j++) {
        bytes += fwrite_fvec_src(x[j], f);
        bytes += fwrite_fvec_data(x[j], f);
        elements++;
    }
    
    return TRUE;
}

/**
 * Closes an open output file.
 */
void output_matlab_close()
{
    if (!f)
        return;

    /* Fix number of bytes in header */
    fseek(f, 0x84, SEEK_SET);
    fwrite_uint32(bytes, f);

    /* Fix number of elements in header */
    fseek(f, 0xa4, SEEK_SET);
    fwrite_uint32(elements, f);

    fclose(f);
}

/** @} */
