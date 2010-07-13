/*
  Provide multidimensional arrays as a basic object type in python.

  Based on Original Numeric implementation
  Copyright (c) 1995, 1996, 1997 Jim Hugunin, hugunin@mit.edu

  with contributions from many Numeric Python developers 1995-2004

  Heavily modified in 2005 with inspiration from Numarray

  by

  Travis Oliphant,  oliphant@ee.byu.edu
  Brigham Young Univeristy


maintainer email:  oliphant.travis@ieee.org

  Numarray design (which provided guidance) by
  Space Science Telescope Institute
  (J. Todd Miller, Perry Greenfield, Rick White)
*/
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

/*#include <stdio.h>*/
#define _MULTIARRAYMODULE
#define NPY_NO_PREFIX
#include "numpy/arrayobject.h"
#include "numpy/arrayscalars.h"
#include "numpy/numpy_api.h"

#include "npy_config.h"

#include "npy_3kcompat.h"

#include "common.h"

#include "number.h"
#include "usertypes.h"
#include "arraytypes.h"
#include "scalartypes.h"
#include "arrayobject.h"
#include "ctors.h"
#include "methods.h"
#include "descriptor.h"
#include "iterators.h"
#include "mapping.h"
#include "getset.h"
#include "sequence.h"
#include "buffer.h"

/*NUMPY_API
  Compute the size of an array (in number of items)
*/
NPY_NO_EXPORT intp
PyArray_Size(PyObject *op)
{
    return PyArray_Check(op) ? NpyArray_Size((NpyArray*)op) : 0;
}

/*NUMPY_API*/
NPY_NO_EXPORT int
PyArray_CopyObject(PyArrayObject *dest, PyObject *src_object)
{
    PyArrayObject *src;
    PyObject *r;
    int ret;

    /*
     * Special code to mimic Numeric behavior for
     * character arrays.
     */
    if (PyArray_DESCR(dest)->type == PyArray_CHARLTR && PyArray_NDIM(dest) > 0 \
        && PyString_Check(src_object)) {
        intp n_new, n_old;
        char *new_string;
        PyObject *tmp;

        n_new = PyArray_DIM(dest, PyArray_NDIM(dest)-1);
        n_old = PyString_Size(src_object);
        if (n_new > n_old) {
            new_string = (char *)malloc(n_new);
            memmove(new_string, PyString_AS_STRING(src_object), n_old);
            memset(new_string + n_old, ' ', n_new - n_old);
            tmp = PyString_FromStringAndSize(new_string, n_new);
            free(new_string);
            src_object = tmp;
        }
    }

    if (PyArray_Check(src_object)) {
        src = (PyArrayObject *)src_object;
        Py_INCREF(src);
    }
    else if (!PyArray_IsScalar(src_object, Generic) &&
             PyArray_HasArrayInterface(src_object, r)) {
        src = (PyArrayObject *)r;
    }
    else {
        PyArray_Descr* dtype;
        dtype = PyArray_DESCR(dest);
        Py_INCREF(dtype);
        src = (PyArrayObject *)PyArray_FromAny(src_object, dtype, 0,
                                               PyArray_NDIM(dest),
                                               FORTRAN_IF(dest),
                                               NULL);
    }
    if (src == NULL) {
        return -1;
    }

    ret = PyArray_MoveInto(dest, src);
    Py_DECREF(src);
    return ret;
}


/* returns an Array-Scalar Object of the type of arr
   from the given pointer to memory -- main Scalar creation function
   default new method calls this.
*/

/* Ideally, here the descriptor would contain all the information needed.
   So, that we simply need the data and the descriptor, and perhaps
   a flag
*/


/*
  Given a string return the type-number for
  the data-type with that string as the type-object name.
  Returns PyArray_NOTYPE without setting an error if no type can be
  found.  Only works for user-defined data-types.
*/

/*NUMPY_API
 */
NPY_NO_EXPORT int
PyArray_TypeNumFromName(char *str)
{
    int i;
    NpyArray_Descr *descr;
    PyTypeObject *type;
    
    for (i = 0; i < NPY_NUMUSERTYPES; i++) {
        descr = npy_userdescrs[i];
        type = (PyTypeObject *)descr->typeobj;
        if (strcmp(type->tp_name, str) == 0) {
            return descr->type_num;
        }
    }
    return NPY_NOTYPE;
}

/*********************** end C-API functions **********************/

/* array object functions */

static void
array_dealloc(PyArrayObject *self) {
    assert(NPY_VALID_MAGIC == self->magic_number);
    assert(NULL == PyArray_BASE_ARRAY(self) || NPY_VALID_MAGIC == PyArray_BASE_ARRAY(self)->magic_number);
    
    _array_dealloc_buffer_info(self);

    if (self->weakreflist != NULL) {
        PyObject_ClearWeakRefs((PyObject *)self);
    }
    
    /* TODO: Unwrap self into the array */
    NpyArray_dealloc(PyArray_ARRAY(self));
}

