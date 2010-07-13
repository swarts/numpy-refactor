
#define _MULTIARRAYMODULE
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "npy_config.h"
#include "numpy/numpy_api.h"

/*
 * Compute the size of an array (in number of items)
 */
npy_intp
NpyArray_Size(NpyArray *op)
{
    return NpyArray_SIZE(op);
}

int
NpyArray_CompareUCS4(npy_ucs4 *s1, npy_ucs4 *s2, size_t len)
{
    npy_ucs4 c1, c2;
    while(len-- > 0) {
        c1 = *s1++;
        c2 = *s2++;
        if (c1 != c2) {
            return (c1 < c2) ? -1 : 1;
        }
    }
    return 0;
}

int
NpyArray_CompareString(char *s1, char *s2, size_t len)
{
    const unsigned char *c1 = (unsigned char *)s1;
    const unsigned char *c2 = (unsigned char *)s2;
    size_t i;

    for(i = 0; i < len; ++i) {
        if (c1[i] != c2[i]) {
            return (c1[i] > c2[i]) ? 1 : -1;
        }
    }
    return 0;
}

int
NpyArray_ElementStrides(NpyArray *arr)
{
    int itemsize = NpyArray_ITEMSIZE(arr);
    int i, N = NpyArray_NDIM(arr);
    npy_intp *strides = PyArray_STRIDES(arr);

    for (i = 0; i < N; i++) {
        if ((strides[i] % itemsize) != 0) {
            return 0;
        }
    }
    return 1;
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
npy_bool
NpyArray_CheckStrides(int elsize, int nd, npy_intp numbytes, npy_intp offset,
                      npy_intp *dims, npy_intp *newstrides)
{
    int i;
    npy_intp byte_begin;
    npy_intp begin;
    npy_intp end;

    if (numbytes == 0) {
        numbytes = NpyArray_MultiplyList(dims, nd) * elsize;
    }
    begin = -offset;
    end = numbytes - offset - elsize;
    for (i = 0; i < nd; i++) {
        byte_begin = newstrides[i]*(dims[i] - 1);
        if ((byte_begin < begin) || (byte_begin > end)) {
            return NPY_FALSE;
        }
    }
    return NPY_TRUE;
}




/* Deallocs & destroy's the array object. */
/* TODO: For now caller is expected to call _array_dealloc_buffer_info and clear weak refs.  Need to revisit. */
void
NpyArray_dealloc(NpyArray *self) {
    assert(NPY_VALID_MAGIC == self->magic_number);
    assert(NULL == self->base_arr || NPY_VALID_MAGIC == self->base_arr->magic_number);
    
    if (self->base_arr) {
        /*
         * UPDATEIFCOPY means that base points to an
         * array that should be updated with the contents
         * of this array upon destruction.
         * self->base->flags must have been WRITEABLE
         * (checked previously) and it was locked here
         * thus, unlock it.
         */
        if (self->flags & NPY_UPDATEIFCOPY) {
            self->base_arr->flags |= NPY_WRITEABLE;
            _Npy_INCREF(self); /* hold on to self in next call */
            if (NpyArray_CopyAnyInto(self->base_arr, self) < 0) {
                NpyErr_Print();
                NpyErr_Clear();
            }
            /*
             * Don't need to DECREF -- because we are deleting
             *self already...
             */
        }
        /*
         * In any case base is pointing to something that we need
         * to DECREF -- either a view or a buffer object
         */
        _Npy_DECREF(self->base_arr);
        self->base_arr = NULL;
    } else if (NULL != self->base_obj) {
        Npy_Interface_DECREF(self->base_obj);
        self->base_obj = NULL;
    }
    
    if ((self->flags & NPY_OWNDATA) && self->data) {
        /* Free internal references if an Object array */
        if (NpyDataType_FLAGCHK(self->descr, NPY_ITEM_REFCOUNT)) {
            _Npy_INCREF(self); /*hold on to self */
            /* TODO: We need make this a core level call. */
            PyArray_XDECREF(Npy_INTERFACE(self));
            /*
             * Don't need to DECREF -- because we are deleting
             * self already...
             */
        }
        NpyDataMem_FREE(self->data);
    }
    
    NpyDimMem_FREE(self->dimensions);
    Npy_DECREF(self->descr);
    self->magic_number = NPY_INVALID_MAGIC;   /* Flag that this object is now deallocated. */

    /* TODO: Free allocation here, does the interface override this function or do we leave this to the interface */
    Py_TYPE(self)->tp_free(self);
}
