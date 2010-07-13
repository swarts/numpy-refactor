#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#define _MULTIARRAYMODULE
#define NPY_NO_PREFIX
#include "numpy/arrayobject.h"
#include "numpy/arrayscalars.h"
#include "numpy/numpy_api.h"

#include "npy_config.h"

#include "npy_3kcompat.h"

#include "common.h"
#include "mapping.h"

#include "sequence.h"

static int
array_any_nonzero(PyArrayObject *mp);

/*************************************************************************
 ****************   Implement Sequence Protocol **************************
 *************************************************************************/

/* Some of this is repeated in the array_as_mapping protocol.  But
   we fill it in here so that PySequence_XXXX calls work as expected
*/


static PyObject *
array_slice(PyArrayObject *self, Py_ssize_t ilow,
            Py_ssize_t ihigh)
{
    PyArrayObject *r;
    Py_ssize_t l;
    char *data;

    if (PyArray_NDIM(self) == 0) {
        PyErr_SetString(PyExc_ValueError, "cannot slice a 0-d array");
        return NULL;
    }

    l=PyArray_DIM(self, 0);
    if (ilow < 0) {
        ilow = 0;
    }
    else if (ilow > l) {
        ilow = l;
    }
    if (ihigh < ilow) {
        ihigh = ilow;
    }
    else if (ihigh > l) {
        ihigh = l;
    }

    if (ihigh != ilow) {
        data = index2ptr(self, ilow);
        if (data == NULL) {
            return NULL;
        }
    }
    else {
        data = PyArray_BYTES(self);
    }

    PyArray_DIM(self, 0) = ihigh-ilow;
    Py_INCREF(PyArray_DESCR(self));
    r = (PyArrayObject *)
        PyArray_NewFromDescr(Py_TYPE(self), PyArray_DESCR(self),
                             PyArray_NDIM(self), PyArray_DIMS(self),
                             PyArray_STRIDES(self), data,
                             PyArray_FLAGS(self), (PyObject *)self);
    PyArray_DIM(self, 0) = l;
    if (r == NULL) {
        return NULL;
    }
    PyArray_BASE_ARRAY(r) = PyArray_ARRAY(self);
    _Npy_INCREF(PyArray_BASE_ARRAY(r));
    assert(NULL == PyArray_BASE_ARRAY(r) || NULL == PyArray_BASE(r));
    PyArray_UpdateFlags(r, UPDATE_ALL);
    return (PyObject *)r;
}


static int
array_ass_slice(PyArrayObject *self, Py_ssize_t ilow,
                Py_ssize_t ihigh, PyObject *v) {
    int ret;
    PyArrayObject *tmp;

    if (v == NULL) {
        PyErr_SetString(PyExc_ValueError,
                        "cannot delete array elements");
        return -1;
    }
    if (!PyArray_ISWRITEABLE(self)) {
        PyErr_SetString(PyExc_RuntimeError,
                        "array is not writeable");
        return -1;
    }
    if ((tmp = (PyArrayObject *)array_slice(self, ilow, ihigh)) == NULL) {
        return -1;
    }
    ret = PyArray_CopyObject(tmp, v);
    Py_DECREF(tmp);

    return ret;
}

static int
array_contains(PyArrayObject *self, PyObject *el)
{
    /* equivalent to (self == el).any() */

    PyObject *res;
    int ret;

    res = PyArray_EnsureAnyArray(PyObject_RichCompare((PyObject *)self,
                                                      el, Py_EQ));
    if (res == NULL) {
        return -1;
    }
    ret = array_any_nonzero((PyArrayObject *)res);
    Py_DECREF(res);
    return ret;
}

NPY_NO_EXPORT PySequenceMethods array_as_sequence = {
#if PY_VERSION_HEX >= 0x02050000
    (lenfunc)array_length,                  /*sq_length*/
    (binaryfunc)NULL,                       /*sq_concat is handled by nb_add*/
    (ssizeargfunc)NULL,
    (ssizeargfunc)array_item_nice,
    (ssizessizeargfunc)array_slice,
    (ssizeobjargproc)array_ass_item,        /*sq_ass_item*/
    (ssizessizeobjargproc)array_ass_slice,  /*sq_ass_slice*/
    (objobjproc) array_contains,            /*sq_contains */
    (binaryfunc) NULL,                      /*sg_inplace_concat */
    (ssizeargfunc)NULL,
#else
    (inquiry)array_length,                  /*sq_length*/
    (binaryfunc)NULL,                       /*sq_concat is handled by nb_add*/
    (intargfunc)NULL,                       /*sq_repeat is handled nb_multiply*/
    (intargfunc)array_item_nice,            /*sq_item*/
    (intintargfunc)array_slice,             /*sq_slice*/
    (intobjargproc)array_ass_item,          /*sq_ass_item*/
    (intintobjargproc)array_ass_slice,      /*sq_ass_slice*/
    (objobjproc) array_contains,            /*sq_contains */
    (binaryfunc) NULL,                      /*sg_inplace_concat */
    (intargfunc) NULL                       /*sg_inplace_repeat */
#endif
};


/****************** End of Sequence Protocol ****************************/

/*
 * Helpers
 */

/* Array evaluates as "TRUE" if any of the elements are non-zero*/
static int
array_any_nonzero(PyArrayObject *mp)
{
    intp index;
    NpyArrayIterObject *it;
    Bool anyTRUE = FALSE;

    it = NpyArray_IterNew(PyArray_ARRAY(mp));
    if (it == NULL) {
        return anyTRUE;
    }
    index = it->size;
    while(index--) {
        if (PyArray_DESCR(mp)->f->nonzero(it->dataptr, mp)) {
            anyTRUE = TRUE;
            break;
        }
        NpyArray_ITER_NEXT(it);
    }
    _Npy_DECREF(it);
    return anyTRUE;
}