static int
dump_data(char **string, int *n, int *max_n, char *data, int nd,
          intp *dimensions, intp *strides, PyArrayObject* self)
{
    PyArray_Descr *descr=PyArray_DESCR(self);
    PyObject *op, *sp;
    char *ostring;
    intp i, N;

#define CHECK_MEMORY do { if (*n >= *max_n-16) {         \
        *max_n *= 2;                                     \
        *string = (char *)_pya_realloc(*string, *max_n); \
    }} while (0)

    if (nd == 0) {
        if ((op = descr->f->getitem(data, self)) == NULL) {
            return -1;
        }
        sp = PyObject_Repr(op);
        if (sp == NULL) {
            Py_DECREF(op);
            return -1;
        }
        ostring = PyString_AsString(sp);
        N = PyString_Size(sp)*sizeof(char);
        *n += N;
        CHECK_MEMORY;
        memmove(*string + (*n - N), ostring, N);
        Py_DECREF(sp);
        Py_DECREF(op);
        return 0;
    }
    else {
        CHECK_MEMORY;
        (*string)[*n] = '[';
        *n += 1;
        for (i = 0; i < dimensions[0]; i++) {
            if (dump_data(string, n, max_n,
                          data + (*strides)*i,
                          nd - 1, dimensions + 1,
                          strides + 1, self) < 0) {
                return -1;
            }
            CHECK_MEMORY;
            if (i < dimensions[0] - 1) {
                (*string)[*n] = ',';
                (*string)[*n+1] = ' ';
                *n += 2;
            }
        }
        CHECK_MEMORY;
        (*string)[*n] = ']';
        *n += 1;
        return 0;
    }

#undef CHECK_MEMORY
}

static PyObject *
array_repr_builtin(PyArrayObject *self, int repr)
{
    PyObject *ret;
    char *string;
    int n, max_n;

    max_n = PyArray_NBYTES(self)*4*sizeof(char) + 7;

    if ((string = (char *)_pya_malloc(max_n)) == NULL) {
        PyErr_SetString(PyExc_MemoryError, "out of memory");
        return NULL;
    }

    if (repr) {
        n = 6;
        sprintf(string, "array(");
    }
    else {
        n = 0;
    }
    if (dump_data(&string, &n, &max_n, PyArray_BYTES(self),
                  PyArray_NDIM(self), PyArray_DIMS(self),
                  PyArray_STRIDES(self), self) < 0) {
        _pya_free(string);
        return NULL;
    }

    if (repr) {
        if (PyArray_ISEXTENDED(self)) {
            char buf[100];
            PyOS_snprintf(buf, sizeof(buf), "%d", PyArray_ITEMSIZE(self));
            sprintf(string+n, ", '%c%s')", PyArray_DESCR(self)->type, buf);
            ret = PyUString_FromStringAndSize(string, n + 6 + strlen(buf));
        }
        else {
            sprintf(string+n, ", '%c')", PyArray_DESCR(self)->type);
            ret = PyUString_FromStringAndSize(string, n+6);
        }
    }
    else {
        ret = PyUString_FromStringAndSize(string, n);
    }

    _pya_free(string);
    return ret;
}

static PyObject *PyArray_StrFunction = NULL;
static PyObject *PyArray_ReprFunction = NULL;
static PyObject *PyArray_DatetimeParseFunction = NULL;

/*NUMPY_API
 * Set the array print function to be a Python function.
 */
NPY_NO_EXPORT void
PyArray_SetStringFunction(PyObject *op, int repr)
{
    if (repr) {
        /* Dispose of previous callback */
        Py_XDECREF(PyArray_ReprFunction);
        /* Add a reference to new callback */
        Py_XINCREF(op);
        /* Remember new callback */
        PyArray_ReprFunction = op;
    }
    else {
        /* Dispose of previous callback */
        Py_XDECREF(PyArray_StrFunction);
        /* Add a reference to new callback */
        Py_XINCREF(op);
        /* Remember new callback */
        PyArray_StrFunction = op;
    }
}

/*NUMPY_API
 * Set the date time print function to be a Python function.
 */
NPY_NO_EXPORT void
PyArray_SetDatetimeParseFunction(PyObject *op)
{
    /* Dispose of previous callback */
    Py_XDECREF(PyArray_DatetimeParseFunction);
    /* Add a reference to the new callback */
    Py_XINCREF(op);
    /* Remember new callback */
    PyArray_DatetimeParseFunction = op;
}


static PyObject *
array_repr(PyArrayObject *self)
{
    PyObject *s, *arglist;

    if (PyArray_ReprFunction == NULL) {
        s = array_repr_builtin(self, 1);
    }
    else {
        arglist = Py_BuildValue("(O)", self);
        s = PyEval_CallObject(PyArray_ReprFunction, arglist);
        Py_DECREF(arglist);
    }
    return s;
}

static PyObject *
array_str(PyArrayObject *self)
{
    PyObject *s, *arglist;

    if (PyArray_StrFunction == NULL) {
        s = array_repr_builtin(self, 0);
    }
    else {
        arglist = Py_BuildValue("(O)", self);
        s = PyEval_CallObject(PyArray_StrFunction, arglist);
        Py_DECREF(arglist);
    }
    return s;
}



/*NUMPY_API
 */
NPY_NO_EXPORT int
PyArray_CompareUCS4(npy_ucs4 *s1, npy_ucs4 *s2, size_t len)
{
    return NpyArray_CompareUCS4(s1, s2, len);
}

/*NUMPY_API
 */
NPY_NO_EXPORT int
PyArray_CompareString(char *s1, char *s2, size_t len)
{
    return NpyArray_CompareString(s1, s2, len);
}


/* This also handles possibly mis-aligned data */
/* Compare s1 and s2 which are not necessarily NULL-terminated.
   s1 is of length len1
   s2 is of length len2
   If they are NULL terminated, then stop comparison.
*/
static int
_myunincmp(PyArray_UCS4 *s1, PyArray_UCS4 *s2, int len1, int len2)
{
    PyArray_UCS4 *sptr;
    PyArray_UCS4 *s1t=s1, *s2t=s2;
    int val;
    intp size;
    int diff;

    if ((intp)s1 % sizeof(PyArray_UCS4) != 0) {
        size = len1*sizeof(PyArray_UCS4);
        s1t = malloc(size);
        memcpy(s1t, s1, size);
    }
    if ((intp)s2 % sizeof(PyArray_UCS4) != 0) {
        size = len2*sizeof(PyArray_UCS4);
        s2t = malloc(size);
        memcpy(s2t, s2, size);
    }
    val = PyArray_CompareUCS4(s1t, s2t, MIN(len1,len2));
    if ((val != 0) || (len1 == len2)) {
        goto finish;
    }
    if (len2 > len1) {
        sptr = s2t+len1;
        val = -1;
        diff = len2-len1;
    }
    else {
        sptr = s1t+len2;
        val = 1;
        diff=len1-len2;
    }
    while (diff--) {
        if (*sptr != 0) {
            goto finish;
        }
        sptr++;
    }
    val = 0;

 finish:
    if (s1t != s1) {
        free(s1t);
    }
    if (s2t != s2) {
        free(s2t);
    }
    return val;
}




/*
 * Compare s1 and s2 which are not necessarily NULL-terminated.
 * s1 is of length len1
 * s2 is of length len2
 * If they are NULL terminated, then stop comparison.
 */
static int
_mystrncmp(char *s1, char *s2, int len1, int len2)
{
    char *sptr;
    int val;
    int diff;

    val = memcmp(s1, s2, MIN(len1, len2));
    if ((val != 0) || (len1 == len2)) {
        return val;
    }
    if (len2 > len1) {
        sptr = s2 + len1;
        val = -1;
        diff = len2 - len1;
    }
    else {
        sptr = s1 + len2;
        val = 1;
        diff = len1 - len2;
    }
    while (diff--) {
        if (*sptr != 0) {
            return val;
        }
        sptr++;
    }
    return 0; /* Only happens if NULLs are everywhere */
}

/* Borrowed from Numarray */

#define SMALL_STRING 2048

#if defined(isspace)
#undef isspace
#define isspace(c)  ((c==' ')||(c=='\t')||(c=='\n')||(c=='\r')||(c=='\v')||(c=='\f'))
#endif

static void _rstripw(char *s, int n)
{
    int i;
    for (i = n - 1; i >= 1; i--) { /* Never strip to length 0. */
        int c = s[i];

        if (!c || isspace(c)) {
            s[i] = 0;
        }
        else {
            break;
        }
    }
}

static void _unistripw(PyArray_UCS4 *s, int n)
{
    int i;
    for (i = n - 1; i >= 1; i--) { /* Never strip to length 0. */
        PyArray_UCS4 c = s[i];
        if (!c || isspace(c)) {
            s[i] = 0;
        }
        else {
            break;
        }
    }
}


static char *
_char_copy_n_strip(char *original, char *temp, int nc)
{
    if (nc > SMALL_STRING) {
        temp = malloc(nc);
        if (!temp) {
            PyErr_NoMemory();
            return NULL;
        }
    }
    memcpy(temp, original, nc);
    _rstripw(temp, nc);
    return temp;
}

static void
_char_release(char *ptr, int nc)
{
    if (nc > SMALL_STRING) {
        free(ptr);
    }
}

static char *
_uni_copy_n_strip(char *original, char *temp, int nc)
{
    if (nc*sizeof(PyArray_UCS4) > SMALL_STRING) {
        temp = malloc(nc*sizeof(PyArray_UCS4));
        if (!temp) {
            PyErr_NoMemory();
            return NULL;
        }
    }
    memcpy(temp, original, nc*sizeof(PyArray_UCS4));
    _unistripw((PyArray_UCS4 *)temp, nc);
    return temp;
}

static void
_uni_release(char *ptr, int nc)
{
    if (nc*sizeof(PyArray_UCS4) > SMALL_STRING) {
        free(ptr);
    }
}


/* End borrowed from numarray */

#define _rstrip_loop(CMP) {                                     \
        void *aptr, *bptr;                                      \
        char atemp[SMALL_STRING], btemp[SMALL_STRING];          \
        while(size--) {                                         \
            aptr = stripfunc(iself->dataptr, atemp, N1);        \
            if (!aptr) return -1;                               \
            bptr = stripfunc(iother->dataptr, btemp, N2);       \
            if (!bptr) {                                        \
                relfunc(aptr, N1);                              \
                return -1;                                      \
            }                                                   \
            val = cmpfunc(aptr, bptr, N1, N2);                  \
            *dptr = (val CMP 0);                                \
            NpyArray_ITER_NEXT(iself);                          \
            NpyArray_ITER_NEXT(iother);                         \
            dptr += 1;                                          \
            relfunc(aptr, N1);                                  \
            relfunc(bptr, N2);                                  \
        }                                                       \
    }

#define _reg_loop(CMP) {                                \
        while(size--) {                                 \
            val = cmpfunc((void *)iself->dataptr,       \
                          (void *)iother->dataptr,      \
                          N1, N2);                      \
            *dptr = (val CMP 0);                        \
            NpyArray_ITER_NEXT(iself);                  \
            NpyArray_ITER_NEXT(iother);                 \
            dptr += 1;                                  \
        }                                               \
    }

#define _loop(CMP) if (rstrip) _rstrip_loop(CMP)        \
        else _reg_loop(CMP)

static int
_compare_strings(PyObject *result, NpyArrayMultiIterObject *multi,
                 int cmp_op, void *func, int rstrip)
{
    NpyArrayIterObject *iself, *iother;
    Bool *dptr;
    intp size;
    int val;
    int N1, N2;
    int (*cmpfunc)(void *, void *, int, int);
    void (*relfunc)(char *, int);
    char* (*stripfunc)(char *, char *, int);

    cmpfunc = func;
    dptr = (Bool *)PyArray_DATA(result);
    iself = multi->iters[0];
    iother = multi->iters[1];
    size = multi->size;
    N1 = iself->ao->descr->elsize;
    N2 = iother->ao->descr->elsize;
    if ((void *)cmpfunc == (void *)_myunincmp) {
        N1 >>= 2;
        N2 >>= 2;
        stripfunc = _uni_copy_n_strip;
        relfunc = _uni_release;
    }
    else {
        stripfunc = _char_copy_n_strip;
        relfunc = _char_release;
    }
    switch (cmp_op) {
    case Py_EQ:
        _loop(==)
            break;
    case Py_NE:
        _loop(!=)
            break;
    case Py_LT:
        _loop(<)
            break;
    case Py_LE:
        _loop(<=)
            break;
    case Py_GT:
        _loop(>)
            break;
    case Py_GE:
        _loop(>=)
            break;
    default:
        PyErr_SetString(PyExc_RuntimeError, "bad comparison operator");
        return -1;
    }
    return 0;
}

#undef _loop
#undef _reg_loop
#undef _rstrip_loop
#undef SMALL_STRING

NPY_NO_EXPORT PyObject *
_strings_richcompare(PyArrayObject *self, PyArrayObject *other, int cmp_op,
                     int rstrip)
{
    PyObject *result;
    NpyArrayMultiIterObject *mit;
    int val;

    /* Cast arrays to a common type */
    if (PyArray_TYPE(self) != PyArray_TYPE(other)) {
#if defined(NPY_PY3K)
        /*
         * Comparison between Bytes and Unicode is not defined in Py3K;
         * we follow.
         */
        result = Py_NotImplemented;
        Py_INCREF(result);
        return result;
#else
        PyObject *new;
        if (PyArray_TYPE(self) == PyArray_STRING &&
            PyArray_TYPE(other) == PyArray_UNICODE) {
            PyArray_Descr* unicode = PyArray_DescrNew(PyArray_DESCR(other));
            unicode->elsize = PyArray_ITEMSIZE(self) << 2;
            new = PyArray_FromAny((PyObject *)self, unicode,
                                  0, 0, 0, NULL);
            if (new == NULL) {
                return NULL;
            }
            Py_INCREF(other);
            self = (PyArrayObject *)new;
        }
        else if (PyArray_TYPE(self) == PyArray_UNICODE &&
                 PyArray_TYPE(other) == PyArray_STRING) {
            PyArray_Descr* unicode = PyArray_DescrNew(PyArray_DESCR(self));
            unicode->elsize = PyArray_ITEMSIZE(other) << 2;
            new = PyArray_FromAny((PyObject *)other, unicode,
                                  0, 0, 0, NULL);
            if (new == NULL) {
                return NULL;
            }
            Py_INCREF(self);
            other = (PyArrayObject *)new;
        }
        else {
            PyErr_SetString(PyExc_TypeError,
                            "invalid string data-types "
                            "in comparison");
            return NULL;
        }
#endif
    }
    else {
        Py_INCREF(self);
        Py_INCREF(other);
    }

    /* Broad-cast the arrays to a common shape */
    mit = NpyArray_MultiIterFromArrays(NULL, 0, 2, self, other);
    Py_DECREF(self);
    Py_DECREF(other);
    if (mit == NULL) {
        return NULL;
    }

    result = PyArray_NewFromDescr(&PyArray_Type,
                                  PyArray_DescrFromType(PyArray_BOOL),
                                  mit->nd,
                                  mit->dimensions,
                                  NULL, NULL, 0,
                                  NULL);
    if (result == NULL) {
        goto finish;
    }

    if (PyArray_TYPE(self) == PyArray_UNICODE) {
        val = _compare_strings(result, mit, cmp_op, _myunincmp, rstrip);
    }
    else {
        val = _compare_strings(result, mit, cmp_op, _mystrncmp, rstrip);
    }

    if (val < 0) {
        Py_DECREF(result); result = NULL;
    }

 finish:
    _Npy_DECREF(mit);
    return result;
}

/*
 * VOID-type arrays can only be compared equal and not-equal
 * in which case the fields are all compared by extracting the fields
 * and testing one at a time...
 * equality testing is performed using logical_ands on all the fields.
 * in-equality testing is performed using logical_ors on all the fields.
 *
 * VOID-type arrays without fields are compared for equality by comparing their
 * memory at each location directly (using string-code).
 */
static PyObject *
_void_compare(PyArrayObject *self, PyArrayObject *other, int cmp_op)
{
    if (!(cmp_op == Py_EQ || cmp_op == Py_NE)) {
        PyErr_SetString(PyExc_ValueError,
                "Void-arrays can only be compared for equality.");
        return NULL;
    }
    if (PyArray_HASFIELDS(self)) {
        const char *key;
        NpyArray_DescrField *value;
        PyObject *res = NULL, *temp, *a, *b;
        PyObject *temp2;
        PyObject *op, *tempKey;
        NpyDict_Iter pos;

        op = (cmp_op == Py_EQ ? n_ops.logical_and : n_ops.logical_or);
        NpyDict_IterInit(&pos);
        while (NpyDict_IterNext(PyArray_DESCR(self)->fields, &pos, (void **)&key, (void **)&value)) {
            if (NULL != value->title && !strcmp(value->title, key)) {
                continue;
            }
            tempKey = PyString_FromString(key);
            a = PyArray_EnsureAnyArray(array_subscript(self, tempKey));
            if (a == NULL) {
                Py_XDECREF(res);
                Py_DECREF(tempKey);
                return NULL;
            }
            b = array_subscript(other, tempKey);
            if (b == NULL) {
                Py_XDECREF(res);
                Py_DECREF(a);
                Py_DECREF(tempKey);
                return NULL;
            }
            Py_DECREF(tempKey);
            
            temp = array_richcompare((PyArrayObject *)a,b,cmp_op);
            Py_DECREF(a);
            Py_DECREF(b);
            if (temp == NULL) {
                Py_XDECREF(res);
                return NULL;
            }
            if (res == NULL) {
                res = temp;
            }
            else {
                temp2 = PyObject_CallFunction(op, "OO", res, temp);
                Py_DECREF(temp);
                Py_DECREF(res);
                if (temp2 == NULL) {
                    return NULL;
                }
                res = temp2;
            }
        }
        if (res == NULL && !PyErr_Occurred()) {
            PyErr_SetString(PyExc_ValueError, "No fields found.");
        }
        return res;
    }
    else {
        /*
         * compare as a string. Assumes self and
         * other have same descr->type
         */
        return _strings_richcompare(self, other, cmp_op, 0);
    }
}

NPY_NO_EXPORT PyObject *
array_richcompare(PyArrayObject *self, PyObject *other, int cmp_op)
{
    PyObject *array_other, *result = NULL;
    int typenum;

    switch (cmp_op) {
    case Py_LT:
        result = PyArray_GenericBinaryFunction(self, other,
                n_ops.less);
        break;
    case Py_LE:
        result = PyArray_GenericBinaryFunction(self, other,
                n_ops.less_equal);
        break;
    case Py_EQ:
        if (other == Py_None) {
            Py_INCREF(Py_False);
            return Py_False;
        }
        /* Try to convert other to an array */
        if (!PyArray_Check(other)) {
            typenum = PyArray_TYPE(self);
            if (typenum != PyArray_OBJECT) {
                typenum = PyArray_NOTYPE;
            }
            array_other = PyArray_FromObject(other,
                    typenum, 0, 0);
            /*
             * If not successful, then return False. This fixes code
             * that used to allow equality comparisons between arrays
             * and other objects which would give a result of False.
             */
            if ((array_other == NULL) ||
                    (array_other == Py_None)) {
                Py_XDECREF(array_other);
                PyErr_Clear();
                Py_INCREF(Py_False);
                return Py_False;
            }
        }
        else {
            Py_INCREF(other);
            array_other = other;
        }
        result = PyArray_GenericBinaryFunction(self,
                array_other,
                n_ops.equal);
        if ((result == Py_NotImplemented) &&
                (PyArray_TYPE(self) == PyArray_VOID)) {
            int _res;

            _res = PyObject_RichCompareBool
                ((PyObject *)PyArray_DESCR(self),
                 (PyObject *)\
                 PyArray_DESCR(array_other),
                 Py_EQ);
            if (_res < 0) {
                Py_DECREF(result);
                return NULL;
            }
            if (_res) {
                Py_DECREF(result);
                result = _void_compare
                    (self,
                      (PyArrayObject *)array_other,
                     cmp_op);
            }
            Py_DECREF(array_other);
            return result;
        }
        /*
         * If the comparison results in NULL, then the
         * two array objects can not be compared together so
         * return zero
         */
        Py_DECREF(array_other);
        if (result == NULL) {
            PyErr_Clear();
            Py_INCREF(Py_False);
            return Py_False;
        }
        break;
    case Py_NE:
        if (other == Py_None) {
            Py_INCREF(Py_True);
            return Py_True;
        }
        /* Try to convert other to an array */
        if (!PyArray_Check(other)) {
            typenum = PyArray_TYPE(self);
            if (typenum != PyArray_OBJECT) {
                typenum = PyArray_NOTYPE;
            }
            array_other = PyArray_FromObject(other, typenum, 0, 0);
            /*
             * If not successful, then objects cannot be
             * compared and cannot be equal, therefore,
             * return True;
             */
            if ((array_other == NULL) || (array_other == Py_None)) {
                Py_XDECREF(array_other);
                PyErr_Clear();
                Py_INCREF(Py_True);
                return Py_True;
            }
        }
        else {
            Py_INCREF(other);
            array_other = other;
        }
        result = PyArray_GenericBinaryFunction(self,
                array_other,
                n_ops.not_equal);
        if ((result == Py_NotImplemented) &&
                (PyArray_TYPE(self) == PyArray_VOID)) {
            int _res;

            _res = PyObject_RichCompareBool(
                    (PyObject *)PyArray_DESCR(self),
                    (PyObject *)
                    PyArray_DESCR(array_other),
                    Py_EQ);
            if (_res < 0) {
                Py_DECREF(result);
                Py_DECREF(array_other);
                return NULL;
            }
            if (_res) {
                Py_DECREF(result);
                result = _void_compare(
                        self,
                        (PyArrayObject *)array_other,
                        cmp_op);
                Py_DECREF(array_other);
            }
            return result;
        }

        Py_DECREF(array_other);
        if (result == NULL) {
            PyErr_Clear();
            Py_INCREF(Py_True);
            return Py_True;
        }
        break;
    case Py_GT:
        result = PyArray_GenericBinaryFunction(self, other,
                n_ops.greater);
        break;
    case Py_GE:
        result = PyArray_GenericBinaryFunction(self, other,
                n_ops.greater_equal);
        break;
    default:
        result = Py_NotImplemented;
        Py_INCREF(result);
    }
    if (result == Py_NotImplemented) {
        /* Try to handle string comparisons */
        if (PyArray_TYPE(self) == PyArray_OBJECT) {
            return result;
        }
        array_other = PyArray_FromObject(other,PyArray_NOTYPE, 0, 0);
        if (PyArray_ISSTRING(self) && PyArray_ISSTRING(array_other)) {
            Py_DECREF(result);
            result = _strings_richcompare(self, (PyArrayObject *)
                                          array_other, cmp_op, 0);
        }
        Py_DECREF(array_other);
    }
    return result;
}

/*NUMPY_API
 */
NPY_NO_EXPORT int
PyArray_ElementStrides(PyObject *arr)
{
    return NpyArray_ElementStrides((NpyArray*)arr);
}

/*
 * This routine checks to see if newstrides (of length nd) will not
 * ever be able to walk outside of the memory implied numbytes and offset.
 *
 * The available memory is assumed to start at -offset and proceed
 * to numbytes-offset.  The strides are checked to ensure
 * that accessing memory using striding will not try to reach beyond
 * this memory for any of the axes.
 *
 * If numbytes is 0 it will be calculated using the dimensions and
 * element-size.
 *
 * This function checks for walking beyond the beginning and right-end
 * of the buffer and therefore works for any integer stride (positive
 * or negative).
 */

/*NUMPY_API*/
NPY_NO_EXPORT Bool
PyArray_CheckStrides(int elsize, int nd, intp numbytes, intp offset,
                     intp *dims, intp *newstrides)
{
    return NpyArray_CheckStrides(elsize, nd, numbytes, offset,
                                 dims, newstrides);
}


static PyObject *
array_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"shape", "dtype", "buffer", "offset", "strides",
                             "order", NULL};
    PyArray_Descr *descr = NULL;
    int itemsize;
    PyArray_Dims dims = {NULL, 0};
    PyArray_Dims strides = {NULL, 0};
    PyArray_Chunk buffer;
    longlong offset = 0;
    NPY_ORDER order = PyArray_CORDER;
    int fortran = 0;
    PyArrayObject *ret;

    buffer.ptr = NULL;
    /*
     * Usually called with shape and type but can also be called with buffer,
     * strides, and swapped info For now, let's just use this to create an
     * empty, contiguous array of a specific type and shape.
     */
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&|O&O&LO&O&",
                                     kwlist, PyArray_IntpConverter,
                                     &dims,
                                     PyArray_DescrConverter,
                                     &descr,
                                     PyArray_BufferConverter,
                                     &buffer,
                                     &offset,
                                     &PyArray_IntpConverter,
                                     &strides,
                                     &PyArray_OrderConverter,
                                     &order)) {
        goto fail;
    }
    if (order == PyArray_FORTRANORDER) {
        fortran = 1;
    }
    if (descr == NULL) {
        descr = PyArray_DescrFromType(PyArray_DEFAULT);
    }

    itemsize = descr->elsize;
    if (itemsize == 0) {
        PyErr_SetString(PyExc_ValueError,
                        "data-type with unspecified variable length");
        goto fail;
    }

    if (strides.ptr != NULL) {
        intp nb, off;
        if (strides.len != dims.len) {
            PyErr_SetString(PyExc_ValueError,
                            "strides, if given, must be "   \
                            "the same length as shape");
            goto fail;
        }

        if (buffer.ptr == NULL) {
            nb = 0;
            off = 0;
        }
        else {
            nb = buffer.len;
            off = (intp) offset;
        }


        if (!PyArray_CheckStrides(itemsize, dims.len,
                                  nb, off,
                                  dims.ptr, strides.ptr)) {
            PyErr_SetString(PyExc_ValueError,
                            "strides is incompatible "      \
                            "with shape of requested "      \
                            "array and size of buffer");
            goto fail;
        }
    }

    if (buffer.ptr == NULL) {
        ret = (PyArrayObject *)
            PyArray_NewFromDescr(subtype, descr,
                                 (int)dims.len,
                                 dims.ptr,
                                 strides.ptr, NULL, fortran, NULL);
        if (ret == NULL) {
            descr = NULL;
            goto fail;
        }
        if (PyDataType_FLAGCHK(descr, NPY_ITEM_HASOBJECT)) {
            /* place Py_None in object positions */
            PyArray_FillObjectArray(ret, Py_None);
            if (PyErr_Occurred()) {
                descr = NULL;
                goto fail;
            }
        }
    }
    else {
        /* buffer given -- use it */
        if (dims.len == 1 && dims.ptr[0] == -1) {
            dims.ptr[0] = (buffer.len-(intp)offset) / itemsize;
        }
        else if ((strides.ptr == NULL) &&
                 (buffer.len < (offset + (((intp)itemsize)*
                                          PyArray_MultiplyList(dims.ptr, 
                                                               dims.len))))) {
            PyErr_SetString(PyExc_TypeError,
                            "buffer is too small for "      \
                            "requested array");
            goto fail;
        }
        /* get writeable and aligned */
        if (fortran) {
            buffer.flags |= FORTRAN;
        }
        ret = (PyArrayObject *)\
            PyArray_NewFromDescr(subtype, descr,
                                 dims.len, dims.ptr,
                                 strides.ptr,
                                 offset + (char *)buffer.ptr,
                                 buffer.flags, NULL);
        if (ret == NULL) {
            descr = NULL;
            goto fail;
        }
        PyArray_UpdateFlags(ret, UPDATE_ALL);
        
        /* TODO: This will be an issue.  Probably need to split buffer.base or we have to unwrap it. */
        if (PyArray_Check(buffer.base)) {
            PyArray_BASE_ARRAY(ret) = (NpyArray *)buffer.base;
            _Npy_INCREF(PyArray_BASE_ARRAY(ret));
        } else {
            PyArray_BASE(ret) = buffer.base;
            Py_INCREF(buffer.base);
        }
        assert(NULL == PyArray_BASE_ARRAY(ret)  || NULL == PyArray_BASE(ret));
    }

    PyDimMem_FREE(dims.ptr);
    if (strides.ptr) {
        PyDimMem_FREE(strides.ptr);
    }
    return (PyObject *)ret;

 fail:
    Py_XDECREF(descr);
    if (dims.ptr) {
        PyDimMem_FREE(dims.ptr);
    }
    if (strides.ptr) {
        PyDimMem_FREE(strides.ptr);
    }
    return NULL;
}


static PyObject *
array_iter(PyArrayObject *arr)
{
    if (PyArray_NDIM(arr) == 0) {
        PyErr_SetString(PyExc_TypeError,
                        "iteration over a 0-d array");
        return NULL;
    }
    return PySeqIter_New((PyObject *)arr);
}

static PyObject *
array_alloc(PyTypeObject *type, Py_ssize_t NPY_UNUSED(nitems))
{
    PyObject *obj;
    /* nitems will always be 0 */
    obj = (PyObject *)_pya_malloc(sizeof(PyArrayObject));
    PyObject_Init(obj, type);
    return obj;
}


NPY_NO_EXPORT PyTypeObject PyArray_Type = {
#if defined(NPY_PY3K)
    PyVarObject_HEAD_INIT(NULL, 0)
#else
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
#endif
    "numpy.ndarray",                            /* tp_name */
    sizeof(PyArrayObject),                      /* tp_basicsize */
    0,                                          /* tp_itemsize */
    /* methods */
    (destructor)array_dealloc,                  /* tp_dealloc */
    (printfunc)NULL,                            /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
#if defined(NPY_PY3K)
    0,                                          /* tp_reserved */
#else
    0,                                          /* tp_compare */
#endif
    (reprfunc)array_repr,                       /* tp_repr */
    &array_as_number,                           /* tp_as_number */
    &array_as_sequence,                         /* tp_as_sequence */
    &array_as_mapping,                          /* tp_as_mapping */
    (hashfunc)0,                                /* tp_hash */
    (ternaryfunc)0,                             /* tp_call */
    (reprfunc)array_str,                        /* tp_str */
    (getattrofunc)0,                            /* tp_getattro */
    (setattrofunc)0,                            /* tp_setattro */
    &array_as_buffer,                           /* tp_as_buffer */
    (Py_TPFLAGS_DEFAULT
#if !defined(NPY_PY3K)
     | Py_TPFLAGS_CHECKTYPES
#endif
#if (PY_VERSION_HEX >= 0x02060000) && (PY_VERSION_HEX < 0x03000000)
     | Py_TPFLAGS_HAVE_NEWBUFFER
#endif
     | Py_TPFLAGS_BASETYPE),                  /* tp_flags */
    0,                                          /* tp_doc */

    (traverseproc)0,                            /* tp_traverse */
    (inquiry)0,                                 /* tp_clear */
    (richcmpfunc)array_richcompare,             /* tp_richcompare */
    offsetof(PyArrayObject, weakreflist),       /* tp_weaklistoffset */
    (getiterfunc)array_iter,                    /* tp_iter */
    (iternextfunc)0,                            /* tp_iternext */
    array_methods,                              /* tp_methods */
    0,                                          /* tp_members */
    array_getsetlist,                           /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc)0,                                /* tp_init */
    array_alloc,                                /* tp_alloc */
    (newfunc)array_new,                         /* tp_new */
    0,                                          /* tp_free */
    0,                                          /* tp_is_gc */
    0,                                          /* tp_bases */
    0,                                          /* tp_mro */
    0,                                          /* tp_cache */
    0,                                          /* tp_subclasses */
    0,                                          /* tp_weaklist */
    0,                                          /* tp_del */
#if PY_VERSION_HEX >= 0x02060000
    0,                                          /* tp_version_tag */
#endif
};
