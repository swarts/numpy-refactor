 /*
  Provide multidimensional arrays as a basic object type in python.  

Based on Original Numeric implementation
Copyright (c) 1995, 1996, 1997 Jim Hugunin, hugunin@mit.edu

with contributions from many Numeric Python developers 1995-2004               *

Heavily modified in 2005 with inspiration from Numarray 

by

Travis Oliphant
Assistant Professor at
Brigham Young University 

maintainer email:  oliphant.travis@ieee.org

Numarray design (which provided guidance) by 
Space Science Telescope Institute 
  (J. Todd Miller, Perry Greenfield, Rick White)

*/

/* $Id: arrayobject.c,v 1.59 2005/09/14 00:14:00 teoliphant Exp $ */

/*
#include "Python.h"
#include "structmember.h"

#define _MULTIARRAYMODULE
#include "Numeric3/arrayobject.h"
*/

/* Helper functions */

#define error_converting(x)  (((x) == -1) && PyErr_Occurred())

static intp
PyArray_PyIntAsIntp(PyObject *o)
{
	longlong long_value = -1;
	PyObject *obj;
	static char *msg = "an integer is required";
	PyObject *arr=NULL;
	PyArray_Typecode typecode = {PyArray_INTP, 0, 0};
	intp ret;

	if (!o) {
		PyErr_SetString(PyExc_TypeError, msg);
		return -1;
	}
	if (PyArray_Check(o)) {
		if (PyArray_SIZE(o)!=1 || !PyArray_ISINTEGER(o)) {
			PyErr_SetString(PyExc_TypeError, msg);
			return -1;
		}
		arr = PyArray_CastToType((PyArrayObject *)o, &typecode);
	}
	if (PyArray_IsScalar(o, Integer)) {
		arr = PyArray_FromScalar(o, &typecode);
	}
	if (arr != NULL) {
		ret = *((intp *)PyArray_DATA(arr));
		Py_DECREF(arr);
		return ret;
	}		
	if (PyInt_Check(o)) {
		long_value = (longlong) PyInt_AS_LONG(o);
	} else if (PyLong_Check(o)) {
		long_value = (longlong) PyLong_AsLongLong(o);
	} else if (o->ob_type->tp_as_number != NULL &&		\
		    o->ob_type->tp_as_number->nb_long != NULL) {
		obj = o->ob_type->tp_as_number->nb_long(o);
		if (obj != NULL) {
			long_value = (longlong) PyLong_AsLongLong(obj);
			Py_DECREF(obj);
		}
	} else if (o->ob_type->tp_as_number != NULL &&		\
		    o->ob_type->tp_as_number->nb_int != NULL) {
		obj = o->ob_type->tp_as_number->nb_int(o);
		if (obj != NULL) {
			long_value = (longlong) PyLong_AsLongLong(obj);
			Py_DECREF(obj);
		}
	} else {
		PyErr_SetString(PyExc_NotImplementedError,"");
	}
	
	if error_converting(long_value) {
		PyErr_SetString(PyExc_TypeError, msg);
		return -1;
	}
	
#if (SIZEOF_LONGLONG != SIZEOF_PY_INTPTR_T)
	if ((long_value < MIN_INTP) || (long_value > MAX_INTP)) {
		PyErr_SetString(PyExc_ValueError,
				"integer won't fit into a C intp");
		return -1;
	}
#endif
	return (intp) long_value;
}


static PyObject *array_int(PyArrayObject *v);

static int
PyArray_PyIntAsInt(PyObject *o)
{
	long long_value = -1;
	PyObject *obj;
	static char *msg = "an integer is required";
	PyObject *arr=NULL;
	PyArray_Typecode typecode = {PyArray_INT, 0, 0};
	int ret;
	
	if (!o) {
		PyErr_SetString(PyExc_TypeError, msg);
		return -1;
	}
	if (PyArray_Check(o)) {
		if (PyArray_SIZE(o)!=1 || !PyArray_ISINTEGER(o)) {
			PyErr_SetString(PyExc_TypeError, msg);
			return -1;
		}
		arr = PyArray_CastToType((PyArrayObject *)o, &typecode);
	}
	if (PyArray_IsScalar(o, Integer)) {
		arr = PyArray_FromScalar(o, &typecode);
	}
	if (arr != NULL) {
		ret = *((int *)PyArray_DATA(arr));
		Py_DECREF(arr);
		return ret;
	}		
	if (PyInt_Check(o)) {
		long_value = (long) PyInt_AS_LONG(o);
	} else if (PyLong_Check(o)) {
		long_value = (long) PyLong_AsLong(o);
	} else if (o->ob_type->tp_as_number != NULL &&		\
		    o->ob_type->tp_as_number->nb_long != NULL) {
		obj = o->ob_type->tp_as_number->nb_long(o);
		if (obj == NULL) return -1;
		long_value = (long) PyLong_AsLong(obj);
		Py_DECREF(obj);
	} else if (o->ob_type->tp_as_number != NULL &&		\
		    o->ob_type->tp_as_number->nb_int != NULL) {
		obj = o->ob_type->tp_as_number->nb_int(o);
		if (obj == NULL) return -1;
		long_value = (long) PyLong_AsLong(obj);
		Py_DECREF(obj);
	} else {
		PyErr_SetString(PyExc_NotImplementedError,"");
	}
	if error_converting(long_value) {
		PyErr_SetString(PyExc_TypeError, msg);
		return -1;
	}
	
#if (SIZEOF_LONG != SIZEOF_INT)
	if ((long_value < INT_MIN) || (long_value > INT_MAX)) {
		PyErr_SetString(PyExc_ValueError,
				"integer won't fit into a C int");
		return -1;
	}
#endif
	return (int) long_value;
}


static double
PyArray_GetPriority(PyObject *obj, double default_) 
{
        PyObject *ret;
        double priority=PyArray_PRIORITY;

	if (PyArray_CheckExact(obj))
		return priority;
        if (PyBigArray_CheckExact(obj)) 
                return PyArray_BIG_PRIORITY;

        ret = PyObject_GetAttrString(obj, "__array_priority__");
        if (ret != NULL) priority = PyFloat_AsDouble(ret);
        if (PyErr_Occurred()) {
                PyErr_Clear();                
                priority = default_;
        }
        Py_XDECREF(ret);
        return priority;        
}

/* Backward compatibility only */
/* In both Zero and One

 ***You must free the memory once you are done with it
    using PyDataMem_FREE(ptr) or you create a memory leak***

    If arr is an Object array you are getting a 
    BORROWED reference to Zero or One.
    Do not DECREF.
    Please INCREF if you will be hanging on to it.

    The memory for the ptr still must be freed in any case;
*/


static char *
PyArray_Zero(PyArrayObject *arr)
{
        char *zeroval;
        int ret, storeflags;
        PyObject *obj;

        zeroval = PyDataMem_NEW(arr->itemsize);
        if (zeroval == NULL) {
                PyErr_SetNone(PyExc_MemoryError);
                return NULL;
        }

	obj=PyInt_FromLong((long) 0);
        if (PyArray_ISOBJECT(arr)) {
                memcpy(zeroval, &obj, sizeof(PyObject *));
                Py_DECREF(obj);
                return zeroval;
        }
	storeflags = arr->flags;
	arr->flags |= BEHAVED_FLAGS;
        ret = arr->descr->setitem(obj, zeroval, arr);
	arr->flags = storeflags;
	Py_DECREF(obj);
	if (ret < 0) {
		PyDataMem_FREE(zeroval);
		return NULL;
	}
        return zeroval;
}

static char *
PyArray_One(PyArrayObject *arr)
{
        char *oneval;
        int ret, storeflags;
        PyObject *obj;

        oneval = PyDataMem_NEW(arr->itemsize);
        if (oneval == NULL) {
                PyErr_SetNone(PyExc_MemoryError);
                return NULL;
        }

        obj = PyInt_FromLong((long) 1);
        if (PyArray_ISOBJECT(arr)) {
                memcpy(oneval, &obj, sizeof(PyObject *));
                Py_DECREF(obj);
                return oneval;
        }        

	storeflags = arr->flags;
	arr->flags |= BEHAVED_FLAGS;
        ret = arr->descr->setitem(obj, oneval, arr);
	arr->flags = storeflags;
        Py_DECREF(obj);
        if (ret < 0) {
                PyDataMem_FREE(oneval);
                return NULL;
        }
        return oneval;
}

/* End deprecated */


static int 
do_sliced_copy(char *dest, intp *dest_strides, intp *dest_dimensions,
	       int dest_nd, char *src, intp *src_strides, 
	       intp *src_dimensions, int src_nd, int elsize, 
	       int copies) {
        intp i, j;
	
        if (src_nd == 0 && dest_nd == 0) {
                for(j=0; j<copies; j++) {
                        memmove(dest, src, elsize);
                        dest += elsize;
                }
                return 0;
        }
	
        if (dest_nd > src_nd) {
                for(i=0; i<*dest_dimensions; i++, dest += *dest_strides) {
                        if (do_sliced_copy(dest, dest_strides+1, 
                                           dest_dimensions+1, dest_nd-1,
                                           src, src_strides, 
                                           src_dimensions, src_nd, 
                                           elsize, copies) == -1) 
                                return -1;
                }
                return 0;
        }
	
        if (dest_nd == 1) {
                if (*dest_dimensions != *src_dimensions) {
                        PyErr_SetString(PyExc_ValueError, 
                                        "matrices are not aligned for copy");
                        return -1;
                }
                for(i=0; i<*dest_dimensions; i++, src += *src_strides) {
                        for(j=0; j<copies; j++) {
                                memmove(dest, src, elsize);
                                dest += *dest_strides;
                        }
                }
        } else {
                for(i=0; i<*dest_dimensions; i++, dest += *dest_strides, 
                            src += *src_strides) {
                        if (do_sliced_copy(dest, dest_strides+1, 
                                           dest_dimensions+1, dest_nd-1, 
                                           src, src_strides+1, 
                                           src_dimensions+1, src_nd-1, 
                                           elsize, copies) == -1) 
                                return -1;
                }
        }
        return 0;
}

/* This function reduces a source and destination array until a
   discontiguous segment is found in either the source or
   destination. Thus, an N dimensional array where the last dimension
   is contiguous and has size n while the items are of size elsize,
   will be reduced to an N-1 dimensional array with items of size n *
   elsize.

   This process is repeated until a discontiguous section is found.
   Thus, a contiguous array will be reduced to a 0-dimensional array
   with items of size elsize * sizeof(N-dimensional array).

   Finally, if a source array has been reduced to a 0-dimensional
   array with large element sizes, the contiguous destination array is
   reduced as well.

   The only thing this function changes is the element size, the
   number of copies, and the source and destination number of
   dimensions.  The strides and dimensions are not changed. 
*/

static int 
optimize_slices(intp **dest_strides, intp **dest_dimensions, 
		    int *dest_nd, intp **src_strides, 
		    intp **src_dimensions, int *src_nd,
		    int *elsize, int *copies) 
{
        while (*src_nd > 0) {
                if (((*dest_strides)[*dest_nd-1] == *elsize) && 
                    ((*src_strides)[*src_nd-1] == *elsize)) {
                        if ((*dest_dimensions)[*dest_nd-1] != 
                            (*src_dimensions)[*src_nd-1]) {
				PyErr_SetString(PyExc_ValueError,  
						"matrices are not aligned");
                                return -1;
                        }
                        *elsize *= (*dest_dimensions)[*dest_nd-1];
                        *dest_nd-=1; *src_nd-=1;
                } else {
                        break;
                }
        }
        if (*src_nd == 0) {
                while (*dest_nd > 0) {
                        if (((*dest_strides)[*dest_nd-1] == *elsize)) {
                                *copies *= (*dest_dimensions)[*dest_nd-1];
                                *dest_nd-=1;
                        } else {
                                break;
                        }
                }
        }
        return 0;
}

static char *
contiguous_data(PyArrayObject *src) 
{
        intp dest_strides[MAX_DIMS], *dest_strides_ptr;
        intp *dest_dimensions=src->dimensions;
        int dest_nd=src->nd;
        intp *src_strides = src->strides;
        intp *src_dimensions=src->dimensions;
        int src_nd=src->nd;
        int elsize=src->itemsize;
        int copies=1;
        int ret, i;
        intp stride=elsize;
        char *new_data;
	
        for(i=dest_nd-1; i>=0; i--) {
                dest_strides[i] = stride;
                stride *= dest_dimensions[i];
        }
	
        dest_strides_ptr = dest_strides;
	
        if (optimize_slices(&dest_strides_ptr, &dest_dimensions, &dest_nd,
                            &src_strides, &src_dimensions, &src_nd,
                            &elsize, &copies) == -1) 
                return NULL;
	
        new_data = (char *)malloc(stride);
	
        ret = do_sliced_copy(new_data, dest_strides_ptr, dest_dimensions, 
                             dest_nd, src->data, src_strides, 
                             src_dimensions, src_nd, elsize, copies);
	
        if (ret != -1) { return new_data; }
        else { free(new_data); return NULL; }
}

/* end Helper functions */


static PyObject *PyArray_New(PyTypeObject *, int nd, intp *, 
                             int, intp *, void *, int, int, PyObject *);

/* C-API functions */

/* Used for arrays of python objects to increment the reference count of */
/* every python object in the array. */
static int 
PyArray_INCREF(PyArrayObject *mp) 
{
	intp i, n;

        PyObject **data, **data2;
	
        if (mp->descr->type_num != PyArray_OBJECT) return 0;
	
        if (PyArray_ISONESEGMENT(mp)) {
                data = (PyObject **)mp->data;
        } else {
                if ((data = (PyObject **)contiguous_data(mp)) == NULL) 
                        return -1;
        }
	
        n = PyArray_SIZE(mp);
        data2 = data;
        for(i=0; i<n; i++, data++) Py_XINCREF(*data);
	
        if (!PyArray_ISONESEGMENT(mp)) free(data2);
	
        return 0;
}

static int 
PyArray_XDECREF(PyArrayObject *mp) 
{
        intp i, n;
        PyObject **data, **data2;
	
        if (mp->descr->type_num != PyArray_OBJECT) return 0;
	
        if (PyArray_ISONESEGMENT(mp)) {
                data = (PyObject **)mp->data;
        } else {
                if ((data = (PyObject **)contiguous_data(mp)) == NULL) 
                        return -1;
        }
	
        n = PyArray_SIZE(mp);
        data2 = data;    
        for(i=0; i<n; i++, data++) Py_XDECREF(*data);
	
        if (!PyArray_ISONESEGMENT(mp)) free(data2);
	
        return 0;
}

/* byte-swap inplace (unrolled loops for special cases) */
static void 
byte_swap_vector(void *p, int n, int size) {
        char *a, *b, c=0;
        int j,m;

        switch(size) {
        case 1: /* no byteswap necessary */
                break;
        case 2:
                for (a = (char*)p ; n > 0; n--, a += 1) {
                        b = a + 1;
                        c = *a; *a++ = *b; *b   = c;
                }
                break;
        case 4:
                for (a = (char*)p ; n > 0; n--, a += 2) {
                        b = a + 3;
                        c = *a; *a++ = *b; *b-- = c;
                        c = *a; *a++ = *b; *b   = c;
                }
                break;
        case 8:
                for (a = (char*)p ; n > 0; n--, a += 4) {
                        b = a + 7;
                        c = *a; *a++ = *b; *b-- = c;
                        c = *a; *a++ = *b; *b-- = c;
                        c = *a; *a++ = *b; *b-- = c;
                        c = *a; *a++ = *b; *b   = c;
                }
                break;
        default:
                m = size / 2;
                for (a = (char *)p ; n > 0; n--, a += m) {
                        b = a + (size-1);
                        for (j=0; j<m; j++)
                                c=*a; *a++ = *b; *b-- = c;
                }
                break;
        }
}


/* If numitems > 1, then dst must be contiguous */
static void
copy_and_swap(void *dst, void *src, int itemsize, intp numitems,
              intp srcstrides, int swap) 
{
        int i;
        char *s1 = (char *)src;
        char *d1 = (char *)dst;
        

        if ((numitems == 1) || (itemsize == srcstrides)) 
                memcpy(d1, s1, itemsize*numitems);
        else {                
                for (i = 0; i < numitems; i++) {
                        memcpy(d1, s1, itemsize);
                        d1 += itemsize;
                        s1 += srcstrides;
                }                                
        }

        if (swap)
                byte_swap_vector(d1, numitems, itemsize);
}


static PyArray_Descr **userdescrs=NULL;
/* Computer-generated arraytype and scalartype code */
#include "scalartypes.inc"
#include "arraytypes.inc"

static char *
index2ptr(PyArrayObject *mp, intp i) 
{
	if(mp->nd == 0) {
		PyErr_SetString(PyExc_IndexError, 
				"0-d arrays can't be indexed");
		return NULL;
	}
	if (i==0 && mp->dimensions[0] > 0)
		return mp->data;
	
        if (mp->nd>0 &&  i>0 && i < mp->dimensions[0]) {
                return mp->data+i*mp->strides[0];
        }
        PyErr_SetString(PyExc_IndexError,"index out of bounds");
        return NULL;
}

static intp 
PyArray_Size(PyObject *op) 
{
        if (PyArray_Check(op)) {
                return PyArray_SIZE((PyArrayObject *)op);
        } 
	else {
                return 0;
        }
}

/* If destination is not the right type, then src 
   will be cast to destination. 
*/

/* Does a flat iterator-based copy. 

   The arrays are assumed to have the same number of elements
   They can be different sizes and have different types however. 
*/

static int
PyArray_CopyInto(PyArrayObject *dest, PyArrayObject *src)
{
        intp dsize, ssize, sbytes, ncopies;
	int elsize, index;
        PyArrayIterObject *dit=NULL;
        PyArrayIterObject *sit=NULL;
	char *dptr;
	int swap;
        PyArray_CopySwapFunc *copyswap;
        PyArray_CopySwapNFunc *copyswapn;
       
        if (!PyArray_ISWRITEABLE(dest)) {
                PyErr_SetString(PyExc_RuntimeError, 
                                "cannot write to array");
                return -1;
        }

        if (!PyArray_EquivArrTypes(dest, src)) {
                return PyArray_CastTo(dest, src);
        }

        dsize = PyArray_SIZE(dest);
        ssize = PyArray_SIZE(src);
	if (ssize == 0) return 0;
        if (dsize % ssize != 0) {
                PyErr_SetString(PyExc_ValueError, 
                                "number of elements in destination must be "\
                                "integer multiple of number of "\
                                "elements in source");
                return -1;
        }
        ncopies = (dsize / ssize);

	swap = PyArray_ISNOTSWAPPED(dest) != PyArray_ISNOTSWAPPED(src);
	copyswap = dest->descr->copyswap;
	copyswapn = dest->descr->copyswapn;

        elsize = dest->itemsize;

        if ((PyArray_ISCONTIGUOUS(dest) && PyArray_ISCONTIGUOUS(src))	\
	    || (PyArray_ISFORTRAN(dest) && PyArray_ISFORTRAN(src))) {
               
                PyArray_XDECREF(dest);
                dptr = dest->data;
                sbytes = ssize * src->itemsize;
                while(ncopies--) {
                        memmove(dptr, src->data, sbytes);
                        dptr += sbytes;
                }
		if (swap)
			copyswapn(dest->data, NULL, dsize, 1, elsize);
                PyArray_INCREF(dest);
                return 0;
        }

        dit = (PyArrayIterObject *)PyArray_IterNew((PyObject *)dest);
        sit = (PyArrayIterObject *)PyArray_IterNew((PyObject *)src);

        if ((dit == NULL) || (sit == NULL)) {
                Py_XDECREF(dit);
                Py_XDECREF(sit);
                return -1;
        }

        PyArray_XDECREF(dest);
        while(ncopies--) {
                index = ssize;
                while(index--) {
                        memmove(dit->dataptr, sit->dataptr, elsize);
			if (swap)
				copyswap(dit->dataptr, NULL, 1, elsize);
                        PyArray_ITER_NEXT(dit);
                        PyArray_ITER_NEXT(sit);
                }
                PyArray_ITER_RESET(sit);
        }                                     
        PyArray_INCREF(dest);
        Py_DECREF(dit);
        Py_DECREF(sit);
	return 0;
}


static int 
PyArray_CopyObject(PyArrayObject *dest, PyObject *src_object) 
{
        PyArrayObject *src;
        int ret;
	PyArray_Typecode typecode;


	typecode.type_num = dest->descr->type_num;
	typecode.itemsize = dest->itemsize;
	typecode.fortran = PyArray_ISFORTRAN(dest);
			
        src = (PyArrayObject *)PyArray_FromAny(src_object,
					       &typecode, 0,
					       dest->nd, 0);
        if (src == NULL) return -1;

        ret = PyArray_CopyInto(dest, src);
        Py_DECREF(src);
        return ret;
}


/* These are also old calls (should use PyArray_New) */

/* They all zero-out the memory as previously done */

static PyObject *
PyArray_FromDimsAndDataAndDescr(int nd, int *d, 
                                PyArray_Descr *descr,
                                char *data) {
	PyObject *ret;

#if SIZEOF_INTP != SIZEOF_INT
	int i;
	intp newd[MAX_DIMS];
	
	for (i=0; i<nd; i++) newd[i] = (intp) d[i];
        ret = PyArray_New(&PyArray_Type, nd, newd, 
                           descr->type_num, NULL, data, descr->elsize, 
			   CARRAY_FLAGS, NULL);
#else
	ret = PyArray_New(&PyArray_Type, nd, (intp *)d, 
                           descr->type_num, NULL, data, descr->elsize, 
			   CARRAY_FLAGS, NULL);
#endif
	return ret;
}


static PyObject *
PyArray_FromDimsAndData(int nd, int *d, int type, char *data) 
{
	PyObject *ret;
#if SIZEOF_INTP != SIZEOF_INT
	int i;	
	intp newd[MAX_DIMS];

	for (i=0; i<nd; i++) newd[i] = (intp) d[i]; 
	ret = PyArray_New(&PyArray_Type, nd, newd, 
			   type, NULL, data, 0, 
			   CARRAY_FLAGS, NULL);
#else
	ret = PyArray_New(&PyArray_Type, nd, (intp *)d, 
			   type, NULL, data, 0, 
			   CARRAY_FLAGS, NULL);
#endif
	return ret;
}


static PyObject *
PyArray_FromDims(int nd, int *d, int type) 
{
	PyObject *ret;
#if SIZEOF_INTP != SIZEOF_INT
	intp newd[MAX_DIMS];
	int i;	

	for (i=0; i<nd; i++) newd[i] = (intp) d[i];
	ret = PyArray_New(&PyArray_Type, nd, newd, type,
			   NULL, NULL, 0, 0, NULL);
#else
	ret = PyArray_New(&PyArray_Type, nd, (intp *)d, type,
			   NULL, NULL, 0, 0, NULL);
#endif
	if (type != PyArray_OBJECT && type != PyArray_OBJECTLTR) 
		memset(PyArray_DATA(ret), 0, PyArray_SIZE(ret));
	return ret;
}

/* end old calls */

/* Copy should always return contiguous array */
static PyObject *
PyArray_Copy(PyArrayObject *m1) 
{
	PyArrayObject *ret;
	ret = (PyArrayObject *)PyArray_New(m1->ob_type, m1->nd, 
					   m1->dimensions,
					   m1->descr->type_num,
					   NULL, NULL, m1->itemsize,
					   0, (PyObject *)m1);
	
        if (PyArray_CopyInto(ret, m1) == -1) return NULL;
	
        return (PyObject *)ret;
}


static PyObject *array_big_item(PyArrayObject *, intp);

static PyObject *
PyArray_Scalar(void *data, int type_num, int itemsize, int swap)
{
        PyArray_Descr *descr;
	PyTypeObject *type;
	PyObject *obj;	
        void *destptr;
        PyArray_CopySwapFunc *copyswap;

        descr = PyArray_DescrFromType(type_num);
        if (descr == NULL) return NULL;
        type = descr->typeobj;
        copyswap = descr->copyswap;
	if (type_num == PyArray_STRING) 
		obj = type->tp_alloc(type, itemsize);
	else
		obj = type->tp_alloc(type, 0);
	if (obj == NULL) return NULL;
	if PyTypeNum_ISFLEXIBLE(type_num) {  
		if (type_num == PyArray_STRING) {
			destptr = PyString_AS_STRING(obj);
			((PyStringObject *)obj)->ob_shash = -1;
			((PyStringObject *)obj)->ob_sstate =	\
				SSTATE_NOT_INTERNED; 
		}
		else {
			destptr = PyDataMem_NEW(itemsize);
			if (destptr == NULL) {
				PyObject_Del(obj);
				return PyErr_NoMemory();
			}
			if (type_num == PyArray_UNICODE) {
				PyUnicode_AS_UNICODE(obj) = \
					(Py_UNICODE *)destptr;
				((PyUnicodeObject*)obj)->length = itemsize / \
					sizeof(Py_UNICODE);
				((PyUnicodeObject*)obj)->hash = -1;
			}
			else {
				((PyVoidScalarObject *)obj)->obval = destptr;
				((PyVoidScalarObject *)obj)->ob_size = itemsize;
			}
		}
	}
	else {
		destptr = &(((PyScalarObject*)obj)->obval);
	}
	/* copyswap for OBJECT increments the reference count */
        copyswap(destptr, data, swap, itemsize);
	return obj;
}

/* returns an Array-Scalar Object of the type of arr
   from the given pointer to memory -- main Scalar creation function
   default new method calls this. 
*/
static PyObject *
PyArray_ToScalar(void *data, PyArrayObject *arr)
{
	int type_num = arr->descr->type_num;
	int itemsize = arr->itemsize;
        int swap = !(PyArray_ISNOTSWAPPED(arr));

	return PyArray_Scalar(data, type_num, itemsize, swap);
}


/* Return Python scalar if 0-d array object is encountered */

static PyObject *
PyArray_Return(PyArrayObject *mp) 
{
        
	if (mp == NULL) return NULL;

        if (PyErr_Occurred()) {
                Py_XDECREF(mp);
                return NULL;
        }

	if (PyArray_Check((PyObject *)mp) && mp->nd == 0) {
		PyObject *ret;
		ret = PyArray_ToScalar(mp->data, mp);
		Py_DECREF(mp);
		return ret;
	}
	else {
		return (PyObject *)mp;
	}
}

/*
  returns typenum to associate with this type >=PyArray_USERDEF.
  Also creates a copy of the VOID_DESCR table inserting it's typeobject in
  and it's typenum in the appropriate place.
 
  needs the userdecrs table and PyArray_NUMUSER variables
  defined in arratypes.inc
*/
static int 
PyArray_RegisterDataType(PyTypeObject *type)
{
	PyArray_Descr *descr;
	int typenum;
	int i;
	
	if ((type == &PyVoidArrType_Type) ||			\
	    !PyType_IsSubtype(type, &PyVoidArrType_Type)) {
		PyErr_SetString(PyExc_ValueError, 
				"can only register void subtypes");
		return -1;
	}
	/* See if this type is already registered */
	for (i=0; i<PyArray_NUMUSERTYPES; i++) {
		descr = userdescrs[i];
		if (descr->typeobj == type)
			return descr->type_num;
	}
	descr = malloc(sizeof(PyArray_Descr));
	memcpy(descr, PyArray_DescrFromType(PyArray_VOID), 
	       sizeof(PyArray_Descr));
	typenum = PyArray_USERDEF + PyArray_NUMUSERTYPES;
	descr->type_num = typenum;
	descr->typeobj = type;
	userdescrs = realloc(userdescrs, 
			    (PyArray_NUMUSERTYPES+1)*sizeof(void *));
        if (userdescrs == NULL) {
                PyErr_SetString(PyExc_MemoryError, "RegisterDataType");
                return -1;
        }
	userdescrs[PyArray_NUMUSERTYPES++] = descr;
	return typenum;
}


/* 
   copyies over from the old descr table for anything
   NULL or zero in what is given. 
   frees the copy of the Descr_table already there.
   places a pointer to the new one into the slot.
*/
static int
PyArray_RegisterDescrForType(int typenum, PyArray_Descr *descr)
{
	PyArray_Descr *old;
	int i;

	if (!PyTypeNum_ISUSERDEF(typenum)) {
		PyErr_SetString(PyExc_TypeError, 
				"data type not registered");
		return -1;
	}
	old = userdescrs[typenum-PyArray_USERDEF];
	descr->typeobj = old->typeobj;
	descr->type_num = typenum;

#define _NULL_CHECK(member) \
	if (descr->member == NULL) descr->member = old->member

	for (i=0; i<PyArray_NTYPES; i++) {
		_NULL_CHECK(cast[i]);
	}
	_NULL_CHECK(getitem);
	_NULL_CHECK(setitem);
	_NULL_CHECK(compare);	
	_NULL_CHECK(argmax);
	_NULL_CHECK(dotfunc);
	_NULL_CHECK(scanfunc);
	_NULL_CHECK(copyswapn);
	_NULL_CHECK(copyswap);
	_NULL_CHECK(nonzero);
#undef _NULL_CHECK

#define _ZERO_CHECK(member) \
	if (descr->member == 0) descr->member = old->member

	_ZERO_CHECK(kind);
	_ZERO_CHECK(type);
	_ZERO_CHECK(elsize);
	_ZERO_CHECK(alignment);
#undef _ZERO_CHECK

	free(old);
	userdescrs[typenum-PyArray_USERDEF] = descr;
	return 0;
}


static int
PyArray_ToFile(PyArrayObject *self, FILE *fp, char *sep, char *format) 
{
        intp size;
        intp n, n2;
        int n3, n4;
        PyArrayIterObject *it;
        PyObject *obj, *strobj, *tupobj;

	n3 = (sep ? strlen((const char *)sep) : 0);
	if (n3 == 0) { /* binary data */
                if (PyArray_ISOBJECT(self)) {
                        PyErr_SetString(PyExc_ValueError, "cannot write "\
					"object arrays to a file in "	\
					"binary mode");
                        return -1;
                }

                if (PyArray_ISCONTIGUOUS(self)) {
                        size = PyArray_SIZE(self);
                        if ((n=fwrite((const void *)self->data, 
                                      (size_t) self->itemsize,
                                      (size_t) size, fp)) < size) {
                                PyErr_Format(PyExc_ValueError, 
                                             "%ld requested and %ld written",
                                             (long) size, (long) n);
                                return -1;
                        }
                }
                else {
                        it=(PyArrayIterObject *)                        \
                                PyArray_IterNew((PyObject *)self);
                        while(it->index < it->size) {
                                if (fwrite((const void *)it->dataptr, 
                                           (size_t) self->itemsize,
                                           1, fp) < 1) {
                                        PyErr_Format(PyExc_IOError, 
                                                     "problem writing element"\
                                                     " %d to file", 
						     (int)it->index);
                                        Py_DECREF(it);
                                        return -1;
                                }
                                PyArray_ITER_NEXT(it);
                        }
                        Py_DECREF(it);
                }                
        }
        else {  /* text data */
                it=(PyArrayIterObject *)                                \
                        PyArray_IterNew((PyObject *)self);
		n4 = (format ? strlen((const char *)format) : 0);
                while(it->index < it->size) {
                        obj = self->descr->getitem(it->dataptr, self);
                        if (obj == NULL) {Py_DECREF(it); return -1;}
			if (n4 == 0) { /* standard writing */
				strobj = PyObject_Str(obj);
				Py_DECREF(obj);
				if (strobj == NULL) {Py_DECREF(it); return -1;}
			}
			else { /* use format string */
				tupobj = PyTuple_New(1);
				if (tupobj == NULL) {Py_DECREF(it); return -1;}
				PyTuple_SET_ITEM(tupobj,0,obj);
				obj = PyString_FromString((const char *)format);
				if (obj == NULL) {Py_DECREF(tupobj); 
					Py_DECREF(it); return -1;}
				strobj = PyString_Format(obj, tupobj);
				Py_DECREF(obj);
				Py_DECREF(tupobj);
				if (strobj == NULL) {Py_DECREF(it); return -1;}
			}
                        if ((n=fwrite(PyString_AS_STRING(strobj), 
                                      1, n2=PyString_GET_SIZE(strobj),
                                      fp)) < n2) {
                                PyErr_Format(PyExc_IOError,
                                             "problem writing element %d"\
                                             " to file", 
					     (int) it->index);
                                Py_DECREF(strobj);
                                Py_DECREF(it);
                                return -1;
                        }
                        /* write separator for all but last one */
                        if (it->index != it->size-1) 
                                fwrite(sep, 1, n3, fp);
                        Py_DECREF(strobj);
                        PyArray_ITER_NEXT(it);
                }
                Py_DECREF(it);
        }
        return 0;
}

static PyObject *
PyArray_ToList(PyArrayObject *self) 
{
        PyObject *lp;
        PyArrayObject *v;
        intp sz, i;
	
        if (!PyArray_Check(self)) return (PyObject *)self;

        if (self->nd == 0) 
		return self->descr->getitem(self->data,self);
	
        sz = self->dimensions[0];
        lp = PyList_New(sz);
	
        for (i=0; i<sz; i++) {
                v=(PyArrayObject *)array_big_item(self, i);
		if (v->nd >= self->nd) {
			PyErr_SetString(PyExc_RuntimeError,
					"array_item not returning smaller-" \
					"dimensional array");
			Py_DECREF(lp);
			return NULL;
		}
                PyList_SetItem(lp, i, PyArray_ToList(v));
		Py_DECREF(v);
        }
	
        return lp;
}

static PyObject *
PyArray_ToString(PyArrayObject *self)
{
        intp numbytes;
        intp index;
        char *dptr;
        int elsize;
        PyObject *ret;
        PyArrayIterObject *it;
        
        if (PyArray_TYPE(self) == PyArray_OBJECT) {
                PyErr_SetString(PyExc_ValueError, "a string for the data" \
				"in an object array is not appropriate");
                return NULL;
        }

        numbytes = PyArray_NBYTES(self);
        if (PyArray_ISONESEGMENT(self)) {
                ret = PyString_FromStringAndSize(self->data, (int) numbytes);
        }
        else {
                it = (PyArrayIterObject *)PyArray_IterNew((PyObject *)self);
                if (it==NULL) return NULL;
                ret = PyString_FromStringAndSize(NULL, (int) numbytes);
                if (ret == NULL) {Py_DECREF(it); return NULL;}
                dptr = PyString_AS_STRING(ret);
                index = it->size;
                elsize = self->itemsize;
                while(index--) {
                        memcpy(dptr, it->dataptr, elsize);
                        dptr += elsize;
                        PyArray_ITER_NEXT(it);
                }
                Py_DECREF(it);
        }
	return ret;
}


/*********************** end C-API functions **********************/


/* array object functions */

static void 
array_dealloc(PyArrayObject *self) {

        if (self->weakreflist != NULL)
                PyObject_ClearWeakRefs((PyObject *)self);

        if(self->base) {
		/* UPDATEIFCOPY means that base points to an 
		   array that should be updated with the contents
		   of this array upon destruction.
                   self->base->flags must have been WRITEABLE 
                   (checked previously) and it was locked here
                   thus, unlock it.
		*/
		if (self->flags & UPDATEIFCOPY) {
                        ((PyArrayObject *)self->base)->flags |= WRITEABLE;
			Py_INCREF(self); /* hold on to self in next call */
                        PyArray_CopyInto((PyArrayObject *)self->base, self);
			/* Don't need to DECREF -- because we are deleting
			   self already... */
		}
		/* In any case base is pointing to something that we need
		   to DECREF -- either a view or a buffer object */
                Py_DECREF(self->base);
        }
        
        if ((self->flags & OWN_DATA) && (self->data != NULL)) {
		/* Free internal references if an Object array */
		PyArray_XDECREF(self);

                PyDataMem_FREE(self->data);
        }
	
        if (self->dimensions != NULL) {
                PyDimMem_FREE(self->dimensions); 
	}
	
        self->ob_type->tp_free((PyObject *)self);
}

/*************************************************************************
 ****************   Implement Mapping Protocol ***************************
 *************************************************************************/

static int 
array_length(PyArrayObject *self) 
{
        if (self->nd != 0) {
                return self->dimensions[0];
        } else {
		PyErr_SetString(PyExc_TypeError, "len() of unsized object");
		return -1;
        }
}


static PyObject *
array_big_item(PyArrayObject *self, intp i) 
{
	char *item;
	PyArrayObject *r;
	
	
	if(self->nd == 0) {
		PyErr_SetString(PyExc_IndexError, 
				"0-d arrays can't be indexed");
		return NULL;
	}
        if ((item = index2ptr(self, i)) == NULL) return NULL;
	
	r = (PyArrayObject *)PyArray_New(self->ob_type, 
					 self->nd-1, self->dimensions+1, 
					 self->descr->type_num, 
					 self->strides+1, item, 
					 self->itemsize, self->flags,
					 (PyObject *)self);
	if (r == NULL) return NULL;
	Py_INCREF(self);
	r->base = (PyObject *)self;
        PyArray_UpdateFlags(r, CONTIGUOUS | FORTRAN);
	return (PyObject *)r;
}

static PyObject *
array_item_nice(PyArrayObject *self, int i) 
{
	return PyArray_Return((PyArrayObject *)array_big_item(self, (intp) i));
}


static int 
array_ass_big_item(PyArrayObject *self, intp i, PyObject *v) 
{
        PyArrayObject *tmp;
        char *item;
        int ret;

        if (v == NULL) {
                PyErr_SetString(PyExc_ValueError, 
                                "can't delete array elements");
                return -1;
        }
	if (!PyArray_ISWRITEABLE(self)) {
		PyErr_SetString(PyExc_RuntimeError,
				"array is not writeable");
		return -1;
	}
        if (self->nd == 0) {
                PyErr_SetString(PyExc_ValueError, 
                                "0-d arrays can't be indexed.");
                return -1;
        }

        if (i < 0) i = i+self->dimensions[0];

        if (self->nd > 1) {
                if((tmp = (PyArrayObject *)array_big_item(self, i)) == NULL)
                        return -1;
                ret = PyArray_CopyObject(tmp, v);
                Py_DECREF(tmp);
                return ret;   
        }
	
        if ((item = index2ptr(self, i)) == NULL) return -1;
        if (self->descr->setitem(v, item, self) == -1) return -1;
        return 0;
}

#if SIZEOF_INT == SIZEOF_INTP
#define array_ass_item array_ass_big_item
#else
static int
array_ass_item(PyArrayObject *self, int i, PyObject *v)
{
	return array_ass_big_item(self, (intp) i, v);
}
#endif


/* -------------------------------------------------------------- */
static int
slice_coerce_index(PyObject *o, int *v)
{
	*v = PyArray_PyIntAsInt(o);
	if (error_converting(*v)) {
		PyErr_Clear();
		return 0;
	}
	return 1;
}


/* This is basically PySlice_GetIndicesEx, but with our coercion
 * of indices to integers (plus, that function is new in Python 2.3) */
static int
slice_GetIndices(PySliceObject *r, int length,
                 int *start, int *stop, int *step,
                 intp *slicelength)
{
	int defstart, defstop;
	
	if (r->step == Py_None) {
		*step = 1;
	} else {
		if (!slice_coerce_index(r->step, step)) return -1;
		if (*step == 0) {
			PyErr_SetString(PyExc_ValueError, 
					"slice step cannot be zero");
			return -1;
		}
	}
	
	defstart = *step < 0 ? length - 1 : 0;
	defstop = *step < 0 ? -1 : length;
	
	if (r->start == Py_None) {
		*start = *step < 0 ? length-1 : 0;
	} else {
		if (!slice_coerce_index(r->start, start)) return -1;
		if (*start < 0) *start += length;
		if (*start < 0) *start = (*step < 0) ? -1 : 0;
		if (*start >= length) {
			*start = (*step < 0) ? length - 1 : length;
		}
	}
	
	if (r->stop == Py_None) {
		*stop = defstop;
	} else {
		if (!slice_coerce_index(r->stop, stop)) return -1;
		if (*stop < 0) *stop += length;
        if (*stop < 0) *stop = -1;
        if (*stop > length) *stop = length;
	}
	
	if ((*step < 0 && *stop >= *start) || \
	    (*step > 0 && *start >= *stop)) {
		*slicelength = 0;
	} else if (*step < 0) {
		*slicelength = (*stop - *start + 1) / (*step) + 1;
	} else {
		*slicelength = (*stop - *start - 1) / (*step) + 1;
	}
	
	return 0;
}

#define PseudoIndex -1
#define RubberIndex -2
#define SingleIndex -3

static int
parse_subindex(PyObject *op, int *step_size, intp *n_steps, int max)
{
	int index;
	
	if (op == Py_None) {
		*n_steps = PseudoIndex;
		index = 0;
	} else if (op == Py_Ellipsis) {
		*n_steps = RubberIndex;
		index = 0;
	} else if (PySlice_Check(op)) {
		int stop;
		if (slice_GetIndices((PySliceObject *)op, max,
				     &index, &stop, step_size, n_steps) < 0) {
			if (!PyErr_Occurred()) {
				PyErr_SetString(PyExc_IndexError, 
						"invalid slice");
			}
			goto fail;
		}
		if (*n_steps <= 0) {
			*n_steps = 0;
			*step_size = 1;
			index = 0;
		}
	} else {
		index = PyArray_PyIntAsInt(op);
		if (error_converting(index)) {
			PyErr_SetString(PyExc_IndexError,
					"each subindex must be either a "\
					"slice, an integer, Ellipsis, or "\
					"newaxis");
			goto fail;
		}
		*n_steps = SingleIndex;
		*step_size = 0;
		if (index < 0) index += max;
		if (index >= max || index < 0) {
			PyErr_SetString(PyExc_IndexError, "invalid index");
			goto fail;
		}
	}
	return index;
 fail:
	return -1;
}


static int 
parse_index(PyArrayObject *self, PyObject *op, 
            intp *dimensions, intp *strides, intp *offset_ptr)
{
        int i, j, n;
        int nd_old, nd_new, start, offset, n_add, n_pseudo;
        int step_size;
	intp n_steps;
        PyObject *op1=NULL;
        int is_slice;


        if (PySlice_Check(op) || op == Py_Ellipsis || op == Py_None) {
                n = 1;
                op1 = op;
                Py_INCREF(op);	
                /* this relies on the fact that n==1 for loop below */
                is_slice = 1;
        }
        else {
                if (!PySequence_Check(op)) {
                        PyErr_SetString(PyExc_IndexError, 
                                        "index must be either an int "\
                                        "or a sequence");
                        return -1;
                }
                n = PySequence_Length(op);
                is_slice = 0;
        }
	
        nd_old = nd_new = 0;
	
        offset = 0;
        for(i=0; i<n; i++) {
                if (!is_slice) {
                        if (!(op1=PySequence_GetItem(op, i))) {
                                PyErr_SetString(PyExc_IndexError, 
                                                "invalid index");
                                return -1;
                        }
                }
	
                start = parse_subindex(op1, &step_size, &n_steps, 
                                       nd_old < self->nd ? \
                                       self->dimensions[nd_old] : 0);
                Py_DECREF(op1);
                if (start == -1) break;
		
                if (n_steps == PseudoIndex) {
                        dimensions[nd_new] = 1; strides[nd_new] = 0; nd_new++;
                } else {
                        if (n_steps == RubberIndex) {
                                for(j=i+1, n_pseudo=0; j<n; j++) {
                                        op1 = PySequence_GetItem(op, j);
                                        if (op1 == Py_None) n_pseudo++;
                                        Py_DECREF(op1);
                                }
                                n_add = self->nd-(n-i-n_pseudo-1+nd_old);
                                if (n_add < 0) {
                                        PyErr_SetString(PyExc_IndexError, 
                                                        "too many indices");
                                        return -1;
                                }
                                for(j=0; j<n_add; j++) {
                                        dimensions[nd_new] = \
                                                self->dimensions[nd_old];
                                        strides[nd_new] = \
                                                self->strides[nd_old];
                                        nd_new++; nd_old++;
                                }
                        } else {
                                if (nd_old >= self->nd) {
                                        PyErr_SetString(PyExc_IndexError, 
                                                        "too many indices");
                                        return -1;
                                }
                                offset += self->strides[nd_old]*start;
                                nd_old++;
                                if (n_steps != SingleIndex) {
                                        dimensions[nd_new] = n_steps;
                                        strides[nd_new] = step_size * \
                                                self->strides[nd_old-1];
                                        nd_new++;
                                }
                        }
                }
        }
        if (i < n) return -1;
        n_add = self->nd-nd_old;
        for(j=0; j<n_add; j++) {
                dimensions[nd_new] = self->dimensions[nd_old];
                strides[nd_new] = self->strides[nd_old];
                nd_new++; nd_old++;
        }	  
        *offset_ptr = offset;
        return nd_new;
}

static void
_swap_axes(PyArrayMapIterObject *mit, PyArrayObject **ret)
{
	PyObject *new, *tup;
	int n1, n2, n3, val;
	int i;

	tup = PyTuple_New(mit->nd);
	/* tuple for transpose is 
	   (n1,..,n1+n2-1,0,..,n1-1,n1+n2,...,n3-1)
	   n1 is the number of dimensions of 
	      the broadcasted index array 
	   n2 is the number of dimensions skipped at the
	      start
	   n3 is the number of dimensions of the 
	      result 
	*/
	n1 = mit->iters[0]->nd_m1 + 1;
	n2 = mit->iteraxes[0];
	n3 = mit->nd;
	val = n1;
	i = 0;
	while(val < n1+n2) 
		PyTuple_SET_ITEM(tup, i++, PyInt_FromLong((long)val++));
	val = 0;
	while(val < n1)
		PyTuple_SET_ITEM(tup, i++, PyInt_FromLong((long)val++));
	val = n1+n2;
	while(val < n3)
		PyTuple_SET_ITEM(tup, i++, PyInt_FromLong((long)val++));

	new = PyArray_Transpose(*ret, tup);
	Py_DECREF(tup);
	Py_DECREF(*ret);
	*ret = (PyArrayObject *)new;
}



static PyObject *
PyArray_GetMap(PyArrayMapIterObject *mit)
{

	PyArrayObject *ret, *temp;
	PyArrayIterObject *it;
	int index;
	int swap;
        PyArray_CopySwapFunc *copyswap;

	/* Unbound map iterator --- Bind should have been called */
	if (mit->ait == NULL) return NULL;

	/* This relies on the map iterator object telling us the shape
	   of the new array in nd and dimensions.
	*/
	temp = mit->ait->ao;
	ret = (PyArrayObject *)\
		PyArray_New(temp->ob_type, mit->nd, mit->dimensions, 
			    temp->descr->type_num, NULL, NULL, 
			    temp->itemsize, 
			    PyArray_ISFORTRAN(temp),
			    (PyObject *)temp);
	if (ret == NULL) return NULL;

	/* Now just iterate through the new array filling it in
	   with the next object from the original array as
	   defined by the mapping iterator */

	if ((it = (PyArrayIterObject *)PyArray_IterNew((PyObject *)ret)) 
	    == NULL) 
		return NULL;
	index = it->size;
	swap = ((temp->flags & NOTSWAPPED) != (ret->flags & NOTSWAPPED));
        copyswap = ret->descr->copyswap;
	PyArray_MapIterReset(mit);
	while (index--) {
                copyswap(it->dataptr, mit->dataptr, swap, ret->itemsize);
		PyArray_MapIterNext(mit);
		PyArray_ITER_NEXT(it);
	}
	Py_DECREF(it);
	
	/* check for consecutive axes */
	if ((mit->subspace != NULL) && (mit->consec)) {
		if (mit->iteraxes[0] > 0) {  /* then we need to swap */
			_swap_axes(mit, &ret);
		}
	}
	return (PyObject *)ret;
}

static int
PyArray_SetMap(PyArrayMapIterObject *mit, PyObject *op)
{
	PyObject *arr=NULL;
	PyArrayIterObject *it;
	int index;
	int swap;
	PyArray_Typecode typecode = {0, 0, 0};
        PyArray_CopySwapFunc *copyswap;

	/* Unbound Map Iterator */
	if (mit->ait == NULL) return -1;

	typecode.type_num = mit->ait->ao->descr->type_num;
	typecode.itemsize = mit->ait->ao->itemsize;

	arr = PyArray_FromAny(op, &typecode, 
			      0, 0, FORCECAST);
	if (arr == NULL) return -1;

	if ((mit->subspace != NULL) && (mit->consec)) {
		if (mit->iteraxes[0] > 0) {  /* then we need to swap */
			_swap_axes(mit, (PyArrayObject **)&arr);
		}
	}
	
	if ((it = (PyArrayIterObject *)PyArray_IterNew(arr))==NULL) {
		Py_DECREF(arr);
		return -1;
	}

	index = mit->size;
	swap = ((mit->ait->ao->flags & NOTSWAPPED) != \
		(PyArray_FLAGS(arr) & NOTSWAPPED));

        copyswap = PyArray_DESCR(arr)->copyswap;
	PyArray_MapIterReset(mit);
        /* Need to decref OBJECT arrays */
        if (PyTypeNum_ISOBJECT(typecode.type_num)) {
                while (index--) {
                        Py_XDECREF(*((PyObject **)mit->dataptr));
                        Py_INCREF(*((PyObject **)it->dataptr));
                        memmove(mit->dataptr, it->dataptr, sizeof(PyObject *));
                        copyswap(mit->dataptr, NULL, swap, 
                                  sizeof(PyObject *));
                        PyArray_MapIterNext(mit);
                        PyArray_ITER_NEXT(it);
                        if (it->index == it->size)
                                PyArray_ITER_RESET(it);
                }
		Py_DECREF(arr);
		Py_DECREF(it);
                return 0;
        }

	while(index--) {
		memmove(mit->dataptr, it->dataptr, PyArray_ITEMSIZE(arr));
                copyswap(mit->dataptr, NULL, swap, PyArray_ITEMSIZE(arr));
		PyArray_MapIterNext(mit);
		PyArray_ITER_NEXT(it);
		if (it->index == it->size)
			PyArray_ITER_RESET(it);
	}		
	Py_DECREF(arr);
	Py_DECREF(it);
	return 0;
}

/* Called when treating array object like a mapping -- called first from 
   Python when using a[object] unless object is a standard slice object
   (not an extended one). 

*/

/* There are two situations:  

     1 - the subscript is a standard view and a reference to the 
         array can be returned

     2 - the subscript uses Boolean masks or integer indexing and
         therefore a new array is created and returned. 

*/

/* Always returns 0-dimensional arrays */

static PyObject *
array_subscript(PyArrayObject *self, PyObject *op) 
{
        intp dimensions[MAX_DIMS], strides[MAX_DIMS];
	intp offset;
        int nd;
	intp i;
        PyArrayObject *other;
	PyArrayMapIterObject *mit;

        if (self->nd == 0) {
                PyErr_SetString(PyExc_ValueError, 
                                "0-d arrays can't be indexed.");
                return NULL;
        }
        if (PyArray_IsScalar(op, Integer) || PyInt_Check(op) || \
            PyLong_Check(op)) {
                intp value;
                value = PyArray_PyIntAsIntp(op);
                if (PyErr_Occurred())
                        PyErr_Clear();
                else if (value >= 0) {
			return array_big_item(self, value);
                }
                else if (value < 0) {
			value += self->dimensions[0];
			return array_big_item(self, value);
		}
        }

	if (PyArrayMapIter_Check(op)) {
		mit = (PyArrayMapIterObject *)op;
		/* bind to current array */
		PyArray_MapIterBind(mit, self);
		
		/* If the mapiterator was created with standard indexing
		   behavior, fall through to view-based code */
		if (!mit->view) return PyArray_GetMap(mit);
		op = mit->indexobj;
	}
	else { /* wrap arguments into a mapiter object */
		mit = (PyArrayMapIterObject *)PyArray_MapIterNew(op);
		if (mit == NULL) return NULL;
		if (!mit->view) {  /* fancy indexing */
			PyArray_MapIterBind(mit, self);
			other = (PyArrayObject *)PyArray_GetMap(mit);
			Py_DECREF(mit);
			return (PyObject *)other;
		}
		Py_DECREF(mit);
	}

	i = PyArray_PyIntAsIntp(op);
	if (!error_converting(i)) {
		if (i < 0 && self->nd > 0) i = i+self->dimensions[0];
		return array_big_item(self, i);
	}
	PyErr_Clear();

	/* Standard (view-based) Indexing */
        if ((nd = parse_index(self, op, dimensions, strides, &offset)) 
            == -1) 
                return NULL;

	/* This will only work if new array will be a view */
	if ((other = (PyArrayObject *)					\
	     PyArray_New(self->ob_type, nd, dimensions, self->descr->type_num,
			 strides, self->data+offset, 
			 self->itemsize, self->flags,
			 (PyObject *)self)) == NULL) 
		return NULL;


	other->base = (PyObject *)self;
	Py_INCREF(self);
	
	PyArray_UpdateFlags(other, UPDATE_ALL_FLAGS);
	
	return (PyObject *)other;
}


/* Another assignment hacked by using CopyObject.  */

/* This only works if subscript returns a standard view.  */

/* Again there are two cases.  In the first case, PyArray_CopyObject
   can be used.  In the second case, a new indexing function has to be 
   used.
*/

static int 
array_ass_sub(PyArrayObject *self, PyObject *index, PyObject *op) 
{
        int ret;
	intp i;
        PyArrayObject *tmp;
	PyArrayMapIterObject *mit;
	
        if (op == NULL) {
                PyErr_SetString(PyExc_ValueError, 
                                "cannot delete array elements");
                return -1;
        }
	if (!PyArray_ISWRITEABLE(self)) {
		PyErr_SetString(PyExc_RuntimeError,
				"array is not writeable");
		return -1;
	}
	
        if (PyArray_IsScalar(index, Integer) || PyInt_Check(index) ||	\
            PyLong_Check(index)) {
                intp value;
                value = PyArray_PyIntAsIntp(index);
                if (PyErr_Occurred())
                        PyErr_Clear();
		return array_ass_big_item(self, value, op);
        }

        if (self->nd == 0) {
                PyErr_SetString(PyExc_ValueError, 
                                "0-d arrays can't be indexed.");
                return -1;
        }
	

	if (PyArrayMapIter_Check(index)) {
		mit = (PyArrayMapIterObject *)index;
		/* bind behavior to current array */
		PyArray_MapIterBind(mit, self);
			
		/* fall through if standard view-based map iterator */
		if (!mit->view) return PyArray_SetMap(mit, op);
		index = mit->indexobj;
	}
	else {
		mit = (PyArrayMapIterObject *)PyArray_MapIterNew(index);
		if (mit == NULL) return -1;
		if (!mit->view) {
			PyArray_MapIterBind(mit, self);
			ret = PyArray_SetMap(mit, op);
			Py_DECREF(mit);
			return ret;
		}
		Py_DECREF(mit);
	}

	i = PyArray_PyIntAsIntp(index);
	if (!error_converting(i)) {
		return array_ass_big_item(self, i, op);
	}
	PyErr_Clear();
	
	/* Rest of standard (view-based) indexing */

        if ((tmp = (PyArrayObject *)array_subscript(self, index)) == NULL)
                return -1; 
	if (PyArray_ISOBJECT(self) && (tmp->nd == 0)) {
		if (tmp->descr->setitem(op, tmp->data, tmp) == -1)
			return -1;
		return 0;
	}
	else {
		ret = PyArray_CopyObject(tmp, op);
		Py_DECREF(tmp);
	}	
        return ret;
}

/* There are places that require that array_subscript return a PyArrayObject
   and not possibly a scalar.  Thus, this is the function exposed to 
   Python so that 0-dim arrays are passed as scalars
*/

static PyObject *
array_subscript_nice(PyArrayObject *self, PyObject *op) 
{
	return PyArray_Return((PyArrayObject *)array_subscript(self, op));
}


static PyMappingMethods array_as_mapping = {
        (inquiry)array_length,		    /*mp_length*/
        (binaryfunc)array_subscript_nice,	/*mp_subscript*/
        (objobjargproc)array_ass_sub,	    /*mp_ass_subscript*/
};

/****************** End of Mapping Protocol ******************************/


/*************************************************************************
 ****************   Implement Buffer Protocol ****************************
 *************************************************************************/

/* removed multiple segment interface */

static int 
array_getsegcount(PyArrayObject *self, int *lenp) 
{
        if (lenp)
                *lenp = PyArray_NBYTES(self);

        if (PyArray_ISONESEGMENT(self)) {
                return 1;
        }

        if (lenp)
                *lenp = 0;
        return 0;
}

static int 
array_getreadbuf(PyArrayObject *self, int segment, void **ptrptr) 
{
        if (segment != 0) {
                PyErr_SetString(PyExc_ValueError, 
                                "accessing non-existing array segment");
                return -1;
        }
        
        if (PyArray_ISONESEGMENT(self)) {
                *ptrptr = self->data;
                return PyArray_NBYTES(self);
        }
        PyErr_SetString(PyExc_ValueError, "array is not a single segment");
        *ptrptr = NULL;
        return -1;
}


static int 
array_getwritebuf(PyArrayObject *self, int segment, void **ptrptr) 
{
        if (PyArray_CHKFLAGS(self, WRITEABLE)) 
                return array_getreadbuf(self, segment, (void **) ptrptr);
        else {
                PyErr_SetString(PyExc_ValueError, "array cannot be "\
                                "accessed as a writeable buffer");
                return -1;
        }
}

static int 
array_getcharbuf(PyArrayObject *self, int segment, const char **ptrptr) 
{
        if (self->descr->type_num == PyArray_STRING || \
	    self->descr->type_num == PyArray_UNICODE)
                return array_getreadbuf(self, segment, (void **) ptrptr);
        else {
                PyErr_SetString(PyExc_TypeError, 
                                "non-character array cannot be interpreted "\
                                "as character buffer");
                return -1;
        }
}

static PyBufferProcs array_as_buffer = {
        (getreadbufferproc)array_getreadbuf,    /*bf_getreadbuffer*/
        (getwritebufferproc)array_getwritebuf,  /*bf_getwritebuffer*/
        (getsegcountproc)array_getsegcount,	    /*bf_getsegcount*/
        (getcharbufferproc)array_getcharbuf,    /*bf_getcharbuffer*/
};

/****************** End of Buffer Protocol *******************************/


/*************************************************************************
 ****************   Implement Number Protocol ****************************
 *************************************************************************/


typedef struct {
        PyObject *add,
                *subtract,
                *multiply,
                *divide,
                *remainder,
                *power,
		*sqrt,
                *negative,
                *absolute,
                *invert,
                *left_shift,
                *right_shift,
                *bitwise_and,
                *bitwise_xor,
                *bitwise_or,
                *less,
                *less_equal,
                *equal,
                *not_equal,
                *greater,
                *greater_equal,
                *floor_divide,
                *true_divide,
		*logical_or,
		*logical_and,
		*floor,
		*ceil,
		*maximum,
		*minimum;	
	
} NumericOps;

static NumericOps n_ops = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
                           NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                           NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                           NULL, NULL, NULL, NULL, NULL};

/* Dictionary can contain any of the numeric operations, by name. 
  Those not present will not be changed
 */

#define SET(op)   temp=PyDict_GetItemString(dict, #op);	\
	if (temp != NULL) {				\
		if (!(PyCallable_Check(temp))) return -1; \
                Py_XDECREF(n_ops.op); \
		n_ops.op = temp; \
	}

        
int 
PyArray_SetNumericOps(PyObject *dict) 
{
        PyObject *temp = NULL;
        SET(add);
        SET(subtract);
        SET(multiply);
        SET(divide);
        SET(remainder);
        SET(power);
	SET(sqrt);
        SET(negative);
        SET(absolute);
        SET(invert);
        SET(left_shift);
        SET(right_shift);
        SET(bitwise_and);
        SET(bitwise_or);
        SET(bitwise_xor);
        SET(less);	       
        SET(less_equal);
        SET(equal);
        SET(not_equal);
        SET(greater);
        SET(greater_equal);
        SET(floor_divide);	
        SET(true_divide);	
	SET(logical_or);
	SET(logical_and);
	SET(floor);
	SET(ceil);
	SET(maximum);
	SET(minimum);
        return 0;
}

#define GET(op) if (n_ops.op &&						\
		    (PyDict_SetItemString(dict, #op, n_ops.op)==-1))	\
		goto fail;

static PyObject *
PyArray_GetNumericOps(void) 
{
	PyObject *dict;
	if ((dict = PyDict_New())==NULL) 
		return NULL;	
	GET(add);
        GET(subtract);
        GET(multiply);
        GET(divide);
        GET(remainder);
        GET(power);
	GET(sqrt);
        GET(negative);
        GET(absolute);
        GET(invert);
        GET(left_shift);
        GET(right_shift);
        GET(bitwise_and);
        GET(bitwise_or);
        GET(bitwise_xor);
        GET(less);	  
        GET(less_equal);
        GET(equal);
        GET(not_equal);
        GET(greater);
        GET(greater_equal);
        GET(floor_divide);  
        GET(true_divide); 
	GET(logical_or);
	GET(logical_and);
	GET(floor);
	GET(ceil);
	GET(maximum);
	GET(minimum);
	return dict;	

 fail:
	Py_DECREF(dict);
	return NULL;		
}

static PyObject *
PyArray_GenericReduceFunction(PyArrayObject *m1, PyObject *op, int axis,
			      int rtype)
{
	PyObject *args, *ret=NULL, *meth;
	if (op == NULL) {
		Py_INCREF(Py_NotImplemented);
		return Py_NotImplemented;
	}
	if (rtype == PyArray_NOTYPE) 
		args = Py_BuildValue("(Oi)", m1, axis);
	else {
		PyArray_Descr *descr;
		descr = PyArray_DescrFromType(rtype);
		args = Py_BuildValue("(Oic)", m1, axis, descr->type);
	}
	meth = PyObject_GetAttrString(op, "reduce");
	if (meth && PyCallable_Check(meth)) {
		ret = PyObject_Call(meth, args, NULL);
	}
	Py_DECREF(args);
	Py_DECREF(meth);
	return ret;
}	


static PyObject *
PyArray_GenericAccumulateFunction(PyArrayObject *m1, PyObject *op, int axis,
				  int rtype)
{
	PyObject *args, *ret=NULL, *meth;
	if (op == NULL) {
		Py_INCREF(Py_NotImplemented);
		return Py_NotImplemented;
	}
	if (rtype == PyArray_NOTYPE) 
		args = Py_BuildValue("(Oi)", m1, axis);
	else {
		PyArray_Descr *descr;
		descr = PyArray_DescrFromType(rtype);
		args = Py_BuildValue("(Oic)", m1, axis, descr->type);
	}
	meth = PyObject_GetAttrString(op, "accumulate");
	if (meth && PyCallable_Check(meth)) {
		ret = PyObject_Call(meth, args, NULL);
	}
	Py_DECREF(args);
	Py_DECREF(meth);
	return ret;
}	


static PyObject *
PyArray_GenericBinaryFunction(PyArrayObject *m1, PyObject *m2, PyObject *op) 
{
        PyObject *args, *ret;
        if (op == NULL) {
                Py_INCREF(Py_NotImplemented);
                return Py_NotImplemented; 
        }
        args = Py_BuildValue("(OO)", m1, m2);
        ret = PyObject_Call(op, args, NULL);
        Py_DECREF(args);
        return ret;
}

static PyObject *
PyArray_GenericUnaryFunction(PyArrayObject *m1, PyObject *op) 
{
        PyObject *args, *ret;
        if (op == NULL) {
                Py_INCREF(Py_NotImplemented);
                return Py_NotImplemented; 
        }
        args = Py_BuildValue("(O)", m1);
        ret = PyObject_Call(op, args, NULL);
        Py_DECREF(args);
        return ret;
}

static PyObject *
PyArray_GenericInplaceBinaryFunction(PyArrayObject *m1, 
				     PyObject *m2, PyObject *op) 
{
        PyObject *args, *ret;
        if (op == NULL) {
                Py_INCREF(Py_NotImplemented);
                return Py_NotImplemented; 
        }
        args = Py_BuildValue("(OOO)", m1, m2, m1);
        ret = PyObject_Call(op, args, NULL);
        Py_DECREF(args);
        return ret;
}

static PyObject *
array_add(PyArrayObject *m1, PyObject *m2) 
{ 
        return PyArray_GenericBinaryFunction(m1, m2, n_ops.add); 
}

static PyObject *
array_subtract(PyArrayObject *m1, PyObject *m2) 
{
	return PyArray_GenericBinaryFunction(m1, m2, n_ops.subtract);
}

static PyObject *
array_multiply(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericBinaryFunction(m1, m2, n_ops.multiply);
}

static PyObject *
array_divide(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericBinaryFunction(m1, m2, n_ops.divide);
}

static PyObject *
array_remainder(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericBinaryFunction(m1, m2, n_ops.remainder);
}

static PyObject *
array_power(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericBinaryFunction(m1, m2, n_ops.power);
}

static PyObject *
array_negative(PyArrayObject *m1) 
{ 
        return PyArray_GenericUnaryFunction(m1, n_ops.negative);
}

static PyObject *
array_absolute(PyArrayObject *m1) 
{ 
        return PyArray_GenericUnaryFunction(m1, n_ops.absolute);
}

static PyObject *
array_invert(PyArrayObject *m1) 
{ 
        return PyArray_GenericUnaryFunction(m1, n_ops.invert);
}

static PyObject *
array_left_shift(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericBinaryFunction(m1, m2, n_ops.left_shift);
}

static PyObject *
array_right_shift(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericBinaryFunction(m1, m2, n_ops.right_shift);
}

static PyObject *
array_bitwise_and(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericBinaryFunction(m1, m2, n_ops.bitwise_and);
}

static PyObject *
array_bitwise_or(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericBinaryFunction(m1, m2, n_ops.bitwise_or);
}

static PyObject *
array_bitwise_xor(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericBinaryFunction(m1, m2, n_ops.bitwise_xor);
}

static PyObject *
array_inplace_add(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericInplaceBinaryFunction(m1, m2, n_ops.add);
}

static PyObject *
array_inplace_subtract(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericInplaceBinaryFunction(m1, m2, n_ops.subtract);
}

static PyObject *
array_inplace_multiply(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericInplaceBinaryFunction(m1, m2, n_ops.multiply);
}

static PyObject *
array_inplace_divide(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericInplaceBinaryFunction(m1, m2, n_ops.divide);
}

static PyObject *
array_inplace_remainder(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericInplaceBinaryFunction(m1, m2, n_ops.remainder);
}

static PyObject *
array_inplace_power(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericInplaceBinaryFunction(m1, m2, n_ops.power);
}

static PyObject *
array_inplace_left_shift(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericInplaceBinaryFunction(m1, m2, n_ops.left_shift);
}

static PyObject *
array_inplace_right_shift(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericInplaceBinaryFunction(m1, m2, n_ops.right_shift);
}

static PyObject *
array_inplace_bitwise_and(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericInplaceBinaryFunction(m1, m2, n_ops.bitwise_and);
}

static PyObject *
array_inplace_bitwise_or(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericInplaceBinaryFunction(m1, m2, n_ops.bitwise_or);
}

static PyObject *
array_inplace_bitwise_xor(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericInplaceBinaryFunction(m1, m2, n_ops.bitwise_xor);
}

static PyObject *
array_floor_divide(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericBinaryFunction(m1, m2, n_ops.floor_divide);
}

static PyObject *
array_true_divide(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericBinaryFunction(m1, m2, n_ops.true_divide);
}

static PyObject *
array_inplace_floor_divide(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericInplaceBinaryFunction(m1, m2, 
						    n_ops.floor_divide);
}

static PyObject *
array_inplace_true_divide(PyArrayObject *m1, PyObject *m2) 
{
        return PyArray_GenericInplaceBinaryFunction(m1, m2, 
						    n_ops.true_divide);
}

/* Array evaluates as "TRUE" if any of the elements are non-zero */
/* static int  */
/* array_any_nonzero(PyArrayObject *mp)  */
/* { */
/* 	intp index; */
/* 	PyArrayIterObject *it; */
/* 	Bool anyTRUE = FALSE; */
	
/* 	it = (PyArrayIterObject *)PyArray_IterNew((PyObject *)mp); */
/* 	if (it==NULL) return anyTRUE; */
/* 	index = it->size; */
/* 	while(index--) { */
/* 		if (mp->descr->nonzero(it->dataptr, mp)) { */
/* 			anyTRUE = TRUE; */
/* 			break; */
/* 		} */
/* 		PyArray_ITER_NEXT(it); */
/* 	} */
/* 	Py_DECREF(it); */
/* 	return anyTRUE; */
/* } */

static int
_array_nonzero(PyArrayObject *mp)
{
	if (PyArray_SIZE(mp) == 1) {
		return mp->descr->nonzero(mp->data, mp);
	}
	else {
		PyErr_SetString(PyExc_ValueError, 
				"The truth value of an array " \
				"with more than one element is ambiguous. " \
				"Use a.any() or a.all()");
		return -1;
	}
}



static PyObject *
array_divmod(PyArrayObject *op1, PyObject *op2) 
{
        PyObject *divp, *modp, *result;

        divp = array_floor_divide(op1, op2);
        if (divp == NULL) return NULL;
        modp = array_remainder(op1, op2);
        if (modp == NULL) {
                Py_DECREF(divp);
                return NULL;
        }
        result = Py_BuildValue("OO", divp, modp);
        Py_DECREF(divp);
        Py_DECREF(modp);
        return result;
}


static PyObject *
array_int(PyArrayObject *v) 
{ 
        PyObject *pv, *pv2;
        if (PyArray_SIZE(v) != 1) {
                PyErr_SetString(PyExc_TypeError, "only length-1 arrays can be"\
				" converted to Python scalars");
                return NULL;
        }
        pv = v->descr->getitem(v->data, v);
        if (pv == NULL) return NULL;
        if (pv->ob_type->tp_as_number == 0) {
                PyErr_SetString(PyExc_TypeError, "cannot convert to an int; "\
				"scalar object is not a number");
                Py_DECREF(pv);
                return NULL;
        }
        if (pv->ob_type->tp_as_number->nb_int == 0) {
                PyErr_SetString(PyExc_TypeError, "don't know how to convert "\
				"scalar number to int");
                Py_DECREF(pv);
                return NULL;
        }

        pv2 = pv->ob_type->tp_as_number->nb_int(pv);
        Py_DECREF(pv);
        return pv2;	       
}

static PyObject *
array_float(PyArrayObject *v) 
{
        PyObject *pv, *pv2;
        if (PyArray_SIZE(v) != 1) {
                PyErr_SetString(PyExc_TypeError, "only length-1 arrays can "\
				"be converted to Python scalars");
                return NULL;
        }
        pv = v->descr->getitem(v->data, v);
        if (pv == NULL) return NULL;
        if (pv->ob_type->tp_as_number == 0) {
                PyErr_SetString(PyExc_TypeError, "cannot convert to an "\
				"int; scalar object is not a number");
                Py_DECREF(pv);
                return NULL;
        }
        if (pv->ob_type->tp_as_number->nb_float == 0) {
                PyErr_SetString(PyExc_TypeError, "don't know how to convert "\
				"scalar number to float");
                Py_DECREF(pv);
                return NULL;
        }
        pv2 = pv->ob_type->tp_as_number->nb_float(pv);
        Py_DECREF(pv);
        return pv2;	       
}

static PyObject *
array_long(PyArrayObject *v) 
{    
        PyObject *pv, *pv2;
        if (PyArray_SIZE(v) != 1) {
                PyErr_SetString(PyExc_TypeError, "only length-1 arrays can "\
				"be converted to Python scalars");
                return NULL;
        }
        pv = v->descr->getitem(v->data, v);
        if (pv->ob_type->tp_as_number == 0) {
                PyErr_SetString(PyExc_TypeError, "cannot convert to an int; "\
				"scalar object is not a number");
                return NULL;
        }
        if (pv->ob_type->tp_as_number->nb_long == 0) {
                PyErr_SetString(PyExc_TypeError, "don't know how to convert "\
				"scalar number to long");
                return NULL;
        }
        pv2 = pv->ob_type->tp_as_number->nb_long(pv);
        Py_DECREF(pv);
        return pv2;	       
}

static PyObject *
array_oct(PyArrayObject *v) 
{	      
        PyObject *pv, *pv2;
        if (PyArray_SIZE(v) != 1) {
                PyErr_SetString(PyExc_TypeError, "only length-1 arrays can "\
				"be converted to Python scalars");
                return NULL;
        }
        pv = v->descr->getitem(v->data, v);
        if (pv->ob_type->tp_as_number == 0) {
                PyErr_SetString(PyExc_TypeError, "cannot convert to an int; "\
				"scalar object is not a number");
                return NULL;
        }
        if (pv->ob_type->tp_as_number->nb_oct == 0) {
                PyErr_SetString(PyExc_TypeError, "don't know how to convert "\
				"scalar number to oct");
                return NULL;
        }
        pv2 = pv->ob_type->tp_as_number->nb_oct(pv);
        Py_DECREF(pv);
        return pv2;	       
}

static PyObject *
array_hex(PyArrayObject *v) 
{ 
        PyObject *pv, *pv2;
        if (PyArray_SIZE(v) != 1) {
                PyErr_SetString(PyExc_TypeError, "only length-1 arrays can "\
				"be converted to Python scalars");
                return NULL;
        }
        pv = v->descr->getitem(v->data, v);
        if (pv->ob_type->tp_as_number == 0) {
                PyErr_SetString(PyExc_TypeError, "cannot convert to an int; "\
				"scalar object is not a number");
                return NULL;
        }
        if (pv->ob_type->tp_as_number->nb_hex == 0) {
                PyErr_SetString(PyExc_TypeError, "don't know how to convert "\
				"scalar number to hex");
                return NULL;
        }
        pv2 = pv->ob_type->tp_as_number->nb_hex(pv);
        Py_DECREF(pv);
        return pv2;	       
}

static PyObject *
_array_copy_nice(PyArrayObject *self)
{
	return PyArray_Return((PyArrayObject *)		\
			      PyArray_Copy(self));
}

static PyNumberMethods array_as_number = {
        (binaryfunc)array_add,		    /*nb_add*/
        (binaryfunc)array_subtract,		    /*nb_subtract*/
        (binaryfunc)array_multiply,		    /*nb_multiply*/
        (binaryfunc)array_divide,		    /*nb_divide*/
        (binaryfunc)array_remainder,	           /*nb_remainder*/
        (binaryfunc)array_divmod,		    /*nb_divmod*/
        (ternaryfunc)array_power,		    /*nb_power*/
        (unaryfunc)array_negative,                  /*nb_neg*/	
        (unaryfunc)_array_copy_nice,		    /*nb_pos*/ 
        (unaryfunc)array_absolute,		    /*(unaryfunc)array_abs,*/
        (inquiry)_array_nonzero,		    /*nb_nonzero*/
        (unaryfunc)array_invert,		    /*nb_invert*/
        (binaryfunc)array_left_shift,	    /*nb_lshift*/
        (binaryfunc)array_right_shift,	    /*nb_rshift*/
        (binaryfunc)array_bitwise_and,	    /*nb_and*/
        (binaryfunc)array_bitwise_xor,	    /*nb_xor*/
        (binaryfunc)array_bitwise_or,	    /*nb_or*/
        0,		                    /*nb_coerce*/
        (unaryfunc)array_int,		    /*nb_int*/
        (unaryfunc)array_long,		    /*nb_long*/
        (unaryfunc)array_float,		    /*nb_float*/
        (unaryfunc)array_oct,		    /*nb_oct*/
        (unaryfunc)array_hex,		    /*nb_hex*/

        /*This code adds augmented assignment functionality*/
        /*that was made available in Python 2.0*/
        (binaryfunc)array_inplace_add,	    /*inplace_add*/
        (binaryfunc)array_inplace_subtract,	    /*inplace_subtract*/
        (binaryfunc)array_inplace_multiply,	    /*inplace_multiply*/
        (binaryfunc)array_inplace_divide,	    /*inplace_divide*/
        (binaryfunc)array_inplace_remainder,    /*inplace_remainder*/
        (ternaryfunc)array_inplace_power,	    /*inplace_power*/
        (binaryfunc)array_inplace_left_shift,   /*inplace_lshift*/
        (binaryfunc)array_inplace_right_shift,  /*inplace_rshift*/
        (binaryfunc)array_inplace_bitwise_and,  /*inplace_and*/
        (binaryfunc)array_inplace_bitwise_xor,  /*inplace_xor*/
        (binaryfunc)array_inplace_bitwise_or,   /*inplace_or*/

        (binaryfunc)array_floor_divide,	     /*nb_floor_divide*/
        (binaryfunc)array_true_divide,	     /*nb_true_divide*/
        (binaryfunc)array_inplace_floor_divide,  /*nb_inplace_floor_divide*/
        (binaryfunc)array_inplace_true_divide,   /*nb_inplace_true_divide*/

};

/****************** End of Buffer Protocol *******************************/


/*************************************************************************
 ****************   Implement Sequence Protocol **************************
 *************************************************************************/

/* Some of this is repeated in the array_as_mapping protocol.  But
   we fill it in here so that PySequence_XXXX calls work as expected 
*/


static PyObject * 
array_slice(PyArrayObject *self, int ilow, int ihigh) 
{
        PyArrayObject *r;
        int l;
        char *data;

        if (self->nd == 0) {
                PyErr_SetString(PyExc_ValueError, "cannot slice a scalar");
                return NULL;
        }
        	
        l=self->dimensions[0];
        if (ihigh < 0) ihigh += l;
        if (ilow  < 0) ilow += l;
        if (ilow < 0) ilow = 0;
        else if (ilow > l) ilow = l;
        if (ihigh < 0) ihigh = 0;
        else if (ihigh > l) ihigh = l;
        if (ihigh < ilow) ihigh = ilow;

        if (ihigh != ilow) {
                data = index2ptr(self, ilow);
                if (data == NULL) return NULL;
        } else {
                data = self->data;
        }

        self->dimensions[0] = ihigh-ilow;
        r = (PyArrayObject *)\
		PyArray_New(self->ob_type, self->nd, self->dimensions, 
			    self->descr->type_num, self->strides, data,
			    self->itemsize, self->flags, (PyObject *)self);

        self->dimensions[0] = l;
        r->base = (PyObject *)self;
        Py_INCREF(self);
	PyArray_UpdateFlags(r, UPDATE_ALL_FLAGS); 
        return (PyObject *)r;
}


static int 
array_ass_slice(PyArrayObject *self, int ilow, int ihigh, PyObject *v) {
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
        if ((tmp = (PyArrayObject *)array_slice(self, ilow, ihigh)) \
            == NULL) 
                return -1;  
        ret = PyArray_CopyObject(tmp, v);
        Py_DECREF(tmp);
	
        return ret;
}

static int
array_contains(PyArrayObject *self, PyObject *el)
{
        /* equivalent to any(self == el) */

        return PyObject_RichCompareBool((PyObject *)self, el, Py_EQ);
}


static PySequenceMethods array_as_sequence = {
        (inquiry)array_length,		/*sq_length*/
        (binaryfunc)NULL, /* sq_concat is handled by nb_add*/
        (intargfunc)NULL, /* sq_repeat is handled nb_multiply*/
        (intargfunc)array_item_nice,		/*sq_item*/
        (intintargfunc)array_slice,		/*sq_slice*/
        (intobjargproc)array_ass_item,	/*sq_ass_item*/
        (intintobjargproc)array_ass_slice,	/*sq_ass_slice*/
	(objobjproc) array_contains,           /* sq_contains */
	(binaryfunc) NULL,         /* sg_inplace_concat */
	(intargfunc) NULL         /* sg_inplace_repeat */
};


/****************** End of Sequence Protocol ****************************/


static int 
dump_data(char **string, int *n, int *max_n, char *data, int nd, 
          intp *dimensions, intp *strides, PyArrayObject* self) 
{
        PyArray_Descr *descr=self->descr;
        PyObject *op, *sp;
        char *ostring;
        int i, N;
	
#define CHECK_MEMORY if (*n >= *max_n-16) { *max_n *= 2; \
		*string = (char *)realloc(*string, *max_n); }
	
        if (nd == 0) {
		
                if ((op = descr->getitem(data, self)) == NULL) return -1;
                sp = PyObject_Repr(op);
                if (sp == NULL) {Py_DECREF(op); return -1;}
                ostring = PyString_AsString(sp);
                N = PyString_Size(sp)*sizeof(char);
                *n += N;
                CHECK_MEMORY
                        memmove(*string+(*n-N), ostring, N);
                Py_DECREF(sp);
                Py_DECREF(op);
                return 0;
        } else {
                CHECK_MEMORY
                        (*string)[*n] = '[';
                *n += 1;
                for(i=0; i<dimensions[0]; i++) {
                        if (dump_data(string, n, max_n, 
                                      data+(*strides)*i, 
                                      nd-1, dimensions+1, 
                                      strides+1, self) < 0) 
                                return -1;
                                CHECK_MEMORY
                                        if (i<dimensions[0]-1) {
                                                (*string)[*n] = ','; 
                                                (*string)[*n+1] = ' ';
                                                *n += 2;
                                        }
                }
                CHECK_MEMORY
                        (*string)[*n] = ']'; *n += 1;
                return 0;
        }

#undef CHECK_MEMORY
}

static PyObject * 
array_repr_builtin(PyArrayObject *self) 
{
        PyObject *ret;
        char *string;
        int n, max_n;
	
        max_n = PyArray_NBYTES(self)*4*sizeof(char) + 7;
	
        if ((string = (char *)malloc(max_n)) == NULL) {
                PyErr_SetString(PyExc_MemoryError, "out of memory");
                return NULL;
        }
	
        n = 6;
        sprintf(string, "array(");
	
        if (dump_data(&string, &n, &max_n, self->data, 
		      self->nd, self->dimensions, 
                      self->strides, self) < 0) { 
		free(string); return NULL; 
	}
	
	if (PyArray_ISFLEXIBLE(self)) {
		char buf[100];
		snprintf(buf, sizeof(buf), "%d", self->itemsize);
		sprintf(string+n, ", '%c%s')", self->descr->type, buf);
		ret = PyString_FromStringAndSize(string, n+6+strlen(buf));
	}
	else {
		sprintf(string+n, ", '%c')", self->descr->type);
		ret = PyString_FromStringAndSize(string, n+6);
	}
	

        free(string);
        return ret;
}

static PyObject *PyArray_StrFunction=NULL;
static PyObject *PyArray_ReprFunction=NULL;

static void 
PyArray_SetStringFunction(PyObject *op, int repr) 
{
        if (repr) {
		/* Dispose of previous callback */
                Py_XDECREF(PyArray_ReprFunction); 
		/* Add a reference to new callback */
                Py_XINCREF(op); 
		/* Remember new callback */
                PyArray_ReprFunction = op; 
        } else {
		/* Dispose of previous callback */
                Py_XDECREF(PyArray_StrFunction); 
		/* Add a reference to new callback */
                Py_XINCREF(op); 
		/* Remember new callback */
                PyArray_StrFunction = op; 
        }
}

static PyObject *
array_repr(PyArrayObject *self) 
{
        PyObject *s, *arglist;
	
        if (PyArray_ReprFunction == NULL) {
                s = array_repr_builtin(self);
        } else {
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
                s = array_repr(self);
        } else {
                arglist = Py_BuildValue("(O)", self);
                s = PyEval_CallObject(PyArray_StrFunction, arglist);
                Py_DECREF(arglist); 
        }
        return s;
}


static PyObject *
array_richcompare(PyArrayObject *self, PyObject *other, int cmp_op) 
{
        PyObject *array_other, *result;

        switch (cmp_op) 
                {
                case Py_LT:
                        return PyArray_GenericBinaryFunction(self, other, 
							     n_ops.less);
                case Py_LE:
                        return PyArray_GenericBinaryFunction(self, other, 
							     n_ops.less_equal);
                case Py_EQ:
                        /* Try to convert other to an array */
                        array_other = PyArray_FromObject(other, 
							 PyArray_NOTYPE, 0, 0);
                        /* If not successful, then return the integer
			   object 0. This fixes code that used to
			   allow equality comparisons between arrays
			   and other objects which would give a result
			   of 0
                        */
                        if ((array_other == NULL) || \
			    (array_other == Py_None)) {
                                Py_XDECREF(array_other);
                                PyErr_Clear();
                                Py_INCREF(Py_False);
                                return Py_False;
                        }
                        result = PyArray_GenericBinaryFunction(self, 
							       array_other, 
							       n_ops.equal);
                        /* If the comparison results in NULL, then the 
			   two array objects can not be compared together so 
			   return zero 
                        */
                        Py_DECREF(array_other);
                        if (result == NULL) {
                                PyErr_Clear();
                                Py_INCREF(Py_False);
                                return Py_False;
                        }
                        return result;
                case Py_NE:
                        /* Try to convert other to an array */
                        array_other = PyArray_FromObject(other, 
							 PyArray_NOTYPE, 0, 0);
                        /* If not successful, then objects cannot be 
			   compared and cannot be equal, therefore, 
			   return True;
                        */
                        if ((array_other == NULL) || \
			    (array_other == Py_None)) {
                                Py_XDECREF(array_other);
                                PyErr_Clear();
                                Py_INCREF(Py_True);
                                return Py_True;
                        }
                        result = PyArray_GenericBinaryFunction(self, 
							       array_other, 
							     n_ops.not_equal);
                        Py_DECREF(array_other);
                        if (result == NULL) {
                                PyErr_Clear();
                                Py_INCREF(Py_True);
                                return Py_True;
                        }
                        return result;
                case Py_GT:
                        return PyArray_GenericBinaryFunction(self, other, 
							     n_ops.greater);
                case Py_GE:
                        return PyArray_GenericBinaryFunction(self, 
							     other, 
					         	 n_ops.greater_equal);
                }
        return NULL;
}

static PyObject *
_check_axis(PyArrayObject *arr, int *axis, int flags)
{
	PyObject *temp;
	int n = arr->nd;

	if ((*axis >= MAX_DIMS) || (n==0)) {
		temp = PyArray_Ravel(arr,0);
		*axis = 0;
		return temp;
	}
	else {
		if (flags) {
			temp = PyArray_FromAny((PyObject *)arr, NULL, 
					       0, 0, flags);
			if (temp == NULL) return NULL;
		}
		else {
			Py_INCREF(arr);
			temp = (PyObject *)arr;
		}
	}
	if (*axis < 0) *axis += n;
	if ((*axis < 0) || (*axis >= n)) {
		PyErr_Format(PyExc_ValueError, 
			     "axis(=%d) out of bounds", *axis);
		Py_DECREF(temp);
		return NULL;
	}
	return temp;
}

#include "arraymethods.c"

/* Lifted from numarray */
static PyObject *
PyArray_IntTupleFromIntp(int len, intp *vals)
{
	int i;
        PyObject *intTuple = PyTuple_New(len);
        if (!intTuple) goto fail;
        for(i=0; i<len; i++) {
                PyObject *o = PyInt_FromLong(vals[i]);
                if (!o) {
                        Py_DECREF(intTuple);
                        intTuple = NULL;
                        goto fail;
                }
                PyTuple_SET_ITEM(intTuple, i, o);
        }
  fail:
        return intTuple;	
}

/* Returns the number of dimensions or -1 if an error occurred */
/*  vals must be large enough to hold maxvals */
static int
PyArray_IntpFromSequence(PyObject *seq, intp *vals, int maxvals) 
{
        int nd, i;
	PyObject *op;
        
        /* Check to see if sequence is a single integer first. 
             or, can be made into one */
	if ((nd=PySequence_Length(seq)) == -1) {
		if (PyErr_Occurred()) PyErr_Clear();
		if (!(op = PyNumber_Int(seq))) return -1;
		nd = 1;
		vals[0] = (intp ) PyInt_AsLong(op);
		Py_DECREF(op);
	} else {
		for(i=0; i < MIN(nd,maxvals); i++) {
			op = PySequence_GetItem(seq, i);
			if (op == NULL) return -1;
			vals[i]=(intp )PyInt_AsLong(op);
			Py_DECREF(op);
			if(PyErr_Occurred()) return -1;
		}
	}
	return nd;
}


/* Check whether the given array is stored contiguously (row-wise) in
   memory. */
static int
_IsContiguous(PyArrayObject *ap) 
{
	intp sd;
	int i;

	if (ap->nd == 0) return 1;
	sd = ap->itemsize;
	if (ap->nd == 1) return sd == ap->strides[0];
	for (i = ap->nd-1; i >= 0; --i) {
		/* contiguous by definition */
		if (ap->dimensions[i] == 0) return 1; 
		
		if (ap->strides[i] != sd) return 0;
		sd *= ap->dimensions[i];
	}
	return 1;
}


static int 
_IsFortranContiguous(PyArrayObject *ap) 
{
	intp sd;
	int i;
	
	if (ap->nd == 0) return 1;
	sd = ap->itemsize;
	if (ap->nd == 1) return sd == ap->strides[0];
	for (i=0; i< ap->nd; ++i) {
		/* contiguous by definition */
		if (ap->dimensions[i] == 0) return 1; 
		
		if (ap->strides[i] != sd) return 0;
		sd *= ap->dimensions[i];
	}
	return 1;
}

static int
_IsAligned(PyArrayObject *ap) 
{
	int i, alignment, aligned=1;
	intp ptr;
	int type = ap->descr->type_num;

	if ((type == PyArray_STRING) || (type == PyArray_VOID))
		return 1;

	alignment = ap->descr->alignment;

	ptr = (intp) ap->data;
        aligned = (ptr % alignment) == 0;
        for (i=0; i <ap->nd; i++)
                aligned &= ((ap->strides[i] % alignment) == 0);
        return aligned != 0;
}

static Bool
_IsWriteable(PyArrayObject *ap)
{
	PyObject *base=ap->base;
	PyBufferProcs *pb;

	/* If we own our own data, then no-problem */
	if ((base == NULL) || (ap->flags & OWN_DATA)) return TRUE;

	/* Get to the final base object 
	   If it is a writeable array, then return TRUE
	   If we can find an array object 
	   or a writeable buffer object as the final base object
	   or a string object (for pickling support memory savings).
	     - this last could be removed if a proper pickleable 
	       buffer was added to Python.
	*/

	while(PyArray_Check(base)) {
		if (PyArray_CHKFLAGS(base, OWN_DATA)) 
			return (Bool) (PyArray_ISWRITEABLE(base));
		base = PyArray_BASE(base);
	}

	/* here so pickle support works seamlessly 
	   and unpickled array can be set and reset writeable 
	   -- could be abused -- */
	if PyString_Check(base) return TRUE;

	pb = base->ob_type->tp_as_buffer;
	if (pb == NULL || pb->bf_getwritebuffer == NULL)
		return FALSE;
	
	return TRUE;
}


static void
PyArray_UpdateFlags(PyArrayObject *ret, int flagmask)
{

	if (flagmask & FORTRAN) {
		if (_IsFortranContiguous(ret)) {
			ret->flags |= FORTRAN;
			if (ret->nd > 1) ret->flags &= ~CONTIGUOUS;
		}
		else ret->flags &= ~FORTRAN;
	}
	if (flagmask & CONTIGUOUS) {
		if (_IsContiguous(ret)) {
			ret->flags |= CONTIGUOUS;
			if (ret->nd > 1) ret->flags &= ~FORTRAN;
		}
		else ret->flags &= ~CONTIGUOUS;
	}
	if (flagmask & ALIGNED) {
		if (_IsAligned(ret)) ret->flags |= ALIGNED;
		else ret->flags &= ~ALIGNED;
	}
	/* This is not checked by default WRITEABLE is not part of UPDATE_ALL_FLAGS */
	if (flagmask & WRITEABLE) {
	        if (_IsWriteable(ret)) ret->flags |= WRITEABLE;
	       	else ret->flags &= ~WRITEABLE;	
        }
	return;
}

/* This routine checks to see if newstrides (of length nd) will not 
 walk outside of the memory implied by either numbytes or
 a single segment array of the provided dimensions and element size if
 numbytes is 0 */
static Bool
PyArray_CheckStrides(int elsize, int nd, intp numbytes, 
		     intp *dims, intp *newstrides)
{
	int i;
	
	if (numbytes == 0) 
		numbytes = PyArray_MultiplyList(dims, nd) * elsize;
	
	for (i=0; i<nd; i++) {
		if (newstrides[i]*(dims[i]-1)+elsize > numbytes) {
			return FALSE;
		}
	}
	return TRUE;
	
}


/* This is the main array creation routine. */

/* Flags argument has multiple related meanings 
   depending on data and strides: 

   If data is given, then flags is flags associated with data.  
   If strides is not given, then a contiguous strides array will be created
   and the CONTIGUOUS bit will be set.  If the flags argument 
   has the FORTRAN bit set, then a FORTRAN-style strides array will be
   created (and of course the FORTRAN flag bit will be set). 

   If data is not given but created here, then flags will be DEFAULT_FLAGS
   and a non-zero flags argument can be used to indicate a FORTRAN style
   array is desired. 
*/

static intp
_array_fill_strides(intp *strides, intp *dims, int nd, intp itemsize, 
		    int inflag, int *objflags) 
{
	int i;
	/* Only make Fortran strides if not contiguous as well */
	if ((inflag & FORTRAN) && !(inflag & CONTIGUOUS)) {
		for (i=0; i<nd; i++) {
			strides[i] = itemsize;
			itemsize *= dims[i] ? dims[i] : 1;
		}
		*objflags |= FORTRAN;
		if (nd > 1) *objflags &= ~CONTIGUOUS;
		else *objflags |= CONTIGUOUS;
	}
	else {
		for (i=nd-1;i>=0;i--) {
			strides[i] = itemsize;
			itemsize *= dims[i] ? dims[i] : 1;
		}
		*objflags |= CONTIGUOUS;
		if (nd > 1) *objflags &= ~FORTRAN;
		else *objflags |= FORTRAN;
	}
	return itemsize;
}

	
static PyObject *
PyArray_New(PyTypeObject *subtype, int nd, intp *dims, int type_num,
            intp *strides, void *data, int itemsize, int flags,
	    PyObject *obj)
{
	PyArrayObject *self;
	PyArray_Descr *descr;
	register int i;
	intp sd, temp=-1;

	descr = PyArray_DescrFromType(type_num);
	if (descr == NULL) return NULL;

	if (nd < 0) {
		PyErr_SetString(PyExc_ValueError,
				"number of dimensions must be >=0");
		return NULL;
	}
        if (nd > MAX_DIMS) {
                PyErr_Format(PyExc_ValueError,
                             "maximum number of dimensions is %d", MAX_DIMS);
                return NULL;
	}

	/* Check dimensions */
	for (i=nd-1;i>=0;i--) {
		if (dims[i] < 0) {
			PyErr_SetString(PyExc_ValueError,
					"negative dimensions "	\
					"are not allowed");
			return NULL;
		}
	}
	
	self = (PyArrayObject *) subtype->tp_alloc(subtype, 0);
	if (self == NULL) return NULL;	
	self->descr = descr;
	self->dimensions = NULL;
	if (data == NULL) {  /* strides is NULL too */
		self->flags = DEFAULT_FLAGS;
		if (flags) {
			self->flags |= FORTRAN; 
			if (nd > 1) self->flags &= ~CONTIGUOUS;
			flags = FORTRAN;
		}
	}
	else self->flags = (flags & ~UPDATEIFCOPY);
	
	if (PyTypeNum_ISFLEXIBLE(type_num)) {
		if (itemsize < 1) {
			PyErr_SetString(PyExc_ValueError,
					"type must provide an itemsize");
			self->ob_type->tp_free((PyObject *)self);
			return NULL;
		}
		self->itemsize = itemsize;
		/* Guarantee that these kind of arrays are never byteswapped
		   unknowingly.  
		*/
		if (type_num != PyArray_UNICODE)
			self->flags |= NOTSWAPPED;
	}
	else self->itemsize = descr->elsize; 
		
	sd = self->itemsize;
	
	if (nd > 0) {
		self->dimensions = PyDimMem_NEW(2*nd);
		if (self->dimensions == NULL) {
			self->ob_type->tp_free((PyObject *)self);
			return PyErr_NoMemory();
		}
		self->strides = self->dimensions + nd;
		memcpy(self->dimensions, dims, sizeof(intp)*nd);
		if (strides == NULL) { /* fill it in */
			sd = _array_fill_strides(self->strides, dims, nd, sd,
						 flags, &(self->flags));
		}
		else {
			if (data == NULL) {
				PyErr_SetString(PyExc_ValueError, 
						"if 'strides' is given in " \
						"array creation, data must " \
						"be given too");
				PyDimMem_FREE(self->dimensions);
				self->ob_type->tp_free((PyObject *)self);
				return NULL;
			}				
			memcpy(self->strides, strides, sizeof(intp)*nd);
		}
	}
       	
		
	if (data == NULL) {

		/* Allocate something even for zero-space arrays 
		 e.g. shape=(0,) -- otherwise buffer exposure (a.data) doesn't work
		 as it should. */

		if (sd==0) sd = sizeof(intp);

		if ((data = PyDataMem_NEW(sd))==NULL) {
			PyDimMem_FREE(self->dimensions);
			self->ob_type->tp_free((PyObject *)self);
			return PyErr_NoMemory();
		}
		self->flags |= OWN_DATA;

		/* It is bad to have unitialized OBJECT pointers */
		/* We shouldn't need to check for the OBJECT Letter
		   but perhaps it's best. */
		if (type_num == PyArray_OBJECT || \
		    type_num == PyArray_OBJECTLTR) {
			memset(data, 0, sd);
		}
	}
	else {
                self->flags &= ~OWN_DATA;  /* If data is passed in, 
					   this object won't own it 
					   by default.
					   Caller must arrange for 
					   this to be reset if truly
					   desired */
        }
        self->data = data;
	self->nd = nd;
	self->base = (PyObject *)NULL;
        self->weakreflist = (PyObject *)NULL;

        /* call the __array_finalize__
	   method if a subtype and some object passed in */
	if ((obj != NULL) && (subtype != &PyArray_Type) && 
	    (subtype != &PyBigArray_Type)) {
		PyObject *res;
		if (temp==-1) { /* did not allocate own data */
			/* update flags before calling back into
			   Python */
			PyArray_UpdateFlags(self, UPDATE_ALL_FLAGS);
		}
		res = PyObject_CallMethod((PyObject *)self, 
					  "__array_finalize__",
					  "O", obj);
		if (res == NULL) {
			PyDimMem_FREE(self->dimensions);
			self->ob_type->tp_free((PyObject *)self);
			return NULL;
		}
		else Py_DECREF(res);
	}

	return (PyObject *)self;
}



static PyObject * 
PyArray_Resize(PyArrayObject *self, PyArray_Dims *newshape)
{
        intp oldsize, newsize;
        int new_nd=newshape->len, k, n, elsize;
        int refcnt;
        intp* new_dimensions=newshape->ptr;
        intp new_strides[MAX_DIMS];
        intp sd;
        intp *dimptr;
        char *new_data;
	
        if (!PyArray_ISCONTIGUOUS(self)) {
                PyErr_SetString(PyExc_ValueError, 
                                "resize only works on contiguous arrays");
                return NULL;
        }


        newsize = PyArray_MultiplyList(new_dimensions, new_nd);

        if (newsize == 0) {
                PyErr_SetString(PyExc_ValueError, 
                                "newsize is zero; cannot delete an array "\
                                "in this way");
                return NULL;
        }
        oldsize = PyArray_SIZE(self);
        
	if (oldsize != newsize) {
		if (!(self->flags & OWN_DATA)) {
			PyErr_SetString(PyExc_ValueError, 
					"cannot resize this array:  "	\
					"it does not own its data");
			return NULL;
		}
		
		refcnt = REFCOUNT(self);
		if ((refcnt > 2) || (self->base != NULL) ||     \
		    (self->weakreflist != NULL)) {
			PyErr_SetString(PyExc_ValueError, 
					"cannot resize an array that has "\
					"been referenced or is referencing\n"\
					"another array in this way.  Use the "\
					"resize function");
			return NULL;
		} 
		
		/* Reallocate space if needed */
		new_data = PyDataMem_RENEW(self->data, 
					   newsize*(self->itemsize));
		if (new_data == NULL) {
			PyErr_SetString(PyExc_MemoryError, 
					"cannot allocate memory for array");
			return NULL;
		}
		self->data = new_data;
	}
        
        if ((newsize > oldsize) && PyArray_ISWRITEABLE(self)) {  
		/* Fill new memory with zeros */
                elsize = self->itemsize;
		if ((PyArray_TYPE(self) == PyArray_OBJECT)) {
			PyObject *zero = PyInt_FromLong(0);
                        PyObject **optr;
			optr = ((PyObject **)self->data) + oldsize;
			n = newsize - oldsize;
			for (k=0; k<n; k++) {
                                Py_INCREF(zero);
                                *optr++ = zero;
			}
			Py_DECREF(zero);
		}
		else{		
			memset(self->data+oldsize*elsize, 0, 
			       (newsize-oldsize)*elsize);
		}
	}
        
        if (self->nd != new_nd) {  /* Different number of dimensions. */
                self->nd = new_nd;
                
                /* Need new dimensions and strides arrays */
                dimptr = PyDimMem_RENEW(self->dimensions, 2*new_nd);
                if (dimptr == NULL) {
			PyErr_SetString(PyExc_MemoryError, 
                                        "cannot allocate memory for array " \
                                        "(array may be corrupted)");
                        return NULL;
                }
                self->dimensions = dimptr;
		self->strides = dimptr + new_nd;
        }

        /* make new_strides variable */
        sd = (intp) self->itemsize;
        sd = _array_fill_strides(new_strides, new_dimensions, new_nd, sd,
                                 0, &(self->flags));

        
        memmove(self->dimensions, new_dimensions, new_nd*sizeof(intp));
        memmove(self->strides, new_strides, new_nd*sizeof(intp));

        Py_INCREF(Py_None);	
        return Py_None;
        
}


/* Assumes contiguous */
static void
PyArray_FillObjectArray(PyArrayObject *arr, PyObject *obj)
{
        PyObject **optr;
        intp i,n;
        optr = (PyObject **)(arr->data);
        n = PyArray_SIZE(arr);
        if (obj == NULL) {
                for (i=0; i<n; i++) {
                        *optr++ = NULL;
                }
        }
        else {
                for (i=0; i<n; i++) {
                        Py_INCREF(obj);
                        *optr++ = obj;
                }
        }
}        

static int
PyArray_FillWithScalar(PyArrayObject *arr, PyObject *obj)
{
	PyObject *newarr;
	int itemsize;
	void *fromptr;
	PyArray_Typecode type = {0,0,0};
	intp size;

	type.type_num = PyArray_TYPE(arr);
	itemsize = PyArray_ITEMSIZE(arr);
	type.itemsize = itemsize;
	newarr = PyArray_FromAny(obj, &type, 0,0, BEHAVED_FLAGS_RO);
	if (newarr == NULL) return -1;
	fromptr = PyArray_DATA(newarr);
	size=PyArray_SIZE(arr);
	if (PyArray_ISONESEGMENT(arr)) {
		char *toptr=PyArray_DATA(arr);
		while(size--) {
			memcpy(toptr, fromptr, itemsize);
			toptr += itemsize;
		}
	}
	else {
		PyArrayIterObject *iter;
		PyObject *behaved;
		
		behaved = PyArray_FromAny((PyObject *)arr, NULL, 0,0, 
					  BEHAVED_FLAGS | UPDATEIFCOPY);
		if (behaved == NULL) { 
			Py_DECREF(newarr);
			return -1;
		}
		
		iter = (PyArrayIterObject *)		\
			PyArray_IterNew(behaved);
		
		if (iter == NULL) {
			Py_DECREF(behaved);
			Py_DECREF(newarr);
			return -1;
		}

		while(size--) {
			memcpy(iter->dataptr, fromptr, itemsize);
			PyArray_ITER_NEXT(iter);
		}

		Py_DECREF(iter);
		Py_DECREF(behaved);
	}
	Py_DECREF(newarr);
	return 0;
}

static PyObject *
array_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) 
{
	static char *kwlist[] = {"shape", "dtype", "buffer", "strides", 
                                 "swap", "fortran", NULL};
	int itemsize = 0;
	PyArray_Typecode typecode = {PyArray_NOTYPE, 0, 0};
	int type_num = PyArray_NOTYPE;
        PyArray_Dims dims = {NULL, 0};
        PyArray_Dims strides = {NULL, 0};
        PyArray_Chunk buffer;
	int fortran = 0;
        int swapped = 0;
	PyArrayObject *ret;

	buffer.ptr = NULL; 
        /* Usually called with shape and type
           but can also be called with buffer, strides, and swapped info
        */

	/* For now, let's just use this to create an empty, contiguous 
	   array of a specific type and shape. 
	*/

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&|O&O&O&ii",
					 kwlist, PyArray_IntpConverter,
                                         &dims, 
                                         PyArray_TypecodeConverter,
					 &typecode, 
                                         PyArray_BufferConverter,
                                         &buffer,
                                         &PyArray_IntpConverter, 
                                         &strides,
                                         &swapped, &fortran)) 
		goto fail;
	
        if (typecode.type_num ==PyArray_NOTYPE) {
		typecode.type_num = PyArray_DOUBLE;
		typecode.itemsize = sizeof(double);
	}
	type_num = typecode.type_num;
	itemsize = typecode.itemsize;


        if (dims.ptr == NULL) {
                PyErr_SetString(PyExc_ValueError, "need to give a "\
                                "valid shape as the first argument");
                goto fail;
        }
        if (buffer.ptr == NULL) {
                ret = (PyArrayObject *)PyArray_New(subtype, (int)dims.len, 
                                                   dims.ptr, type_num,
                                                   NULL, NULL, itemsize, 
                                                   fortran, NULL);
                
                if (ret == NULL) goto fail;
                if (type_num == PyArray_OBJECT) { /* place Py_None */
                        PyArray_FillObjectArray(ret, Py_None);
                }
        }
        else {  /* buffer given -- use it */
                if (dims.len == 1 && dims.ptr[0] == -1) {
                        dims.ptr[0] = buffer.len / itemsize;
                }
                else if (buffer.len < itemsize*                 \
                         PyArray_MultiplyList(dims.ptr, dims.len)) {
                        PyErr_SetString(PyExc_TypeError, 
                                        "buffer is too small for "      \
                                        "requested array");
                        goto fail;
                }
                if (strides.ptr != NULL) {
			if (strides.len != dims.len) {
				PyErr_SetString(PyExc_ValueError, 
						"strides, if given, must be "\
						"the same length as shape");
				goto fail;
			}
			if (!PyArray_CheckStrides(itemsize, strides.len, 
						  buffer.len,
						  dims.ptr, strides.ptr)) {
				PyErr_SetString(PyExc_ValueError, 
						"strides is incompatible "\
						"with shape of requested"\
						"array and size of buffer");
				goto fail;
			}
                }
                if (type_num == PyArray_OBJECT) {
                        PyErr_SetString(PyExc_TypeError, "cannot construct "\
                                        "an object array from buffer data");
                        goto fail;
                }
                /* get writeable and aligned */
                if (fortran) buffer.flags |= FORTRAN;
                if (!(swapped)) buffer.flags |= NOTSWAPPED;
                ret = (PyArrayObject *)PyArray_New(subtype, dims.len, 
                                                   dims.ptr, type_num,
                                                   strides.ptr,
                                                   (char *)buffer.ptr, 
                                                   itemsize, 
                                                   buffer.flags, NULL); 
                if (ret == NULL) goto fail;    
                PyArray_UpdateFlags(ret, UPDATE_ALL_FLAGS);
                ret->base = buffer.base;
                Py_INCREF(buffer.base); 
        }

        PyDimMem_FREE(dims.ptr);
        if (strides.ptr) PyDimMem_FREE(strides.ptr);
        return PyArray_Return(ret);
        
 fail:
        if (dims.ptr) PyDimMem_FREE(dims.ptr);
        if (strides.ptr) PyDimMem_FREE(strides.ptr);
        return NULL;
}


static PyObject *
array_iter(PyArrayObject *arr)
{
	if (arr->nd == 0) {
		PyErr_SetString(PyExc_TypeError,
				"iteration over a scalar (0-dim array)");
		return NULL;
	}
	return PySeqIter_New((PyObject *)arr);
}


/*******************  array attribute get and set routines ******************/

static PyObject *
array_ndim_get(PyArrayObject *self)
{
	return PyInt_FromLong(self->nd);
}

static PyObject *
array_flags_get(PyArrayObject *self)
{
        static PyObject *module=NULL;

        if (module==NULL) {
                module = PyImport_ImportModule("scipy.base._internal");
                if (module == NULL) return NULL;
        }
        return PyObject_CallMethod(module, "flagsobj", "Oii", 
                                   self, self->flags, 0);
}

/*
static int
array_flags_set(PyArrayObject *self, PyObject *obj) 
{
	int flagback = self->flags;

        if (PyDict_Check(obj)) {
                PyObject *new;
		new = PyDict_GetItemString(obj, "ALIGNED");
		if (new) {
			if (PyObject_Not(new)) self->flags &= ~ALIGNED;
			else if (_IsAligned(self)) self->flags |= ALIGNED;
			else {
				PyErr_SetString(PyExc_ValueError,
						"cannot set aligned flag of " \
						"mis-aligned array to True");
				return -1;
			}
		}
                new = PyDict_GetItemString(obj, "UPDATEIFCOPY");
                if (new) {
                        if (PyObject_Not(new)) {
                                self->flags &= ~UPDATEIFCOPY;
                                Py_DECREF(self->base);
                                self->base = NULL;
                        }
                        else {
				self->flags = flagback;
                                PyErr_SetString(PyExc_ValueError, 
                                                "cannot set UPDATEIFCOPY " \
                                                "flag to True");
                                return -1;
                        }
                }
                new = PyDict_GetItemString(obj, "WRITEABLE");
                if (new) {
			if (PyObject_IsTrue(new)) {
				if (_IsWriteable(self)) {
					self->flags |= WRITEABLE;
				}
				else {
					self->flags = flagback;
					PyErr_SetString(PyExc_ValueError,
							"cannot set "	\
							"WRITEABLE "	\
							"flag to True of "\
							"this array ");
					return -1;
				}
			}
                        else
                                self->flags &= ~WRITEABLE;
                }
                new = PyDict_GetItemString(obj, "NOTSWAPPED");
                if (new) {
                        if (PyObject_IsTrue(new))
                                self->flags |= NOTSWAPPED;
                        else {
                                self->flags &= ~NOTSWAPPED;
			}
		}
                return 0;
        }
        PyErr_SetString(PyExc_ValueError, 
                        "object must be a dictionary");
        return -1;
}
*/


static PyObject *
array_shape_get(PyArrayObject *self)
{
	return PyArray_IntTupleFromIntp(self->nd, self->dimensions);
}


static int
array_shape_set(PyArrayObject *self, PyObject *val)
{
 	int nd;
	PyObject *ret;

	if (!PyTuple_Check(val)) {
		PyErr_SetString(PyExc_TypeError, "shape must be a tuple");
		return -1;
	}
	ret = PyArray_Reshape(self, val);
	if (ret == NULL) return -1;
	
	if (self->nd > 0) { /* Free old dimensions and strides */
		PyDimMem_FREE(self->dimensions);
	}
	nd = PyArray_NDIM(ret);
	self->nd = nd;
	if (nd > 0) {  /* create new dimensions and strides */
		self->dimensions = PyDimMem_NEW(2*nd);
		if (self->dimensions == NULL) {
			Py_DECREF(ret);
			PyErr_SetString(PyExc_MemoryError,"");
			return -1;
		}
		self->strides = self->dimensions + nd;
		memcpy(self->dimensions, PyArray_DIMS(ret), 
		       nd*sizeof(intp));
		memcpy(self->strides, PyArray_STRIDES(ret), 
		       nd*sizeof(intp));
	}
	else self->dimensions=NULL;
	Py_DECREF(ret);
	PyArray_UpdateFlags(self, CONTIGUOUS | FORTRAN);
	return 0;
}


static PyObject *
array_strides_get(PyArrayObject *self)
{
	return PyArray_IntTupleFromIntp(self->nd, self->strides);
}

static int
array_strides_set(PyArrayObject *self, PyObject *obj)
{
	PyArray_Dims newstrides = {NULL, 0};
	PyArrayObject *new;
	intp numbytes;

	if (!PyArray_IntpConverter(obj, &newstrides) || \
	    newstrides.ptr == NULL) {
		PyErr_SetString(PyExc_TypeError, "invalid strides");
		return -1;
	}
	if (newstrides.len != self->nd) {
		PyErr_Format(PyExc_ValueError, "strides must be "	\
			     " same length as shape (%d)", self->nd);
		goto fail;
	}
	new = self;
	while(new->base != NULL) {
		if (PyArray_Check(new->base)) 
			new = (PyArrayObject *)new->base;
	}
	numbytes = PyArray_MultiplyList(new->dimensions, 
					new->nd)*new->itemsize;
	
	if (!PyArray_CheckStrides(self->itemsize, self->nd, numbytes, 
				  self->dimensions, newstrides.ptr)) {
		PyErr_SetString(PyExc_ValueError, "strides is not "\
				"compatible with available memory");
		goto fail;
	}
	memcpy(self->strides, newstrides.ptr, sizeof(intp)*newstrides.len);
	PyArray_UpdateFlags(self, CONTIGUOUS | FORTRAN);
	PyDimMem_FREE(newstrides.ptr);
	return 0;

 fail:
	PyDimMem_FREE(newstrides.ptr);
	return -1;
}


static PyObject *
array_protocol_strides_get(PyArrayObject *self)
{
	if PyArray_ISCONTIGUOUS(self) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	return PyArray_IntTupleFromIntp(self->nd, self->strides);
}

static PyObject *
array_priority_get(PyArrayObject *self)
{
	if (PyArray_CheckExact(self)) 
		return PyFloat_FromDouble(PyArray_PRIORITY);
	else if (PyBigArray_CheckExact(self)) 
		return PyFloat_FromDouble(PyArray_BIG_PRIORITY);
	else
		return PyFloat_FromDouble(PyArray_SUBTYPE_PRIORITY);
}


static PyObject *
array_dataptr_get(PyArrayObject *self)
{
	return Py_BuildValue("NO",
			     PyString_FromFormat("%p", self->data),
			     (self->flags & WRITEABLE ? Py_False :
			      Py_True));
}

static PyObject *
array_data_get(PyArrayObject *self)
{
	intp nbytes;
	if (!(PyArray_ISONESEGMENT(self))) {
		PyErr_SetString(PyExc_AttributeError, "cannot get single-"\
				"segment buffer for discontiguous array");
		return NULL;
	}		
	nbytes = PyArray_NBYTES(self);
	if PyArray_ISWRITEABLE(self) 
		return PyBuffer_FromReadWriteObject((PyObject *)self, 0, 
						    (int) nbytes);
	else
		return PyBuffer_FromObject((PyObject *)self, 0, (int) nbytes);
}

static int
array_data_set(PyArrayObject *self, PyObject *op)
{
	void *buf;
	int buf_len;
	int writeable=1;

	if (PyObject_AsWriteBuffer(op, &buf, &buf_len) < 0) {
		writeable = 0;
		if (PyObject_AsReadBuffer(op, (const void **)&buf, 
					  &buf_len) < 0) {
			PyErr_SetString(PyExc_AttributeError, 
					"object does not have single-segment " \
					"buffer interface");
			return -1;
		}
	}
	if (!PyArray_ISONESEGMENT(self)) {
		PyErr_SetString(PyExc_AttributeError, "cannot set single-" \
				"segment buffer for discontiguous array");
		return -1;
	}
	if (PyArray_NBYTES(self) > buf_len) {
		PyErr_SetString(PyExc_AttributeError, 
				"not enough data for array");
		return -1;
	}
	if (self->flags & OWN_DATA) {
		PyArray_XDECREF(self);
		PyDataMem_FREE(self->data);
	}
	if (self->base) {
		if (self->flags & UPDATEIFCOPY) {
			((PyArrayObject *)self->base)->flags |= WRITEABLE;
			self->flags &= ~UPDATEIFCOPY;
		}
		Py_DECREF(self->base);
	}
	Py_INCREF(op);
	self->base = op;
	self->data = buf;
	self->flags = CARRAY_FLAGS;
	if (!writeable)
		self->flags &= ~WRITEABLE;
	return 0;
}


static PyObject *
array_itemsize_get(PyArrayObject *self)
{
	return PyInt_FromLong((long) self->itemsize);
}

static PyObject *
array_size_get(PyArrayObject *self)
{
	intp size=PyArray_SIZE(self);
#if SIZEOF_INTP == SIZEOF_LONG
        return PyInt_FromLong((long) size);
#else
	if (size > MAX_LONG || size < MIN_LONG)
		return PyLong_FromLongLong(size);
	else 
		return PyInt_FromLong((long) size);
#endif
}

static PyObject *
array_nbytes_get(PyArrayObject *self)
{
        intp nbytes = PyArray_NBYTES(self);
#if SIZEOF_INTP == SIZEOF_LONG
        return PyInt_FromLong((long) nbytes);
#else
	if (nbytes > MAX_LONG || nbytes < MIN_LONG)
		return PyLong_FromLongLong(nbytes);
	else 
		return PyInt_FromLong((long) nbytes);
#endif
}


static PyObject *
array_typechar_get(PyArrayObject *self)
{
	if PyArray_ISFLEXIBLE(self) 
		return PyString_FromFormat("%c%d", (self->descr->type),
					   self->itemsize);
	else 
		return PyString_FromStringAndSize(&(self->descr->type), 1);
}

static PyObject *
array_typestr_get(PyArrayObject *self)
{
	static char endians[] = "<>";
	char endian;
	int which;
	unsigned long val = 1;
	char *s;
	char basic_=self->descr->kind;

	s = (char *)&val; /* s[0] == 0 implies big-endian */
	which = (PyArray_ISNOTSWAPPED(self) ? 0 : 1);
	if (s[0] == 0) which = 1 - which;
	endian = endians[which];       
	
	if ((basic_==PyArray_VOIDLTR) || (basic_==PyArray_STRINGLTR) || \
	    (basic_==PyArray_OBJECTLTR) || (self->itemsize == 1))
		return PyString_FromFormat("|%c%d", basic_, self->itemsize);
	else
		return PyString_FromFormat("%c%c%d", endian, basic_,
					   self->itemsize);
}

static PyObject *
array_descr_get(PyArrayObject *self)
{
	PyObject *res;
	PyObject *dobj;

	/* hand this off to the typeobject */
	/* or give default */
	if (PyArray_ISUSERDEF(self)) {
		res = PyObject_GetAttrString((PyObject *)self->descr->typeobj, 
					     "__array_descr__");
		if (res) return res;
		PyErr_Clear();
	}
	/* get default */
	dobj = PyTuple_New(2);
	if (dobj == NULL) return NULL;
	PyTuple_SET_ITEM(dobj, 0, PyString_FromString(""));
	PyTuple_SET_ITEM(dobj, 1, array_typestr_get(self));
	res = PyList_New(1);
	if (res == NULL) {Py_DECREF(dobj); return NULL;}
	PyList_SET_ITEM(res, 0, dobj);
	return res;
}

static PyObject *
array_struct_get(PyArrayObject *self)
{
        PyArrayInterface *inter;
        
        inter = (PyArrayInterface *)malloc(sizeof(PyArrayInterface));
        inter->version = 2;
        inter->nd = self->nd;
        inter->typekind = self->descr->kind;
        inter->itemsize = self->itemsize;
        inter->flags = self->flags;
	inter->flags &= ~UPDATEIFCOPY;   /* reset this flag */
        inter->strides = self->strides;
        inter->shape = self->dimensions;
        inter->data = self->data;
	Py_INCREF(self);
        return PyCObject_FromVoidPtrAndDesc(inter, self, gentype_struct_free);
}

static PyObject *
array_type_get(PyArrayObject *self)
{
        Py_INCREF(self->descr->typeobj);
        return (PyObject *)self->descr->typeobj;
}

/* If the type is changed.  
    Also needing change: strides, itemsize

    Either itemsize is exactly the same
    or the array is single-segment (contiguous or fortran) with
    compatibile dimensions

*/

static int
array_type_set(PyArrayObject *self, PyObject *arg)
{
        PyArray_Typecode newtype = {PyArray_NOTYPE, 0, 0};
        intp newdim;
        int index;
        char *msg = "new type not compatible with array.";

        if ((PyArray_TypecodeConverter(arg, &newtype) < 0) ||
            newtype.type_num == PyArray_NOTYPE) {
                PyErr_SetString(PyExc_TypeError, "invalid type for array");
                return -1;
        }
        if (!(PyArray_ISONESEGMENT(self) ||		\
	      (newtype.itemsize != self->itemsize))) {
                PyErr_SetString(PyExc_ValueError, msg);
                return -1;                 
        }

	if (PyArray_ISCONTIGUOUS(self)) index = self->nd - 1;
	else index = 0;

        if (newtype.itemsize < self->itemsize) {
                /* if it is compatible increase the size of the dimension
                   at end (or at the front for FORTRAN)
                */
                if (self->itemsize % newtype.itemsize != 0) {
                        PyErr_SetString(PyExc_ValueError, msg);
                        return -1;
                }
                newdim = self->itemsize / newtype.itemsize;
		self->dimensions[index] *= newdim;
                self->strides[index] = newtype.itemsize;
	}
        
        else if (newtype.itemsize > self->itemsize) {
        
                /* Determine if last (or first if FORTRAN) dimension
                   is compatible */

		newdim = self->dimensions[index] * self->itemsize;
                if ((newdim % newtype.itemsize) != 0) {
			PyErr_SetString(PyExc_ValueError, msg);
                        return -1;
                }
		
                self->dimensions[index] = newdim / newtype.itemsize;
                self->strides[index] = newtype.itemsize;
		
        }
	
        
        /* fall through -- adjust type*/

        self->descr = PyArray_DescrFromType(newtype.type_num);
        self->itemsize = newtype.itemsize;
        PyArray_UpdateFlags(self, ALIGNED);
        return 0;

}



static PyObject *
array_base_get(PyArrayObject *self)
{
	if (self->base == NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	else {
		Py_INCREF(self->base);
		return self->base;
	}
}


static PyObject *
array_real_get(PyArrayObject *self)
{
	PyArrayObject *ret;

	if (PyArray_ISCOMPLEX(self)) {
		ret = (PyArrayObject *)PyArray_New(self->ob_type,
						   self->nd,
						   self->dimensions,
						   self->descr->type_num - \
						   PyArray_NUM_FLOATTYPE,
						   self->strides,
						   self->data,
						   0,
						   self->flags, (PyObject *)self);
		if (ret == NULL) return NULL;
		ret->flags &= ~CONTIGUOUS;
		ret->flags &= ~FORTRAN;
		Py_INCREF(self);
		ret->base = (PyObject *)self;
		return (PyObject *)ret;
	}
	else {
		Py_INCREF(self);
		return (PyObject *)self;
	}
}


static int
array_real_set(PyArrayObject *self, PyObject *val)
{
	PyArrayObject *ret;
	PyArrayObject *new;
	int rint;

	new = (PyArrayObject *)PyArray_FromAny(val, NULL, 0, 0, 0);
	if (new == NULL) return -1;
	
	if (PyArray_ISCOMPLEX(self)) {
		ret = (PyArrayObject *)PyArray_New(self->ob_type,
						   self->nd,
						   self->dimensions,
						   self->descr->type_num - \
						   PyArray_NUM_FLOATTYPE,
						   self->strides,
						   self->data,
						   0,
						   self->flags, (PyObject *)self);
		if (ret == NULL) return -1;
		ret->flags &= ~CONTIGUOUS;
		ret->flags &= ~FORTRAN;
		Py_INCREF(self);
		ret->base = (PyObject *)self;
	}
	else {
		Py_INCREF(self);
		ret = self;
	}	
	rint = PyArray_CopyInto(ret, new);
	Py_DECREF(ret);
	return rint;
}

static PyObject *
array_imag_get(PyArrayObject *self)
{	
	PyArrayObject *ret;
	int itemsize;
	int typenum;
        PyArray_Typecode type;
	
	type.type_num = self->descr->type_num;
	type.itemsize = self->itemsize;
	type.fortran = PyArray_ISFORTRAN(self);
	
	if (PyArray_ISCOMPLEX(self)) {
		itemsize = self->itemsize >> 1;
		typenum = self->descr->type_num - PyArray_NUM_FLOATTYPE;
		ret = (PyArrayObject *)PyArray_New(self->ob_type,
						   self->nd,
						   self->dimensions,
						   typenum,
						   self->strides,
						   self->data + itemsize,
						   0,
						   self->flags, (PyObject *)self);
		if (ret == NULL) return NULL;
		ret->flags &= ~CONTIGUOUS;
		ret->flags &= ~FORTRAN;
		Py_INCREF(self);
		ret->base = (PyObject *)self;
		return (PyObject *) ret;
	}
	else {
		ret = (PyArrayObject *)PyArray_Zeros(self->nd, 
						     self->dimensions, &type);
		ret->flags &= ~WRITEABLE;
		return (PyObject *)ret;
	}
}

static int
array_imag_set(PyArrayObject *self, PyObject *val)
{	
	if (PyArray_ISCOMPLEX(self)) {
		PyArrayObject *ret;
		PyArrayObject *new;
		int rint;

		new = (PyArrayObject *)PyArray_FromAny(val, NULL, 0, 0, 0); 
		if (new == NULL) return -1;
		ret = (PyArrayObject *)PyArray_New(self->ob_type,
						   self->nd,
						   self->dimensions,
						   self->descr->type_num - \
						   PyArray_NUM_FLOATTYPE,
						   self->strides+ \
						   (self->itemsize >> 1) ,
 						   self->data,
						   0,
						   self->flags, (PyObject *)self);
		if (ret == NULL) {
			Py_DECREF(new); 
			return -1;
		}
		ret->flags &= ~CONTIGUOUS;
		ret->flags &= ~FORTRAN;
		Py_INCREF(self);
		ret->base = (PyObject *)self;
		rint = PyArray_CopyInto(ret, new);
		Py_DECREF(ret);		
		Py_DECREF(new);
		return rint;
	}
	else {
		PyErr_SetString(PyExc_TypeError, "does not have imaginary " \
				"part to set");
		return -1;
	}
}

static PyObject *
array_flat_get(PyArrayObject *self)
{
        return PyArray_IterNew((PyObject *)self);
}

static int 
array_flat_set(PyArrayObject *self, PyObject *val)
{
	PyObject *arr=NULL;
	int retval = -1;
	PyArrayIterObject *selfit=NULL, *arrit=NULL;
	PyArray_Typecode typecode;
        int swap;
        PyArray_CopySwapFunc *copyswap;

	typecode.type_num = self->descr->type_num;
	typecode.itemsize = self->itemsize;
	typecode.fortran = PyArray_ISFORTRAN(self);
	
	arr = PyArray_FromAny(val, &typecode, 
			      0, 0, FORCECAST);
	if (arr == NULL) return -1;
	arrit = (PyArrayIterObject *)PyArray_IterNew(arr);
	if (arrit == NULL) goto exit;
	selfit = (PyArrayIterObject *)PyArray_IterNew((PyObject *)self);
	if (selfit == NULL) goto exit;

        swap = PyArray_ISNOTSWAPPED(self) != PyArray_ISNOTSWAPPED(arr);
        copyswap = self->descr->copyswap;
        if (PyArray_ISOBJECT(self)) {
                while(selfit->index < selfit->size) {
                        Py_XDECREF(*((PyObject **)selfit->dataptr));
                        Py_INCREF(*((PyObject **)arrit->dataptr)); 
                        memmove(selfit->dataptr, arrit->dataptr, 
                                sizeof(PyObject *));
                        copyswap(selfit->dataptr, NULL, swap, 
                                  sizeof(PyObject *));
                        PyArray_ITER_NEXT(selfit);
                        PyArray_ITER_NEXT(arrit);
                        if (arrit->index == arrit->size) 
                                PyArray_ITER_RESET(arrit);
                }
                retval = 0; 
                goto exit;
        }

	while(selfit->index < selfit->size) {
		memmove(selfit->dataptr, arrit->dataptr, self->itemsize);
                copyswap(selfit->dataptr, NULL, swap, self->itemsize);
		PyArray_ITER_NEXT(selfit);
		PyArray_ITER_NEXT(arrit);
		if (arrit->index == arrit->size) 
			PyArray_ITER_RESET(arrit);
	}
	retval = 0;
 exit:
	Py_XDECREF(selfit);
	Py_XDECREF(arrit);
	Py_XDECREF(arr);
	return retval;
}

static PyGetSetDef array_getsetlist[] = {
        {"ndim", 
	 (getter)array_ndim_get, 
	 NULL, 
	 "number of array dimensions"},
        {"flags", 
	 (getter)array_flags_get, 
         NULL,
	 "special dictionary of flags"},
        {"shape", 
	 (getter)array_shape_get, 
	 (setter)array_shape_set, 
	 "tuple of array dimensions"},
        {"strides", 
	 (getter)array_strides_get, 
	 (setter)array_strides_set,
	 "tuple of bytes steps in each dimension"},
        {"data", 
	 (getter)array_data_get, 
	 (setter)array_data_set, 
	 "pointer to start of data"},
        {"itemsize", 
	 (getter)array_itemsize_get, 
	 NULL,
	 "length of one element in bytes"},
        {"size",
         (getter)array_size_get,
	 NULL,
         "number of elements in the array"},
        {"nbytes",
         (getter)array_nbytes_get,
         NULL,
         "number of bytes in the array"},
	{"base",
	 (getter)array_base_get,
	 NULL,
	 "base object"},
        {"dtype", 
	 (getter)array_type_get, 
	 (setter)array_type_set,
	 "get array type class"},
	{"dtypechar",
	 (getter)array_typechar_get,
	 NULL,
	 "get array type character code"},
	{"dtypestr",
	 (getter)array_typestr_get,
	 NULL,
	 "get array type string"},
        {"real", 
	 (getter)array_real_get, 
	 (setter)array_real_set, 
	 "real part of array"},
        {"imag", 
	 (getter)array_imag_get, 
	 (setter)array_imag_set, 
	 "imaginary part of array"},
	{"flat", 
	 (getter)array_flat_get, 
	 (setter)array_flat_set, 
	 "a 1-d view of a contiguous array"}, 
	{"__array_data__", 
	 (getter)array_dataptr_get,
	 NULL,
	 "Array protocol: data"},
	{"__array_typestr__",
	 (getter)array_typestr_get,
	 NULL,
	 "Array protocol: typestr"},
	{"__array_descr__",
	 (getter)array_descr_get,
	 NULL,
	 "Array protocol: descr"},
	{"__array_shape__", 
	 (getter)array_shape_get,
	 NULL,
	 "Array protocol: shape"},
	{"__array_strides__",
	 (getter)array_protocol_strides_get,
	 NULL,
	 "Array protocol: strides"},
        {"__array_struct__",
         (getter)array_struct_get,
         NULL,
         "Array protocol: struct"},
	{"__array_priority__",
	 (getter)array_priority_get,
	 NULL,
	 "Array priority"},
       	{NULL, NULL, NULL, NULL},  /* Sentinel */
};

/****************** end of attribute get and set routines *******************/



static char Arraytype__doc__[] = 
        "A array object represents a multidimensional, homogeneous array\n"
	"  of basic values.  Arrays are sequence, mapping and numeric\n"
	"  objects.  More information is available in the scipy module and\n"
	"  by looking at the methods and attributes of an array.";

static PyTypeObject PyBigArray_Type = { 
        PyObject_HEAD_INIT(NULL)
        0,					  /*ob_size*/
        "scipy.bigndarray",		  /*tp_name*/
        sizeof(PyArrayObject),		          /*tp_basicsize*/
        0,					  /*tp_itemsize*/
        /* methods */
        (destructor)array_dealloc,		  /*tp_dealloc  */
        (printfunc)NULL,			  /*tp_print*/
        0,					  /*tp_getattr*/
        0,					  /*tp_setattr*/
        (cmpfunc)0,     		          /*tp_compare*/
        (reprfunc)array_repr,		          /*tp_repr*/
        &array_as_number,			  /*tp_as_number*/
        NULL, 			                  /*tp_as_sequence*/
        &array_as_mapping,			  /*tp_as_mapping*/
        (hashfunc)0,			          /*tp_hash*/
        (ternaryfunc)0,			          /*tp_call*/
        (reprfunc)array_str,     	          /*tp_str*/
		
        (getattrofunc)0,			  /*tp_getattro*/
        (setattrofunc)0,			  /*tp_setattro*/
        NULL,                            	  /*tp_as_buffer*/
        (Py_TPFLAGS_DEFAULT 
         | Py_TPFLAGS_BASETYPE
         | Py_TPFLAGS_CHECKTYPES),           /*tp_flags*/
        /*Documentation string */
        Arraytype__doc__,			  /*tp_doc*/

        (traverseproc)0,			  /*tp_traverse */
        (inquiry)0,			          /*tp_clear */
        (richcmpfunc)array_richcompare,	          
        offsetof(PyArrayObject, weakreflist),     /*tp_weaklistoffset */

        /* Iterator support (use standard) */

        (getiterfunc)array_iter,   	          /* tp_iter */
        (iternextfunc)0,			  /* tp_iternext */

        /* Sub-classing (new-style object) support */

        array_methods,			          /* tp_methods */
        0,					  /* tp_members */
        array_getsetlist,		          /* tp_getset */
        0,					  /* tp_base */
        0,					  /* tp_dict */
        0,					  /* tp_descr_get */
        0,					  /* tp_descr_set */
        0,					  /* tp_dictoffset */
        (initproc)0,	  	                  /* tp_init */
        0,	                                  /* tp_alloc */ 
        (newfunc)array_new,		          /* tp_new */
        0,	               	                  /* tp_free */
        0,					  /* tp_is_gc */
        0,					  /* tp_bases */
        0,					  /* tp_mro */
        0,					  /* tp_cache */
        0,					  /* tp_subclasses */
        0					  /* tp_weaklist */
};

/* A standard array will subclass from the Big Array and 
   add the array_as_sequence table
   and the array_as_buffer table
 */

static PyTypeObject PyArray_Type = { 
        PyObject_HEAD_INIT(NULL)
        0,					  /*ob_size*/
        "scipy.ndarray",			  /*tp_name*/
        sizeof(PyArrayObject),		          /*tp_basicsize*/
        0,					  /*tp_itemsize*/
};


/* The rest of this code is to build the right kind of array from a python */
/* object. */

static int 
discover_depth(PyObject *s, int max, int stop_at_string) 
{
        int d=0;
        PyObject *e;
	
        if(max < 1) return -1;

        if(! PySequence_Check(s) || PyInstance_Check(s) || \
	   PySequence_Length(s) < 0) {
                PyErr_Clear(); return 0;
        }
        if (PyArray_Check(s))
		return PyArray_NDIM(s);
        if(PyString_Check(s) || PyBuffer_Check(s) || PyUnicode_Check(s))
		return stop_at_string ? 0:1;
        if (PySequence_Length(s) == 0) 
		return 1;
	
        if ((e=PySequence_GetItem(s,0)) == NULL) return -1;
        if(e!=s) {
		d=discover_depth(e,max-1, stop_at_string);
		if(d >= 0) d++;
	}
        Py_DECREF(e);
        return d;
}

static int
discover_itemsize(PyObject *s, int nd, int *itemsize) 
{
	int n, r, i;
	PyObject *e;
	
	n = PyObject_Length(s);

	if ((nd == 0) || PyString_Check(s) ||		\
	    PyUnicode_Check(s) || PyBuffer_Check(s)) {
		if PyUnicode_Check(s) 
			*itemsize = MAX(*itemsize, sizeof(Py_UNICODE)*n);
		else
			*itemsize = MAX(*itemsize, n);
		return 0;
	}
	for (i=0; i<n; i++) {
		if ((e=PySequence_GetItem(s,i))==NULL) return -1;
                r=discover_itemsize(e,nd-1,itemsize);
                Py_DECREF(e);
                if (r == -1) return -1; 
        }
        return 0;
}

/* Take an arbitrary object known to represent
   an array of ndim nd, and determine the size in each dimension
*/

static int 
discover_dimensions(PyObject *s, int nd, intp *d, int check_it) 
{
        PyObject *e;
        int r, n, i, n_lower;
	
        n=PyObject_Length(s);
        *d = n;
        if(*d < 0) return -1;
        if(nd <= 1) return 0;
        n_lower = 0;
        for(i=0; i<n; i++) {
                if ((e=PySequence_GetItem(s,i)) == NULL) return -1;
                r=discover_dimensions(e,nd-1,d+1,check_it);
                Py_DECREF(e);
		
                if (r == -1) return -1; 
		if (check_it && n_lower != 0 && n_lower != d[1]) {
			PyErr_SetString(PyExc_ValueError,
					"inconsistent shape in sequence");
			return -1;
		}		
                if (d[1] > n_lower) n_lower = d[1];
        }
        d[1] = n_lower;
	
        return 0;
}

static void
_array_small_type(int chktype, int mintype, int chksize, int minsize, 
		  PyArray_Typecode *outtype)
{
	outtype->type_num = MAX(chktype, mintype);
	if (PyTypeNum_ISFLEXIBLE(outtype->type_num) &&          \
	    (PyTypeNum_ISFLEXIBLE(mintype) || mintype==0)) {
		/* Handle string->unicode case separately 
		   because string itemsize is twice as large */
		if (outtype->type_num == PyArray_UNICODE && 
		    mintype == PyArray_STRING) {
			outtype->itemsize = MAX(chksize, 2*minsize);
		}
		else {
			outtype->itemsize = MAX(chksize, minsize);
		}
	}
	else {
		outtype->itemsize = chksize;
	}
	return;	
}

static void
_array_find_type(PyObject *op, PyArray_Typecode *minitype, 
		 PyArray_Typecode *outtype, int max)
{
        int l;
        PyObject *ip;
	int chktype=0;
	int chksize=0;
	int mintype, minsize;

	if (minitype == NULL) {
		mintype = PyArray_BOOL;
		minsize = sizeof(Bool);
	}
	else {
		mintype = minitype->type_num;
		minsize = minitype->itemsize;
	}

    
        if (max < 0 || mintype == -1) goto deflt;
	
        if (PyArray_Check(op)) {
		chktype = PyArray_TYPE(op);
		chksize = PyArray_ITEMSIZE(op);
		goto finish;
	}
	
	if (PyArray_IsScalar(op, Generic)) {
		PyArray_TypecodeFromScalar(op, outtype);
		chktype = outtype->type_num;
		chksize = outtype->itemsize;
		goto finish;
	}

	if ((ip=PyObject_GetAttrString(op, "__array_typestr__"))!=NULL) {
		int swap=0, res=-1;
		if (PyString_Check(ip)) {
			res = _array_typecode_fromstr(PyString_AS_STRING(ip), 
						      &swap, outtype);   
			if (res >= 0) {
				chktype = outtype->type_num;
				chksize = outtype->itemsize;
			}
		}
		Py_DECREF(ip);
                if (res >= 0) goto finish;
	}
        
        if ((ip=PyObject_GetAttrString(op, "__array_struct__")) != NULL) {
                PyArrayInterface *inter;
                char buf[40];
                int swap=0, res=-1;
                if (PyCObject_Check(ip)) {
                        inter=(PyArrayInterface *)PyCObject_AsVoidPtr(ip);
                        if (inter->version == 2) {
                                snprintf(buf, 40, "|%c%d", inter->typekind, inter->itemsize);
                                res = _array_typecode_fromstr(buf, &swap, outtype);
                                if (res >= 0) {
                                        chktype = outtype->type_num;
                                        chktype = outtype->itemsize;
                                }
                        }
                }
                Py_DECREF(ip);
                if (res >= 0) goto finish;
        }
        	
        if (PyString_Check(op)) {
		chktype = PyArray_STRING;
		chksize = PyString_GET_SIZE(op);
		goto finish;
        }

	if (PyUnicode_Check(op)) {
		chktype = PyArray_UNICODE;
		chksize = PyUnicode_GET_DATA_SIZE(op);
		goto finish;
	}

	if (PyBuffer_Check(op)) {
		chktype = PyArray_VOID;
		chksize = op->ob_type->tp_as_sequence->sq_length(op);
                PyErr_Clear();
		goto finish;
	}


        if (PyObject_HasAttrString(op, "__array__")) {
                ip = PyObject_CallMethod(op, "__array__", NULL);
                if(ip && PyArray_Check(ip)) {
			chktype = PyArray_TYPE(ip);
			chksize = PyArray_ITEMSIZE(ip);
			goto finish;
		}
        } 

	if (PyInstance_Check(op)) goto deflt;
	
        if (PySequence_Check(op)) {
		PyArray_Typecode newtype;
		newtype.type_num = mintype;
		newtype.itemsize = minsize;
		newtype.fortran = 0;
                l = PyObject_Length(op);
                if (l < 0 && PyErr_Occurred()) { 
			PyErr_Clear(); 
			goto deflt;
		}
                if (l == 0 && mintype == 0) {
			newtype.type_num = PyArray_INTP;
			newtype.itemsize = sizeof(intp);
		}
                while (--l >= 0) {
                        ip = PySequence_GetItem(op, l);
                        if (ip==NULL) {
				PyErr_Clear(); 
				goto deflt;
			}
			_array_find_type(ip, &newtype, outtype, max-1);
			_array_small_type(outtype->type_num,
					  newtype.type_num, 
					  outtype->itemsize,
					  newtype.itemsize,
					  &newtype);
                        Py_DECREF(ip);
                }
		chktype = newtype.type_num;
		chksize = newtype.itemsize;
		goto finish;
        }
	
	if (PyBool_Check(op)) {
		chktype = PyArray_BOOL;
		chksize = sizeof(Bool);
		goto finish;		
	}
        else if (PyInt_Check(op)) {
		chktype = PyArray_LONG;
		chksize = sizeof(long);
		goto finish;
        } else if (PyFloat_Check(op)) {
		chktype = PyArray_DOUBLE;
		chksize = sizeof(double);
		goto finish;
	} else if (PyComplex_Check(op)) {
		chktype = PyArray_CDOUBLE;
		chksize = sizeof(cdouble);
		goto finish;
	}

 deflt:
	chktype = PyArray_OBJECT;
	chksize = sizeof(void *);

 finish:
	_array_small_type(chktype, mintype, chksize, minsize, 
			  outtype);
	return;
}

static int 
Assign_Array(PyArrayObject *self, PyObject *v) 
{
        PyObject *e;
        int l, r;
	
        if (!PySequence_Check(v)) {
                PyErr_SetString(PyExc_ValueError,
				"assignment from non-sequence");
                return -1;
        }
	
        l=PyObject_Length(v);
        if(l < 0) return -1; 
	
        while(--l >= 0)
                {
                        e=PySequence_GetItem(v,l);
                        if (e == NULL) return -1; 
			r = PySequence_SetItem((PyObject*)self,l,e);
                        Py_DECREF(e);
                        if(r == -1) return -1;
                }
        return 0;
}

/* "Array Scalars don't call this code" */ 
static PyObject *
Array_FromScalar(PyObject *op, PyArray_Typecode *typecode) 
{
        PyArrayObject *ret;
	int itemsize; 
	int type;
	
	itemsize = typecode->itemsize;
	type = typecode->type_num;

	if (itemsize == 0 && PyTypeNum_ISFLEXIBLE(type)) {
		itemsize = PyObject_Length(op);
	}

	ret = (PyArrayObject *)PyArray_New(&PyArray_Type, 0, NULL, type,
					   NULL, NULL, itemsize, 0, NULL);

	if (ret == NULL) return NULL;

        ret->descr->setitem(op, ret->data, ret);
	
        if (PyErr_Occurred()) {
                array_dealloc(ret);
                return NULL;
        } else {
                return (PyObject *)ret;
        }
}


static PyObject *
Array_FromSequence(PyObject *s, PyArray_Typecode *typecode, int min_depth, 
		   int max_depth)
{
        PyArrayObject *r;
        int nd;
	intp *d;
	int stop_at_string;
	int type = typecode->type_num;
	int itemsize = typecode->itemsize;
	
	
	stop_at_string = ((type == PyArray_OBJECT) ||	\
			  (type == PyArray_STRING) ||	\
			  (type == PyArray_UNICODE) ||  \
			  (type == PyArray_VOID));

        if (!((nd=discover_depth(s, MAX_DIMS+1, stop_at_string)) > 0)) {
		if (nd==0)
			return Array_FromScalar(s, typecode);
                PyErr_SetString(PyExc_ValueError, 
				"invalid input sequence");
                return NULL;
        }
	
        if ((max_depth && nd > max_depth) ||	\
	    (min_depth && nd < min_depth)) {
                PyErr_SetString(PyExc_ValueError, 
				"invalid number of dimensions");
                return NULL;
        }
	
	if ((d=PyDimMem_NEW(nd)) == NULL) {
		return PyErr_NoMemory();
        }
	if(discover_dimensions(s,nd,d, !stop_at_string) == -1) {
		PyDimMem_FREE(d);
		return NULL;
	}
	if (itemsize == 0 && PyTypeNum_ISFLEXIBLE(type)) {
		if (discover_itemsize(s, nd, &itemsize) == -1) {
			PyDimMem_FREE(d);
			return NULL;
		}
	}
	
        r=(PyArrayObject*)PyArray_New(&PyArray_Type, nd, d, 
				      type, NULL, NULL,
				      itemsize, 
				      typecode->fortran, NULL);

        PyDimMem_FREE(d);
        if(!r) return NULL;
        if(Assign_Array(r,s) == -1) {
		Py_DECREF(r);
		return NULL;
	}
        return (PyObject*)r;
}


static int 
PyArray_ValidType(int type) 
{
	PyArray_Descr *descr;
	
	descr = PyArray_DescrFromType(type);
	if (descr==NULL) return FALSE;
	return TRUE;
}


/* If the output is not a CARRAY, then it is buffered also */

static int
_bufferedcast(PyArrayObject *out, PyArrayObject *in)
{
	char *inbuffer, *bptr, *optr;
	char *outbuffer=NULL;
	PyArrayIterObject *it_in=NULL, *it_out=NULL;
	register intp i, index;
	intp ncopies = PyArray_SIZE(out) / PyArray_SIZE(in);
	int elsize=in->itemsize;
	int nels = PyArray_BUFSIZE;
	int el;
	int inswap, outswap=0;
	int obuf=!PyArray_ISCARRAY(out);
	int oelsize = out->itemsize;
	PyArray_VectorUnaryFunc *castfunc;
        PyArray_CopySwapFunc *in_csn;
        PyArray_CopySwapFunc *out_csn;
	int retval = -1;

	castfunc = in->descr->cast[out->descr->type_num];
        in_csn = in->descr->copyswap;
        out_csn = out->descr->copyswap;

	/* If the input or output is STRING, UNICODE, or VOID */
	/*  then getitem and setitem are used for the cast */
	/*  and byteswapping is handled by those methods */

	inswap = !(PyArray_ISFLEXIBLE(in) || PyArray_ISNOTSWAPPED(in));
	
	inbuffer = PyDataMem_NEW(PyArray_BUFSIZE*elsize);
	if (inbuffer == NULL) return -1;
	it_in = (PyArrayIterObject *)PyArray_IterNew((PyObject *)in);
	if (it_in == NULL) goto exit;

	if (obuf) {
		outswap = !(PyArray_ISFLEXIBLE(out) || \
			    PyArray_ISNOTSWAPPED(out));
		outbuffer = PyDataMem_NEW(PyArray_BUFSIZE*oelsize);
		if (outbuffer == NULL) goto exit;

		it_out = (PyArrayIterObject *)PyArray_IterNew((PyObject *)out);
		if (it_out == NULL) goto exit;

		nels = MIN(nels, PyArray_BUFSIZE);
	}
	
	optr = (obuf) ? outbuffer: out->data;
	bptr = inbuffer;
	el = 0;	
	while(ncopies--) {
		index = it_in->size;
		PyArray_ITER_RESET(it_in);
		while(index--) {
                        in_csn(bptr, it_in->dataptr, inswap, elsize);
			bptr += elsize;
			PyArray_ITER_NEXT(it_in);
			el += 1;
			if ((el == nels) || (index == 0)) {
				/* buffer filled, do cast */
				
				castfunc(inbuffer, optr, el, in, out);
				
				if (obuf) {
					/* Copy from outbuffer to array */
					for(i=0; i<el; i++) {
                                                out_csn(it_out->dataptr,
                                                        optr, outswap,
                                                        oelsize);
						optr += oelsize;
						PyArray_ITER_NEXT(it_out);
					}
					optr = outbuffer;
				}
				else {
					optr += out->itemsize * nels;
				}
				el = 0;
				bptr = inbuffer;
			}
		}
	}
	retval = 0;
 exit:
	Py_XDECREF(it_in);
	PyDataMem_FREE(inbuffer);
	PyDataMem_FREE(outbuffer);	
	if (obuf) {
		Py_XDECREF(it_out);
	}
	return retval;

}


/* For backward compatibility */

static PyObject *
PyArray_Cast(PyArrayObject *mp, int type_num) 
{
	PyArray_Typecode type;
	PyArray_Descr *descr;

	descr = PyArray_DescrFromType(type_num);
	type.itemsize = descr->elsize;
	type.type_num = descr->type_num;
	type.fortran = 0;
	
	return PyArray_CastToType(mp, &type);
}

static PyObject * 
PyArray_CastToType(PyArrayObject *mp, PyArray_Typecode *at) 
{
	PyObject *out;
	int ret;

	if ((mp->descr->type_num == at->type_num) &&                    \
	    (at->itemsize==0 || mp->itemsize == at->itemsize) &&
	    PyArray_ISBEHAVED_RO(mp)) {
		Py_INCREF(mp);
		return (PyObject *)mp;
	}
		
	if (at->itemsize == 0) {
		if (mp->descr->type_num == PyArray_STRING &&	\
		    at->type_num == PyArray_UNICODE)
			at->itemsize = mp->itemsize*sizeof(Py_UNICODE);
		if (mp->descr->type_num == PyArray_UNICODE &&
		    at->type_num == PyArray_STRING) 
			at->itemsize = mp->itemsize/sizeof(Py_UNICODE);
		if (at->type_num == PyArray_VOID)
			at->itemsize = mp->itemsize;
	}

	out = PyArray_New(mp->ob_type, mp->nd, 
			  mp->dimensions, 
			  at->type_num,
			  NULL, NULL, at->itemsize, 
			  at->fortran, (PyObject *)mp);
	if (out == NULL) return NULL;
	ret = PyArray_CastTo((PyArrayObject *)out, mp);
	if (ret != -1) return out;

	Py_DECREF(out);
	return NULL;
	
}
	 
/* The number of elements in out must be an integer multiple
   of the number of elements in mp. 
*/

static int
PyArray_CastTo(PyArrayObject *out, PyArrayObject *mp)
{

	int simple;
	intp mpsize = PyArray_SIZE(mp);
	intp outsize = PyArray_SIZE(out);

	if (mpsize == 0) return 0;
	if (!PyArray_ISWRITEABLE(out)) {
		PyErr_SetString(PyExc_ValueError, 
				"output array is not writeable");
		return -1;
	}
	if (outsize % mpsize != 0) {
		PyErr_SetString(PyExc_ValueError, 
				"output array must have an integer-multiple"\
				" of the number of elements in the input "\
				"array");
		return -1; 
	}

	if (out->descr->type_num >= PyArray_NTYPES) {
		PyErr_SetString(PyExc_ValueError, 
				"Can only cast to builtin types.");
		return -1;
				
	}

	simple =  ((PyArray_ISCARRAY(mp) && PyArray_ISCARRAY(out)) ||   \
                   (PyArray_ISFARRAY(mp) && PyArray_ISFARRAY(out)));
	
	if (simple) {
		char *inptr;
		char *optr = out->data;
		intp obytes = out->itemsize * outsize;
		intp ncopies = outsize / mpsize;

		while(ncopies--) {
			inptr = mp->data;
			mp->descr->cast[out->descr->type_num](inptr, 
							      optr,
							      mpsize,
							      mp, out);
			optr += obytes;
		}
		return 0;
	}
	
	/* If not a well-behaved cast, then use buffers */
	if (_bufferedcast(out, mp) == -1) {
		return -1;
	}
	return 0;
}

static PyObject *
array_fromarray(PyArrayObject *arr, PyArray_Typecode *typecode, int flags) 
{
	
	PyArrayObject *ret=NULL;
	int type = typecode->type_num;
	int itemsize = typecode->itemsize;
	int copy = 0;
	int arrflags;
	PyArray_Typecode oldtype;
	char *msg = "cannot copy back to a read-only array";
        PyTypeObject *subtype;

	oldtype.type_num = PyArray_TYPE(arr);
	oldtype.itemsize = PyArray_ITEMSIZE(arr);
	oldtype.fortran = 0;

        subtype = arr->ob_type;

	if (type == PyArray_NOTYPE) type = arr->descr->type_num;
	if (itemsize == 0) itemsize = arr->itemsize;
	typecode->type_num = type;
	typecode->itemsize = itemsize;

	/* Don't copy if sizes are compatible */
	if (PyArray_EquivalentTypes(&oldtype, typecode)) {
		arrflags = arr->flags;

		copy = (flags & ENSURECOPY) || \
			((flags & CONTIGUOUS) && (!(arrflags & CONTIGUOUS))) \
			|| (PyArray_ITEMSIZE(arr) != itemsize) || \
			((flags & ALIGNED) && (!(arrflags & ALIGNED))) || \
			((flags & NOTSWAPPED) && (!(arrflags & NOTSWAPPED))) \
			|| (arr->nd > 1 &&				\
			    ((flags & FORTRAN) != (arrflags & FORTRAN))) || \
			((flags & WRITEABLE) && (!(arrflags & WRITEABLE)));
		
		if (copy) {
                        if ((flags & UPDATEIFCOPY) && \
                            (!PyArray_ISWRITEABLE(arr))) {
                                PyErr_SetString(PyExc_ValueError, msg);
                                return NULL;
                        }
                        if ((flags & ENSUREARRAY) && \
                            (subtype != &PyBigArray_Type)) {
                                subtype = &PyArray_Type;
                        }
			ret = (PyArrayObject *)         \
				PyArray_New(subtype, 
					    arr->nd, 
					    arr->dimensions,
					    type,
					    NULL, NULL, 
					    itemsize,
					    flags & FORTRAN,
					    (PyObject *)arr);
			if (PyArray_CopyInto(ret, arr) == -1) return NULL;
			if (flags & UPDATEIFCOPY)  {
				ret->flags |= UPDATEIFCOPY;
				ret->base = (PyObject *)arr;
                                PyArray_FLAGS(ret->base) &= ~WRITEABLE;
				Py_INCREF(arr);
			}
		} 
		/* If no copy then just increase the reference
		   count and return the input */
		else {  
                        if ((flags & ENSUREARRAY) && \
                            (subtype != &PyBigArray_Type)) {
                                ret = (PyArrayObject *) \
                                        PyArray_New(&PyArray_Type,
                                                    arr->nd,
                                                    arr->dimensions,
                                                    arr->descr->type_num,
                                                    arr->strides,
                                                    arr->data,
                                                    arr->itemsize,
                                                    arr->flags,NULL);
                                if (ret == NULL) return NULL;
                                ret->base = (PyObject *)arr;
                                ret->flags &= ~UPDATEIFCOPY;
                        }
                        else {
                                ret = arr;
                        }
			Py_INCREF(arr);
		}
	}
	
	/* The desired output type is different than the input
	   array type */
	else {
		/* Cast to the desired type if we can do it safely
		   Also cast if source is a ndim-0 array to mimic
		   behavior with Python scalars */
		if (flags & FORCECAST || PyArray_NDIM(arr)==0 ||
		    PyArray_CanCastSafely(PyArray_TYPE(arr), type)) {
                        if ((flags & UPDATEIFCOPY) && \
                            (!PyArray_ISWRITEABLE(arr))) {
                                PyErr_SetString(PyExc_ValueError, msg);
                                return NULL;
                        }
                        if ((flags & ENSUREARRAY) && \
                            (subtype != &PyBigArray_Type)) {
                                subtype = &PyArray_Type;
                        }
                        ret = (PyArrayObject *)\
                                PyArray_New(subtype, arr->nd,
                                            arr->dimensions, typecode->type_num,
                                            NULL, NULL, typecode->itemsize,
                                            typecode->fortran, 
                                            (PyObject *)arr);
                        if (ret == NULL) return NULL;
                        if (PyArray_CastTo(ret, arr) < 0) {
                                Py_DECREF(ret);
                                return NULL;
                        }
			if (flags & UPDATEIFCOPY)  {
				ret->flags |= UPDATEIFCOPY;
				ret->base = (PyObject *)arr;
                                PyArray_FLAGS(ret->base) &= ~WRITEABLE;
				Py_INCREF(arr);
			}
		}
		else {
			PyErr_SetString(PyExc_TypeError, 
					"array cannot be safely cast " \
					"to required type");
			ret = NULL;
		}
	}
	return (PyObject *)ret;
}


static int
_array_typecode_fromstr(char *str, int *swap, PyArray_Typecode *type)
{
    int type_num;
    char typechar;
    int size;
    unsigned long number = 1;
    char *s;
    char msg[] = "unsupported typestring";
    
    s = (char *)&number;   /* s[0] == 0 implies big-endian */

    *swap = 0;

    if (str[0] == '<' || str[0] == '>') {
	if ((str[0] == '<') && (s[0] == 0)) *swap = 1;
	else if ((str[0] == '>') && (s[0] != 0)) *swap = 1;
    }
    str += 1;
    
#define _MY_FAIL {				    \
	    PyErr_SetString(PyExc_ValueError, msg); \
	    return -1;			    \
    }		

    typechar = str[0];
    size = atoi(str + 1);
    switch (typechar) {
    case 'b':
	    if (size == sizeof(Bool))
		    type_num = PyArray_BOOL;	    
	    else _MY_FAIL 
	    break;		    
    case 'u':
            if (size == sizeof(uintp))
                    type_num = PyArray_UINTP;
	    else if (size == sizeof(char))
		    type_num = PyArray_UBYTE;
	    else if (size == sizeof(short)) 
		    type_num = PyArray_USHORT;
	    else if (size == sizeof(int)) 
		    type_num = PyArray_UINT;
	    else if (size == sizeof(ulong)) 
		    type_num = PyArray_ULONG;
	    else if (size == sizeof(ulonglong))
		    type_num = PyArray_ULONGLONG;
	    else _MY_FAIL
	    break;		    
    case 'i':
            if (size == sizeof(intp))
                    type_num = PyArray_INTP;
	    else if (size == sizeof(char)) 
		    type_num = PyArray_BYTE;
	    else if (size == sizeof(short)) 
		    type_num = PyArray_SHORT;
	    else if (size == sizeof(int))
		    type_num = PyArray_INT;
	    else if (size == sizeof(long)) 
		    type_num = PyArray_LONG;
	    else if (size == sizeof(longlong))
		    type_num = PyArray_LONGLONG;
	    else _MY_FAIL
	    break;		    
    case 'f':
	    if (size == sizeof(float))
		    type_num = PyArray_FLOAT;
	    else if (size == sizeof(double))
		    type_num = PyArray_DOUBLE;
	    else if (size == sizeof(longdouble))
		    type_num = PyArray_LONGDOUBLE;
	    else _MY_FAIL
	    break;
    case 'c':
	    if (size == sizeof(float)*2)
		    type_num = PyArray_CFLOAT;
	    else if (size == sizeof(double)*2)
		    type_num = PyArray_CDOUBLE;
	    else if (size == sizeof(longdouble)*2)
		    type_num = PyArray_CLONGDOUBLE;
	    else _MY_FAIL
	    break;
    case 'O':
	    if (size == sizeof(PyObject *))
		    type_num = PyArray_OBJECT;
	    else _MY_FAIL
	    break;
    case 'S':
	    type_num = PyArray_STRING;
	    break;
    case 'U':
	    type_num = PyArray_UNICODE;
	    size *= sizeof(Py_UNICODE);
	    break;	    
    case 'V':
	    type_num = PyArray_VOID;
	    break;
    default:
	    _MY_FAIL
    }

#undef _MY_FAIL

    type->type_num = type_num;
    type->itemsize = size;
    type->fortran = 0;
    return 0;
}

static PyObject *
array_fromstructinterface(PyObject *input, PyArray_Typecode *intype, int flags)
{
        PyArray_Typecode thetype = {0,0,0};
        int swap;
        char buf[40];
        PyArrayInterface *inter;
        PyObject *attr, *r;
        
        attr = PyObject_GetAttrString(input, "__array_struct__");
        if (attr == NULL) {PyErr_Clear(); return Py_NotImplemented;}
        if (!PyCObject_Check(attr) ||                                   \
            ((inter=((PyArrayInterface *)PyCObject_AsVoidPtr(attr)))->version != 2)) {
                PyErr_SetString(PyExc_ValueError, "invalid __array_struct__");
                return NULL;
        }
        snprintf(buf, 40, "|%c%d", inter->typekind, inter->itemsize);
        if (_array_typecode_fromstr(buf, &swap, &thetype) < 0) {
                return NULL;
        }
        r = PyArray_New(&PyArray_Type, inter->nd, inter->shape, thetype.type_num,
                        inter->strides, inter->data, thetype.itemsize,
                        inter->flags, NULL);
	Py_INCREF(input);
	PyArray_BASE(r) = input;
        Py_DECREF(attr);
        PyArray_UpdateFlags((PyArrayObject *)r, UPDATE_ALL_FLAGS);
        return r;
}

static PyObject *
array_frominterface(PyObject *input, PyArray_Typecode *intype, int flags)
{
	PyObject *attr=NULL, *item=NULL, *r;
        PyObject *tstr, *shape;        
	PyArrayObject *ret=NULL;
	PyArray_Typecode type;
	char *data;
	int buffer_len;
	int res, i, n;
	intp dims[MAX_DIMS], strides[MAX_DIMS];
	int swap;
	int dataflags = BEHAVED_FLAGS;

	/* Get the memory from __array_data__ and __array_offset__ */
	/* Get the shape */
	/* Get the typestring -- ignore array_descr */
	/* Get the strides */
	
        shape = PyObject_GetAttrString(input, "__array_shape__");
        if (shape == NULL) {PyErr_Clear(); return Py_NotImplemented;}
        tstr = PyObject_GetAttrString(input, "__array_typestr__");
        if (tstr == NULL) {Py_DECREF(shape); PyErr_Clear(); return Py_NotImplemented;}
        
	attr = PyObject_GetAttrString(input, "__array_data__");
	if ((attr == NULL) || (attr==Py_None) || (!PyTuple_Check(attr))) {
		if (attr && (attr != Py_None)) item=attr;
		else item=input;
		res = PyObject_AsWriteBuffer(item, (void **)&data, 
					     &buffer_len);
		if (res < 0) {
			PyErr_Clear();
			res = PyObject_AsReadBuffer(item, (const void **)&data,
						    &buffer_len);
			if (res < 0) {Py_XDECREF(attr); return NULL;}
			dataflags &= ~WRITEABLE;
		}
		attr = PyObject_GetAttrString(input, "__array_offset__");
		if (attr) {
			long num = PyInt_AsLong(attr);
			if (error_converting(num)) {
				PyErr_SetString(PyExc_TypeError, 
						"__array_offset__ "\
						"must be an integer");
				return NULL;
			}
			data += num;
		}
		else PyErr_Clear();
	}
	else {
		if (PyTuple_GET_SIZE(attr) != 2) {
			Py_DECREF(attr);
			PyErr_SetString(PyExc_TypeError, 
					"__array_data__ must return "	\
					"a 2-tuple with ('data pointer "\
					"string', read-only flag)");
			return NULL;
		}
		res = sscanf(PyString_AsString(PyTuple_GET_ITEM(attr,0)),
			     "%p", (void **)&data);
		if (res < 1) {
			Py_DECREF(attr);
			PyErr_SetString(PyExc_TypeError, 
					"__array_data__ string cannot be " \
					"converted");
			return NULL;
		}
		if (PyObject_IsTrue(PyTuple_GET_ITEM(attr,1))) {
			dataflags &= ~WRITEABLE;
		}
	}
	Py_XDECREF(attr);
	attr = tstr;
	if (!PyString_Check(attr)) {
		PyErr_SetString(PyExc_TypeError, "__array_typestr__ must be a string");
		Py_DECREF(attr);
		return NULL;	
	}
	res = _array_typecode_fromstr(PyString_AS_STRING(attr), &swap, &type);
	Py_DECREF(attr);
	if (res < 0) return NULL;    
	attr = shape;
	if (!PyTuple_Check(attr)) {
		PyErr_SetString(PyExc_TypeError, "__array_shape__ must be a tuple");
		Py_DECREF(attr);
		return NULL;
	}
	n = PyTuple_GET_SIZE(attr);
	for (i=0; i<n; i++) {
		item = PyTuple_GET_ITEM(attr, i);
		dims[i] = PyArray_PyIntAsIntp(item);
		if (error_converting(dims[i])) break;
	}
	Py_DECREF(attr);

	ret = (PyArrayObject *)PyArray_New(&PyArray_Type, n, dims, 
					   type.type_num, 
					   NULL, data, type.itemsize, 
                                           dataflags, NULL);
	if (ret == NULL) return NULL;
	Py_INCREF(input);
	ret->base = input;
    
	attr = PyObject_GetAttrString(input, "__array_strides__");
	if (attr != NULL && attr != Py_None) {
		if (!PyTuple_Check(attr)) {
			PyErr_SetString(PyExc_TypeError, 
					"__array_strides__ must be a tuple");
			Py_DECREF(attr);
			return NULL;
		}
		if (n != PyTuple_GET_SIZE(attr)) {
			PyErr_SetString(PyExc_ValueError, 
					"mismatch in length of "\
					"__array_strides__ and "\
					"__array_shape__");
			Py_DECREF(attr);
			return NULL;
		}
		for (i=0; i<n; i++) {
			item = PyTuple_GET_ITEM(attr, i);
			strides[i] = PyArray_PyIntAsIntp(item);
			if (error_converting(strides[i])) break;
		}
		Py_DECREF(attr);
		if (PyErr_Occurred()) PyErr_Clear();	
		memcpy(ret->strides, strides, n*sizeof(intp));
	}
	else PyErr_Clear();

	if (swap) {
		PyObject *tmp;
                tmp = PyArray_Byteswap(ret, TRUE);
		Py_DECREF(tmp);
	}

	PyArray_UpdateFlags(ret, UPDATE_ALL_FLAGS);
	r = array_fromarray(ret, intype, flags);
	Py_DECREF(ret);
	return r;
}

static PyObject *
array_fromattr(PyObject *op, PyArray_Typecode *typecode, int flags) 
{
        PyObject *new, *r;
	        
        if (typecode->type_num == PyArray_NOTYPE) {
                new = PyObject_CallMethod(op, "__array__", NULL);
        } else {
		PyArray_Descr *descr;
		PyObject *obj;

		descr = PyArray_DescrFromType(typecode->type_num);
		if (PyTypeNum_ISFLEXIBLE(typecode->type_num)) {
			obj = PyString_FromFormat("%c%d", descr->type,
						  typecode->itemsize);
		}
		else { 
			obj = (PyObject *)(descr->typeobj); Py_INCREF(obj);
		}
		new = PyObject_CallMethod(op, "__array__", "N", obj);
        }
        if (new == NULL) return NULL;
        if (!PyArray_Check(new)) {
                PyErr_SetString(PyExc_ValueError, 
                                "object __array__ method not "  \
                                "producing an array");
                Py_DECREF(new);
                return NULL;
        }
        r = array_fromarray((PyArrayObject *)new, typecode, flags);
        Py_DECREF(new);
        return r;
}        


static PyObject *
array_fromobject(PyObject *op, PyArray_Typecode *typecode, int min_depth, 
		  int max_depth, int flags) 
{
        /* This is the main code to make a NumPy array from a Python
           Object.  It is called from lot's of different places which
           is why there are so many checks.  The comments try to
           explain some of the checks. */

	int type = typecode->type_num;
        PyObject *r=NULL;
        int seq = FALSE;

	/* Is input object already an array? */
	/*  This is where the flags are used */
        if (PyArray_Check(op)) 
		r = array_fromarray((PyArrayObject *)op, typecode, flags);
	else if (PyArray_IsScalar(op, Generic)) {
		r = PyArray_FromScalar(op, typecode);
	}
        else if ((r = array_fromstructinterface(op, typecode, flags)) != Py_NotImplemented) {
        }
        else if ((r = array_frominterface(op, typecode, flags)) != Py_NotImplemented) {
	}
        else if (PyObject_HasAttrString(op, "__array__")) {
                /* Code that returns the object to convert for a non
		   multiarray input object from the __array__ attribute of the
		   object. */
                r = array_fromattr(op, typecode, flags);
        }
	else {
		if (type == PyArray_NOTYPE) {
			_array_find_type(op, NULL, typecode, MAX_DIMS);
		}
		if (PySequence_Check(op))  /* necessary but not sufficient */
		{
			r = Array_FromSequence(op, typecode, 
					       min_depth, max_depth);
			if (PyErr_Occurred() && r == NULL)
                            /* It wasn't really a sequence after all.
                             * Try interpreting it as a scalar */
                            PyErr_Clear();
                        else
                            seq = TRUE;
                }
                if (!seq)
			r = Array_FromScalar(op, typecode);
	}

        /* If we didn't succeed return NULL */
        if (r == NULL) return NULL;
	
	/* Be sure we succeed here */
	
        if(!PyArray_Check(r)) {
                PyErr_SetString(PyExc_RuntimeError, 
				"internal error: array_fromobject "\
				"not producing an array");
		Py_DECREF(r);
                return NULL;
        }

        if (min_depth != 0 && ((PyArrayObject *)r)->nd < min_depth) {
                Py_DECREF(r);
                PyErr_SetString(PyExc_ValueError, 
                                "object of too small depth for desired array");
                return NULL;
        }
        if (max_depth != 0 && ((PyArrayObject *)r)->nd > max_depth) {
                Py_DECREF(r);
                PyErr_SetString(PyExc_ValueError, 
                                "object too deep for desired array");
                return NULL;
        }
        return r;
}

static void
PyArray_ArrayType(PyObject *op, PyArray_Typecode *intype, 
		  PyArray_Typecode *outtype) 
{
	_array_find_type(op, intype, outtype, MAX_DIMS);
	return;
}

static int 
PyArray_ObjectType(PyObject *op, int minimum_type) 
{
	PyArray_Typecode intype = {0,0,0};
        PyArray_Typecode outtype = {0,0,0};
	intype.type_num = minimum_type;
	_array_find_type(op, &intype, &outtype, MAX_DIMS);
	return outtype.type_num;
}



/* flags is any of 
  CONTIGUOUS, 
  FORTRAN, (or set typecode->fortran=1)
  ALIGNED, 
  NOTSWAPPED, 
  WRITEABLE, 
  ENSURECOPY, 
  UPDATEIFCOPY,
  FORCECAST,
  ENSUREARRAY

   or'd (|) together

   Any of these flags present means that the returned array should 
   guarantee that aspect of the array.  Otherwise the returned array
   won't guarantee it -- it will depend on the object as to whether or 
   not it has such features. 

   Note that ENSURECOPY is enough
   to guarantee CONTIGUOUS, ALIGNED, NOTSWAPPED, and WRITEABLE
   and therefore it is redundant to include those as well. 

   BEHAVED_FLAGS == ALIGNED | NOTSWAPPED | WRITEABLE
   BEHAVED_FLAGS_RO == ALIGNED | NOTSWAPPED
   CARRAY_FLAGS = CONTIGUOUS | BEHAVED_FLAGS
   FARRAY_FLAGS = FORTRAN | BEHAVED_FLAGS
   
   typecode->fortran can be set to request a
   fortran-contiguous  array.  Fortran arrays are always behaved (aligned, 
   notswapped, and writeable) and not (C) CONTIGUOUS.  Note that either
   FORTRAN in the flag or typecode->fortran = 1 is enough to request
   a FORTRAN-style array. 

   UPDATEIFCOPY flag sets this flag in the returned array if a copy is
   made and the base argument points to the (possibly) misbehaved array.
   When the new array is deallocated, the original array held in base
   is updated with the contents of the new array. 

   FORCECAST will cause a cast to occur regardless of whether or not
   it is safe. 
*/


static PyObject *
PyArray_FromAny(PyObject *op, PyArray_Typecode *typecode, int min_depth, 
		int max_depth, int requires) 
{
 	PyArray_Typecode mine = {PyArray_NOTYPE, 0, 0};
	PyArray_Typecode *type;
	
	if (typecode == NULL) type = &mine;
	else type = typecode;
		
	if (requires & ENSURECOPY) {
		requires |= DEFAULT_FLAGS;
	}
	/* Ensure that type->fortran and flags & FORTRAN are the
	   same */
	if (requires & FORTRAN) typecode->fortran = 1;
	if (type->fortran == 1) {
		requires |= FARRAY_FLAGS;
		if (min_depth > 2) requires &= ~CONTIGUOUS;
	}

	/* make sure itemsize is not 0 unless warranted. */
	if ((type->itemsize == 0) && (type->type_num != PyArray_NOTYPE)) {
		PyArray_Descr *descr;
		descr = PyArray_DescrFromType(type->type_num);
		if (descr != NULL) type->itemsize = descr->elsize;
		else return NULL;
	}
	
	return array_fromobject(op, type, min_depth, max_depth, 
				requires);	
}

/* This is a quick wrapper around PyArray_FromAny(op, NULL, 0, 0, 
    ENSUREARRAY) */
/*  that special cases Arrays and PyArray_Scalars up front */
/*  It steals a reference to the object */
/*  It also guarantees that the result is PyArray_Type or PyBigArray_Type */

/*  Because it decrefs op if any conversion needs to take place 
    -- so it can be used like PyArray_EnsureArray(some_function(...)) */

static PyObject *
PyArray_EnsureArray(PyObject *op)
{
        PyObject *new;

        if (op == NULL) return NULL;

        if (PyArray_CheckExact(op) || PyBigArray_CheckExact(op)) return op;
        
        if (PyArray_IsScalar(op, Generic)) {
                new = PyArray_FromScalar(op, NULL);
                Py_DECREF(op);
                return new;
        }
        new = PyArray_FromAny(op, NULL, 0, 0, ENSUREARRAY);
        Py_DECREF(op);
        return new;
}

/* These are all compressed into a single API */
/* Deprecated calls -- Use PyArray_FromAny */

static PyObject *
PyArray_FromObject(PyObject *op, int type, int min_depth, int max_depth) 
{
	PyArray_Typecode typecode = {0, 0, 0};
	typecode.type_num = type;
        return PyArray_FromAny(op, &typecode, min_depth,
			       max_depth, BEHAVED_FLAGS | ENSUREARRAY);
}

static PyObject *
PyArray_ContiguousFromObject(PyObject *op, int type, int min_depth, 
			     int max_depth) 
{
	PyArray_Typecode typecode = {0, 0, 0};
	typecode.type_num = type;
        return PyArray_FromAny(op, &typecode, min_depth,
			       max_depth, DEFAULT_FLAGS | ENSUREARRAY);
}

static PyObject *
PyArray_CopyFromObject(PyObject *op, int type, int min_depth, 
		       int max_depth) 
{
	PyArray_Typecode typecode = {0, 0, 0};
	typecode.type_num = type;
        return PyArray_FromAny(op, &typecode, min_depth, max_depth,
			       ENSURECOPY | ENSUREARRAY);
}

/* End of deprecated */

static PyObject *
PyArray_ContiguousFromAny(PyObject *op, int type, int min_depth, 
			  int max_depth)
{
	PyArray_Typecode typecode = {0,0,0};
	typecode.type_num = type;
        return PyArray_FromAny(op, &typecode, min_depth,
			       max_depth, DEFAULT_FLAGS);
}	

static int 
PyArray_CanCastSafely(int fromtype, int totype) 
{
	PyArray_Descr *from, *to;

        if (fromtype == totype) return 1;
        if (fromtype == PyArray_BOOL) return 1;
	if (totype == PyArray_BOOL) return 0;
        if (totype == PyArray_OBJECT || totype == PyArray_VOID) return 1;
	if (fromtype == PyArray_OBJECT || fromtype == PyArray_VOID) return 0;

	from = PyArray_DescrFromType(fromtype);
	to = PyArray_DescrFromType(totype);

        switch(fromtype) {
        case PyArray_BYTE:
	case PyArray_SHORT:
        case PyArray_INT:
        case PyArray_LONG:
	case PyArray_LONGLONG:
		if (PyTypeNum_ISINTEGER(totype)) {
			if (PyTypeNum_ISUNSIGNED(totype)) {
				return (to->elsize > from->elsize);
			}
			else {
				return (to->elsize >= from->elsize);
			}
		}
		else if (PyTypeNum_ISFLOAT(totype)) {
                        if (from->elsize < 8)
                                return (to->elsize > from->elsize);
                        else
                                return (to->elsize >= from->elsize);
		}
		else if (PyTypeNum_ISCOMPLEX(totype)) {
                        if (from->elsize < 8)
                                return ((to->elsize >> 1) > from->elsize);
                        else
                                return ((to->elsize >> 1) >= from->elsize);
		}
		else return totype > fromtype;
        case PyArray_UBYTE:
        case PyArray_USHORT:
        case PyArray_UINT:
	case PyArray_ULONG:
	case PyArray_ULONGLONG:
		if (PyTypeNum_ISINTEGER(totype)) {
			if (PyTypeNum_ISSIGNED(totype)) {
				return (to->elsize > from->elsize);
			}
			else {
				return (to->elsize >= from->elsize);
			}
		}
		else if (PyTypeNum_ISFLOAT(totype)) {
                        if (from->elsize < 8)
                                return (to->elsize > from->elsize);
                        else
                                return (to->elsize >= from->elsize);
		}
		else if (PyTypeNum_ISCOMPLEX(totype)) {
                        if (from->elsize < 8)
                                return ((to->elsize >> 1) > from->elsize);
                        else
                                return ((to->elsize >> 1) >= from->elsize);
		}
		else return totype > fromtype;
        case PyArray_FLOAT:
        case PyArray_DOUBLE:
	case PyArray_LONGDOUBLE:
		if (PyTypeNum_ISCOMPLEX(totype)) 
			return ((to->elsize >> 1) >= from->elsize);
		else
			return (totype > fromtype);
        case PyArray_CFLOAT:
        case PyArray_CDOUBLE:
	case PyArray_CLONGDOUBLE:
		return (totype > fromtype);
	case PyArray_STRING:
	case PyArray_UNICODE:
		return (totype > fromtype);
        default:
                return 0;
        }
}

static Bool
PyArray_CanCastTo(PyArray_Typecode *from, PyArray_Typecode *to)
{
	int fromtype=from->type_num;
	int totype=to->type_num;
	Bool ret;

	ret = (Bool) PyArray_CanCastSafely(fromtype, totype);
	if (ret) { /* Check String and Unicode more closely */
		if (fromtype == PyArray_STRING) {
			if (totype == PyArray_STRING) {
				ret = (from->itemsize <= to->itemsize);
			}
			else if (totype == PyArray_UNICODE) {
				ret = (from->itemsize * sizeof(Py_UNICODE)\
				       <= to->itemsize);
			}
		}
		else if (fromtype == PyArray_UNICODE) {
			if (totype == PyArray_UNICODE) {
				ret = (from->itemsize <= to->itemsize);
			}
		}
		/* TODO: If totype is STRING or unicode 
		    see if the length is long enough to hold the
		    stringified value of the object.		    
		*/
	}
	return ret;
}



/*********************** Element-wise Array Iterator ***********************/
/*  Aided by Peter J. Verveer's  nd_image package and scipy's arraymap  ****/
/*         and Python's array iterator                                   ***/
                     

static PyObject *
PyArray_IterNew(PyObject *obj)
{
        PyArrayIterObject *it;
	int i, nd; 
	PyArrayObject *ao = (PyArrayObject *)obj;

        if (!PyArray_Check(ao)) {
                PyErr_BadInternalCall();
                return NULL;
        }

        it = PyObject_GC_New(PyArrayIterObject, &PyArrayIter_Type);
        if (it == NULL)
                return NULL;

	nd = ao->nd;
	PyArray_UpdateFlags(ao, CONTIGUOUS);
	it->contiguous = 0;
	if PyArray_ISCONTIGUOUS(ao) it->contiguous = 1;
        Py_INCREF(ao);
        it->ao = ao;
	it->size = PyArray_SIZE(ao);
	it->nd_m1 = nd - 1;
	it->factors[nd-1] = 1;
	for (i=0; i < nd; i++) {
		it->dims_m1[i] = it->ao->dimensions[i] - 1;
		it->strides[i] = it->ao->strides[i];
		it->backstrides[i] = it->strides[i] *	\
			it->dims_m1[i];
		if (i > 0)
			it->factors[nd-i-1] = it->factors[nd-i] *	\
				it->ao->dimensions[nd-i];
	}
	PyArray_ITER_RESET(it);
	
        PyObject_GC_Track(it);
        return (PyObject *)it;
}

/* Returns an array scalar holding the element desired */

static PyObject *
arrayiter_next(PyArrayIterObject *it)
{
	PyObject *ret;

	if (it->index < it->size) {
		ret = PyArray_ToScalar(it->dataptr, it->ao);
		PyArray_ITER_NEXT(it);
		return ret;
	}
        return NULL;
}

static void
arrayiter_dealloc(PyArrayIterObject *it)
{
        PyObject_GC_UnTrack(it);
        Py_XDECREF(it->ao);
        PyObject_GC_Del(it);
}

static int
arrayiter_traverse(PyArrayIterObject *it, visitproc visit, void *arg)
{
        if (it->ao != NULL)
                return visit((PyObject *)(it->ao), arg);
        return 0;
}


static int
iter_length(PyArrayIterObject *self) 
{
        return (int) self->size;
}


static PyObject *
iter_subscript_Bool(PyArrayIterObject *self, PyArrayObject *ind)
{
	int index, strides, itemsize;
	intp count=0;
	char *dptr, *optr;
	PyObject *r;
	int swap;
        PyArray_CopySwapFunc *copyswap;


	if (ind->nd != 1) {
		PyErr_SetString(PyExc_ValueError, 
				"boolean index array should have 1 dimension");
		return NULL;
	}
	index = (ind->dimensions[0]);
	strides = ind->strides[0];
	dptr = ind->data;
	/* Get size of return array */
	while(index--) {
		if (*((Bool *)dptr) != 0)
			count++;
		dptr += strides;
	}
	itemsize = self->ao->itemsize;
	r = PyArray_New(self->ao->ob_type, 1, &count, 
			self->ao->descr->type_num, NULL, NULL,
			itemsize, 0, (PyObject *)self->ao);
	if (r==NULL) return NULL;

	/* Set up loop */
	optr = PyArray_DATA(r);
	index = ind->dimensions[0];
	dptr = ind->data;

        copyswap = self->ao->descr->copyswap;
	/* Loop over Boolean array */
	swap = !(PyArray_ISNOTSWAPPED(self->ao));
	while(index--) {
		if (*((Bool *)dptr) != 0) {
                        copyswap(optr, self->dataptr, swap, itemsize);
			optr += itemsize;
		}
		dptr += strides;
		PyArray_ITER_NEXT(self);
	}
	PyArray_ITER_RESET(self);
	return r;
}

static PyObject *
iter_subscript_int(PyArrayIterObject *self, PyArrayObject *ind)
{
	intp num;
	PyObject *r;
	PyArrayIterObject *ind_it;
	int itemsize;
	int swap;
	char *optr;
	int index;
        PyArray_CopySwapFunc *copyswap;

	itemsize = self->ao->itemsize;
	if (ind->nd == 0) {
		num = *((intp *)ind->data);
		PyArray_ITER_GOTO1D(self, num);
		r = PyArray_ToScalar(self->dataptr, self->ao);
		PyArray_ITER_RESET(self);
		return r;
	}
	
	r = PyArray_New(self->ao->ob_type, ind->nd, ind->dimensions,
			self->ao->descr->type_num, NULL, 
			NULL, self->ao->itemsize, 
			0, (PyObject *)self->ao);
	if (r==NULL) return NULL;

	optr = PyArray_DATA(r);
	ind_it = (PyArrayIterObject *)PyArray_IterNew((PyObject *)ind);
	if (ind_it == NULL) {Py_DECREF(r); return NULL;}
	index = ind_it->size;
        copyswap = PyArray_DESCR(r)->copyswap;
        swap = !PyArray_ISNOTSWAPPED(self->ao);
	while(index--) {
		num = *((intp *)(ind_it->dataptr));
		if (num < 0) num += self->size;
		if (num < 0 || num >= self->size) {
			PyErr_Format(PyExc_IndexError,
				     "index %d out of bounds"		\
				     " 0<=index<%d", (int) num, 
				     (int) self->size);
			Py_DECREF(ind_it);
			Py_DECREF(r);
			PyArray_ITER_RESET(self);
			return NULL;
		}
		PyArray_ITER_GOTO1D(self, num);
                copyswap(optr, self->dataptr, swap, itemsize);
		optr += itemsize;
		PyArray_ITER_NEXT(ind_it);
	}
	Py_DECREF(ind_it);
	PyArray_ITER_RESET(self);
	return r;
}


static PyObject *
iter_subscript(PyArrayIterObject *self, PyObject *ind)
{
	int i;
	PyArray_Typecode indtype = {PyArray_INTP, 0, 0};
	int start, step_size;
	intp n_steps;
	PyObject *r;
	char *dptr;
	int size;
	PyObject *obj = NULL;
	int swap;
        PyArray_CopySwapFunc *copyswap;
		

	if (ind == Py_Ellipsis) {
		ind = PySlice_New(NULL, NULL, NULL);
		obj = iter_subscript(self, ind);
		Py_DECREF(ind);
		return obj;
	}

	/* Tuples not accepted --- i.e. no NewAxis */
	/* Could implement this with adjusted strides
	   and dimensions in iterator */

	/* Check for Boolean -- this is first becasue
	   Bool is a subclass of Int */
	PyArray_ITER_RESET(self);

	if (PyBool_Check(ind)) {
		if (PyObject_IsTrue(ind)) {
			return PyArray_ToScalar(self->dataptr, self->ao);
		}
		else { /* empty array */
			intp ii = 0;
			r = PyArray_New(self->ao->ob_type, 1, &ii, 
					self->ao->descr->type_num, NULL, 
					NULL, self->ao->itemsize, 0,
					(PyObject *)self->ao);
			return r;			
		}
	}

	/* Check for Integer or Slice */
	
	
	if (PyLong_Check(ind) || PyInt_Check(ind) || PySlice_Check(ind)) {
		start = parse_subindex(ind, &step_size, &n_steps, 
				       self->size);
		if (start == -1) 
			goto fail;
		if (n_steps == RubberIndex || n_steps == PseudoIndex) {
			PyErr_SetString(PyExc_IndexError, 
					"cannot use Ellipsis or NewAxes here");
			goto fail;
		}
		PyArray_ITER_GOTO1D(self, start)
		if (n_steps == SingleIndex) { /* Integer */
			r = PyArray_ToScalar(self->dataptr, self->ao);
			PyArray_ITER_RESET(self);
			return r;
		}
		size = self->ao->itemsize;
		r = PyArray_New(self->ao->ob_type, 1, &n_steps, 
				self->ao->descr->type_num, NULL, NULL,
				size, 0, (PyObject *)self->ao);
		if (r==NULL) goto fail; 
		dptr = PyArray_DATA(r);
                swap = !PyArray_ISNOTSWAPPED(self->ao);
                copyswap = PyArray_DESCR(r)->copyswap;
		while(n_steps--) {
                        copyswap(dptr, self->dataptr, swap, size);
			for(i=0; i< step_size; i++) 
				PyArray_ITER_NEXT(self);
			dptr += size;
		}
		PyArray_ITER_RESET(self);
		return r;
	} 

	/* convert to INTP array if Integer array scalar or List */

	if (PyArray_IsScalar(ind, Integer) || PyList_Check(ind)) {
		obj = PyArray_FromAny(ind, &indtype, 0, 0, FORCECAST);
	}
	else {
		Py_INCREF(ind);
		obj = ind;
	}
	
	if (PyArray_Check(obj)) {
		/* Check for Boolean object */
		if (PyArray_TYPE(obj)==PyArray_BOOL) {
			r = iter_subscript_Bool(self, (PyArrayObject *)obj);
		} 
		/* Check for integer array */
		else if (PyArray_ISINTEGER(obj)) {
			PyObject *new;
			new = PyArray_FromAny(obj, &indtype, 0, 0, 
					      FORCECAST | BEHAVED_FLAGS);
			if (new==NULL) goto fail;
			obj = new;
			r = iter_subscript_int(self, (PyArrayObject *)obj);
		}
		else {
			PyErr_SetString(PyExc_IndexError, 
					"unsupported iterator index");
			goto fail;
		}
		Py_DECREF(obj);
		return r;
	}

	PyErr_SetString(PyExc_IndexError, "unsupported iterator index");

 fail:
	Py_XDECREF(obj);
	return NULL;

}


static int
iter_ass_sub_Bool(PyArrayIterObject *self, PyArrayObject *ind,
		  PyArrayIterObject *val, int swap)
{
	int index, strides, itemsize;
	char *dptr;
        PyArray_CopySwapFunc *copyswap;

	if (ind->nd != 1) {
		PyErr_SetString(PyExc_ValueError, 
				"boolean index array should have 1 dimension");
		return -1;
	}
	itemsize = self->ao->itemsize;
	index = ind->dimensions[0];
	strides = ind->strides[0];
	dptr = ind->data;
	PyArray_ITER_RESET(self);
	/* Loop over Boolean array */
        copyswap = self->ao->descr->copyswap;
	while(index--) {
		if (*((Bool *)dptr) != 0) {
                        copyswap(self->dataptr, val->dataptr, swap,
                                  itemsize);
		}
		dptr += strides;
		PyArray_ITER_NEXT(self);
		PyArray_ITER_NEXT(val);
		if (val->index==val->size) 
			PyArray_ITER_RESET(val);
	}
	PyArray_ITER_RESET(self);
	return 0;
}

static int
iter_ass_sub_int(PyArrayIterObject *self, PyArrayObject *ind,
		   PyArrayIterObject *val, int swap)
{
	PyArray_Typecode typecode;
	intp num;
	PyArrayIterObject *ind_it;
	int itemsize;
	int index;
        PyArray_CopySwapFunc *copyswap;

	itemsize = typecode.itemsize = self->ao->itemsize;
	typecode.type_num = self->ao->descr->type_num;
        copyswap = self->ao->descr->copyswap;
	if (ind->nd == 0) {
		num = *((intp *)ind->data);
		PyArray_ITER_GOTO1D(self, num);
                copyswap(self->dataptr, val->dataptr, swap, itemsize);
		return 0;
	}
	ind_it = (PyArrayIterObject *)PyArray_IterNew((PyObject *)ind);
	if (ind_it == NULL) return -1;
	index = ind_it->size;
	while(index--) {
		num = *((intp *)(ind_it->dataptr));
		if (num < 0) num += self->size;
		if ((num < 0) || (num >= self->size)) {
			PyErr_Format(PyExc_IndexError,
				     "index %d out of bounds"		\
				     " 0<=index<%d", (int) num, 
				     (int) self->size);
			Py_DECREF(ind_it);
			return -1;
		}
		PyArray_ITER_GOTO1D(self, num);
                copyswap(self->dataptr, val->dataptr, swap, itemsize);
		PyArray_ITER_NEXT(ind_it);
		PyArray_ITER_NEXT(val);
		if (val->index == val->size) 
			PyArray_ITER_RESET(val);
	}
	Py_DECREF(ind_it);
	return 0;
}


static int
iter_ass_subscript(PyArrayIterObject *self, PyObject *ind, PyObject *val) 
{
	int i;
	PyObject *arrval=NULL;
	PyArrayIterObject *val_it=NULL;
	PyArray_Typecode type;
	PyArray_Typecode indtype = {PyArray_INTP, 0, 0};
	int swap;
	int itemsize;
	int start, step_size;
	intp n_steps;
	PyObject *obj;
        PyArray_CopySwapFunc *copyswap;
	
	if (ind == Py_Ellipsis) {
		ind = PySlice_New(NULL, NULL, NULL);
		i = iter_ass_subscript(self, ind, val);
		Py_DECREF(ind);
		return i;
	}
	
	type.type_num = self->ao->descr->type_num;
	itemsize = type.itemsize = self->ao->itemsize;
	type.fortran = 0;
	
	arrval = PyArray_FromAny(val, &type, 0, 0, 0);
	if (arrval==NULL) return -1;
	val_it = (PyArrayIterObject *)PyArray_IterNew(arrval);
	if (val_it==NULL) goto fail;

	/* Check for Boolean -- this is first becasue
	   Bool is a subclass of Int */

        copyswap = PyArray_DESCR(arrval)->copyswap;
	swap = (PyArray_ISNOTSWAPPED(self->ao)!=PyArray_ISNOTSWAPPED(arrval));
	if (PyBool_Check(ind)) {
		if (PyObject_IsTrue(ind)) {
                        copyswap(self->dataptr, PyArray_DATA(arrval), 
                                  swap, itemsize);
		}
		goto succeed;
	}

	/* Check for Integer or Slice */
	
	if (PyLong_Check(ind) || PyInt_Check(ind) || PySlice_Check(ind)) {
		start = parse_subindex(ind, &step_size, &n_steps, 
				       self->size);
		if (start == -1) goto fail;
		if (n_steps == RubberIndex || n_steps == PseudoIndex) {
			PyErr_SetString(PyExc_IndexError, 
					"cannot use Ellipsis or NewAxes here");
			goto fail;
		}
		PyArray_ITER_GOTO1D(self, start);
		if (n_steps == SingleIndex) { /* Integer */
                        copyswap(self->dataptr, PyArray_DATA(arrval),
                                  swap, itemsize);
			PyArray_ITER_RESET(self);
			goto succeed;
		}
		while(n_steps--) {
                        copyswap(self->dataptr, val_it->dataptr,
                                  swap, itemsize);
			for(i=0; i < step_size; i++) 
				PyArray_ITER_NEXT(self);
			PyArray_ITER_NEXT(val_it);
			if (val_it->index == val_it->size) 
				PyArray_ITER_RESET(val_it);
		}
		PyArray_ITER_RESET(self);
		goto succeed;
	} 

	/* convert to INTP array if Integer array scalar or List */

	if (PyArray_IsScalar(ind, Integer))
		obj = PyArray_FromScalar(ind, &indtype);
	else if (PyList_Check(ind)) {
		obj = PyArray_FromAny(ind, &indtype, 0, 0, FORCECAST);
	}
	else {
		Py_INCREF(ind);
		obj = ind;
	}
	
	if (PyArray_Check(obj)) {
		/* Check for Boolean object */
		if (PyArray_TYPE(obj)==PyArray_BOOL) {
			if (iter_ass_sub_Bool(self, (PyArrayObject *)obj,
					      val_it, swap) < 0)
				goto fail;
		} 
		/* Check for integer array */
		else if (PyArray_ISINTEGER(obj)) {
			PyObject *new;
			new = PyArray_FromAny(obj, &indtype, 0, 0, 
					      FORCECAST | BEHAVED_FLAGS);
			if (new==NULL) goto fail;
			Py_DECREF(obj);
			obj = new;
			if (iter_ass_sub_int(self, (PyArrayObject *)obj,
					     val_it, swap) < 0)
				goto fail;
		}
		else goto fail;
		Py_DECREF(obj);
		goto succeed;
	}

	PyErr_SetString(PyExc_IndexError, "unsupported iterator index");
	goto fail;

 succeed:
	Py_DECREF(val_it);
	Py_DECREF(arrval);
	return 0;

 fail:
	Py_XDECREF(val_it);
	Py_XDECREF(arrval);
	return -1;
	
}


static PyMappingMethods iter_as_mapping = {
        (inquiry)iter_length,		        /*mp_length*/
        (binaryfunc)iter_subscript,	        /*mp_subscript*/
        (objobjargproc)iter_ass_subscript,	/*mp_ass_subscript*/
};

static char doc_iter_array[] = "__array__(type=None)\n Get array "\
        "from iterator";

static PyObject *
iter_array(PyArrayIterObject *it, PyObject *op) 
{
       
        PyObject *r;
        intp size;

        /* Any argument ignored */

        /* Two options: 
            1) underlying array is contiguous
               -- return 1-d wrapper around it 
            2) underlying array is not contiguous
               -- make new 1-d contiguous array with updateifcopy flag set
                  to copy back to the old array
        */

        size = PyArray_SIZE(it->ao);
        if (PyArray_ISCONTIGUOUS(it->ao)) {
                r = PyArray_New(it->ao->ob_type, 1, &size, 
				it->ao->descr->type_num,
                                NULL, it->ao->data, it->ao->itemsize, 
				it->ao->flags,
                                (PyObject *)it->ao);                
		if (r==NULL) return NULL;
        }
        else {
                r = PyArray_New(it->ao->ob_type, 1, &size, 
				it->ao->descr->type_num,
                                NULL, NULL, it->ao->itemsize, 0, 
                                (PyObject *)it->ao);
		if (r==NULL) return NULL;
		if (PyArray_CopyInto((PyArrayObject *)r, it->ao) < 0) {
			Py_DECREF(r); 
			return NULL;
		}
                PyArray_FLAGS(r) |= UPDATEIFCOPY;
                it->ao->flags &= ~WRITEABLE;
        }
        Py_INCREF(it->ao);
        PyArray_BASE(r) = (PyObject *)it->ao;
        return r;
        
}

static char doc_iter_copy[] = "copy()\n Get a copy of 1-d array";

static PyObject *
iter_copy(PyArrayIterObject *it, PyObject *args)
{
        if (!PyArg_ParseTuple(args, "")) return NULL;	
	return PyArray_Flatten(it->ao, 0);
}

static PyMethodDef iter_methods[] = {
        /* to get array */
        {"__array__", (PyCFunction)iter_array, 1, doc_iter_array},
	{"copy", (PyCFunction)iter_copy, 1, doc_iter_copy},
        {NULL,		NULL}		/* sentinel */
};


static PyTypeObject PyArrayIter_Type = {
        PyObject_HEAD_INIT(NULL)
        0,					 /* ob_size */
        "scipy.flatiter",		         /* tp_name */
        sizeof(PyArrayIterObject),               /* tp_basicsize */
        0,					 /* tp_itemsize */
        /* methods */
        (destructor)arrayiter_dealloc,		/* tp_dealloc */
        0,					/* tp_print */
        0,					/* tp_getattr */
        0,					/* tp_setattr */
        0,					/* tp_compare */
        0,					/* tp_repr */
        0,					/* tp_as_number */
        0,     			                /* tp_as_sequence */
        &iter_as_mapping,        	        /* tp_as_mapping */
        0,					/* tp_hash */
        0,					/* tp_call */
        0,					/* tp_str */
        0,	                         	/* tp_getattro */
        0,					/* tp_setattro */
        0,					/* tp_as_buffer */
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,/* tp_flags */
        0,					/* tp_doc */
        (traverseproc)arrayiter_traverse,	/* tp_traverse */
        0,					/* tp_clear */
        0,					/* tp_richcompare */
        0,					/* tp_weaklistoffset */
        0,		    	                /* tp_iter */
        (iternextfunc)arrayiter_next,		/* tp_iternext */
        iter_methods,				/* tp_methods */
};

/** END of Array Iterator **/



/*********************** Subscript Array Iterator *************************
 *                                                                        *
 * This object handles subscript behavior for array objects.              *
 *  It is an iterator object with a next method                           *
 *  It abstracts the n-dimensional mapping behavior to make the looping   *
 *     code more understandable (maybe)                                   *
 *     and so that indexing can be set up ahead of time                   *
 */ 

/* This checks the args for any fancy indexing objects */

#define SOBJ_NOTFANCY 0 
#define SOBJ_ISFANCY 1
#define SOBJ_BADARRAY 2
#define SOBJ_TOOMANY 3
#define SOBJ_LISTTUP 4

static int
fancy_indexing_check(PyObject *args)
{
	int i, n;
	PyObject *obj;
	int retval = SOBJ_NOTFANCY;

	if (PyTuple_Check(args)) {
		n = PyTuple_GET_SIZE(args);
		if (n >= MAX_DIMS) return SOBJ_TOOMANY;
		for (i=0; i<n; i++) {
			obj = PyTuple_GET_ITEM(args,i);
			if (PyArray_Check(obj)) {
				if (PyArray_ISINTEGER(obj))
					retval = SOBJ_ISFANCY;
				else {
					retval = SOBJ_BADARRAY;
					break;
				}
			}
			else if (PySequence_Check(obj))
				retval = SOBJ_ISFANCY;
		}
	}	
	else if (PyArray_Check(args)) {
		if ((PyArray_TYPE(args)==PyArray_BOOL) ||
		    (PyArray_ISINTEGER(args)))
			return SOBJ_ISFANCY;
		else
			return SOBJ_BADARRAY;
	}
	else if (PySequence_Check(args)) {
		/* Sequences < MAX_DMS with any slice objects
		   or NewAxis, or Ellipsis is considered standard
		   as long as there are also no Arrays and or additional
		   sequences embedded.
		*/
		retval = SOBJ_ISFANCY;
		n = PySequence_Size(args);
		if (n<0 || n>=MAX_DIMS) return SOBJ_ISFANCY;
		for (i=0; i<n; i++) {
			obj = PySequence_GetItem(args, i);
			if (obj == NULL) return SOBJ_ISFANCY;
			if (PyArray_Check(obj)) {
				if (PyArray_ISINTEGER(obj))
					retval = SOBJ_LISTTUP;
				else
					retval = SOBJ_BADARRAY;
			}
			else if (PySequence_Check(obj)) {
				retval = SOBJ_LISTTUP;
			}
			else if (PySlice_Check(obj) || obj == Py_Ellipsis || \
			    obj == Py_None) {
				retval = SOBJ_NOTFANCY;
			}
			Py_DECREF(obj);
			if (retval > SOBJ_ISFANCY) return retval;
		}
	}

	return retval;
}

/* convert an indexing object to an INTP indexing array iterator
   if possible -- otherwise, it is a Slice or Ellipsis object
   and has to be interpreted on bind to a particular 
   array so leave it NULL for now.
 */
static int
_convert_obj(PyObject *obj, PyArrayIterObject **iter)
{
	PyArray_Typecode indtype = {PyArray_INTP, 0, 0};
	PyObject *arr;

	if (PySlice_Check(obj) || (obj == Py_Ellipsis))
		*iter = NULL;
	else {
		arr = PyArray_FromAny(obj, &indtype, 0, 0, FORCECAST);
		if (arr == NULL) return -1;
		*iter = (PyArrayIterObject *)PyArray_IterNew(arr);
		Py_DECREF(arr);
		if (*iter == NULL) return -1;
	}
	return 0;
}

/* Adjust dimensionality and strides for index object iterators 
   --- i.e. broadcast
 */
static int
PyArray_Broadcast(PyArrayMultiIterObject *mit)
{
	int i, nd, k, j;
	intp tmp;
	PyArrayIterObject *it;
	
	/* Discover the broadcast number of dimensions */
	for (i=0, nd=0; i<mit->numiter; i++) 
		nd = MAX(nd, mit->iters[i]->ao->nd);
	mit->nd = nd;

	/* Discover the broadcast shape in each dimension */
	for (i=0; i<nd; i++) {
		mit->dimensions[i] = 1;
		for (j=0; j<mit->numiter; j++) {
			it = mit->iters[j];
			/* This prepends 1 to shapes not already 
			   equal to nd */
			k = i + it->ao->nd - nd;
			if (k>=0) {
				tmp = it->ao->dimensions[k];
				if (tmp == 1) continue;
				if (mit->dimensions[i] == 1) 
					mit->dimensions[i] = tmp;
				else if (mit->dimensions[i] != tmp) {
					PyErr_SetString(PyExc_ValueError, 
							"index objects are " \
							"not broadcastable " \
							"to a single shape");
					return -1;
				}
			}
		}
	}

	/* Reset the iterator dimensions and strides of each iterator
	   object -- using 0 valued strides for broadcasting */

	tmp = PyArray_MultiplyList(mit->dimensions, mit->nd);
	mit->size = tmp;
	for (i=0; i<mit->numiter; i++) {
		it = mit->iters[i];
		it->nd_m1 = mit->nd - 1;
		it->size = tmp;
		nd = it->ao->nd;
		it->factors[mit->nd-1] = 1;
		for (j=0; j < mit->nd; j++) {
			it->dims_m1[j] = mit->dimensions[j] - 1;
			k = j + nd - mit->nd;
			/* If this dimension was added or shape
			   of underlying array was 1 */
			if ((k < 0) || \
			    it->ao->dimensions[k] != mit->dimensions[j]) {
				it->contiguous = 0;
				it->strides[j] = 0;
			}
			else {
				it->strides[j] = it->ao->strides[k];
			}
			it->backstrides[j] = it->strides[j] *	\
				it->dims_m1[j];
			if (j > 0)
				it->factors[mit->nd-j-1] =		\
					it->factors[mit->nd-j] *	\
					mit->dimensions[mit->nd-j];
		}
		PyArray_ITER_RESET(it);
	}
	return 0;
}

/* Reset the map iterator to the beginning */
static void
PyArray_MapIterReset(PyArrayMapIterObject *mit)
{
	int i,j; intp coord[MAX_DIMS];
	PyArrayIterObject *it;
	PyArray_CopySwapFunc *copyswap;

	mit->index = 0;

	copyswap = mit->iters[0]->ao->descr->copyswap;

	if (mit->subspace != NULL) {
		memcpy(coord, mit->bscoord, sizeof(intp)*mit->ait->ao->nd);
		PyArray_ITER_RESET(mit->subspace);
		for (i=0; i<mit->numiter; i++) {
			it = mit->iters[i];
			PyArray_ITER_RESET(it);
			j = mit->iteraxes[i];
			copyswap(coord+j,it->dataptr,
				 !PyArray_ISNOTSWAPPED(it->ao),
				 sizeof(intp));
		}
		PyArray_ITER_GOTO(mit->ait, coord);
		mit->subspace->dataptr = mit->ait->dataptr;
		mit->dataptr = mit->subspace->dataptr;
	}
	else {
		for (i=0; i<mit->numiter; i++) {
			it = mit->iters[i];
			PyArray_ITER_RESET(it);
			copyswap(coord+i,it->dataptr, 
				 !PyArray_ISNOTSWAPPED(it->ao),
				 sizeof(intp));
		}
		PyArray_ITER_GOTO(mit->ait, coord);
		mit->dataptr = mit->ait->dataptr;
	}
	return;
}

/* This function needs to update the state of the map iterator
   and point mit->dataptr to the memory-location of the next object
*/
static void
PyArray_MapIterNext(PyArrayMapIterObject *mit)
{
	int i, j;
	intp coord[MAX_DIMS];
	PyArrayIterObject *it;
	PyArray_CopySwapFunc *copyswap;

	mit->index += 1;
	if (mit->index >= mit->size) return;
	copyswap = mit->iters[0]->ao->descr->copyswap;
	/* Sub-space iteration */
	if (mit->subspace != NULL) {
		PyArray_ITER_NEXT(mit->subspace);
		if (mit->subspace->index == mit->subspace->size) {
			/* reset coord to coordinates of 
			   beginning of the subspace */
			memcpy(coord, mit->bscoord, 
			       sizeof(intp)*mit->ait->ao->nd);
			PyArray_ITER_RESET(mit->subspace);
			for (i=0; i<mit->numiter; i++) {
				it = mit->iters[i];
				PyArray_ITER_NEXT(it);
				j = mit->iteraxes[i];
				copyswap(coord+j,it->dataptr,
					 !PyArray_ISNOTSWAPPED(it->ao),
					 sizeof(intp));
			}
			PyArray_ITER_GOTO(mit->ait, coord);
			mit->subspace->dataptr = mit->ait->dataptr;
		}
		mit->dataptr = mit->subspace->dataptr;
	}
	else {
		for (i=0; i<mit->numiter; i++) {
			it = mit->iters[i];
			PyArray_ITER_NEXT(it);
			copyswap(coord+i,it->dataptr, 
				 !PyArray_ISNOTSWAPPED(it->ao),
				 sizeof(intp));
		}
		PyArray_ITER_GOTO(mit->ait, coord);
		mit->dataptr = mit->ait->dataptr;
	}
	return;
}

/*  Bind a mapiteration to a particular array */

/*  Determine if subspace iteration is necessary.  If so, 
         1) Fill in mit->iteraxes
	 2) Create subspace iterator
	 3) Update nd, dimensions, and size. 

    Subspace iteration is necessary if:  arr->nd > mit->numiter
*/

/* Need to check for index-errors somewhere.  

   Let's do it at bind time and also convert all <0 values to >0 here
   as well. 
*/
static void
PyArray_MapIterBind(PyArrayMapIterObject *mit, PyArrayObject *arr)
{
	int subnd;
	PyObject *sub=NULL, *obj=NULL;
	int i, j, n, curraxis, ellipexp, noellip;
	PyArrayIterObject *it;
	intp dimsize;
	intp *indptr;

	/* Remove old binding if any */
	Py_XDECREF(mit->ait);
	mit->ait = NULL;

	Py_XDECREF(mit->subspace);
	mit->subspace = NULL;
	
	subnd = arr->nd - mit->numiter;
	if (subnd < 0) {
		PyErr_SetString(PyExc_ValueError, 
				"too many indices for array");
		return;
	}

	mit->ait = (PyArrayIterObject *)PyArray_IterNew((PyObject *)arr);
	if (mit->ait == NULL) return;

	/* If this is just a view, then do nothing more    */
	/*   views are handled by just adjusting the strides
	     and dimensions of the object.
	*/
	     
	if (mit->view) return;

	/* no subspace iteration needed.  Return */
	if (subnd == 0) {
		n = arr->nd;
		for (i=0; i<n; i++) {
			mit->iteraxes[i] = i;
		}
		goto finish;
	}

	/* all indexing arrays have been converted to 0 
	   therefore we can extract the subspace with a simple
	   getitem call which will use view semantics
	*/
	
	sub = PyObject_GetItem((PyObject *)arr, mit->indexobj);
	if (sub == NULL) goto fail;
	mit->subspace = (PyArrayIterObject *)PyArray_IterNew(sub);
	if (mit->subspace == NULL) goto fail;
	Py_DECREF(sub);

	/* Expand dimensions of result */
	n = mit->subspace->ao->nd;
	for (i=0; i<n; i++) 
		mit->dimensions[mit->nd+i] = mit->subspace->ao->dimensions[i];
	mit->nd += n;

	/* Now, we still need to interpret the ellipsis and slice objects 
	   to determine which axes the indexing arrays are referring to
	*/
	n = PyTuple_GET_SIZE(mit->indexobj);

	/* The number of dimensions an ellipsis takes up */
	ellipexp = arr->nd - n + 1;
	/* Now fill in iteraxes -- remember indexing arrays have been 
           converted to 0's in mit->indexobj */
	curraxis = 0;
	j = 0;
	noellip = 1;  /* Only expand the first ellipsis */
	memset(mit->bscoord, 0, sizeof(intp)*arr->nd);
	for (i=0; i<n; i++) {
		/* We need to fill in the starting coordinates for
		   the subspace */
		obj = PyTuple_GET_ITEM(mit->indexobj, i);
		if (PyInt_Check(obj) || PyLong_Check(obj)) 
			mit->iteraxes[j++] = curraxis++;
		else if (noellip && obj == Py_Ellipsis) {
			curraxis += ellipexp;
			noellip = 0;
		}
		else {
			int start=0;
			int stop, step;
			/* Should be slice object or
			   another Ellipsis */
			if (obj == Py_Ellipsis) {
				mit->bscoord[curraxis] = 0;
			}
			else if (!PySlice_Check(obj) || \
				 (slice_GetIndices((PySliceObject *)obj, 
						   arr->dimensions[curraxis],
						   &start, &stop, &step,
						   &dimsize) < 0)) {
				PyErr_Format(PyExc_ValueError, 
					     "unexpected object "	\
					     "(%s) in selection position %d",
					     obj->ob_type->tp_name, i);
			        goto fail;
			}
			else {
				mit->bscoord[curraxis] = start;
			}
			curraxis += 1;  
		}
	}
 finish:
	/* Here check the indexes (now that we have iteraxes) */
	mit->size = PyArray_MultiplyList(mit->dimensions, mit->nd);
	for (i=0; i<mit->numiter; i++) {
		it = mit->iters[i];
		PyArray_ITER_RESET(it);
		dimsize = arr->dimensions[mit->iteraxes[i]];
		while(it->index < it->size) {
			indptr = ((intp *)it->dataptr);
			if (*indptr < 0) *indptr += dimsize;
			if (*indptr < 0 || *indptr >= dimsize) {
				PyErr_Format(PyExc_IndexError,
					     "index (%d) out of range "\
					     "(0<=index<=%d) in dimension %d",
					     (int) *indptr, (int) (dimsize-1), 
					     mit->iteraxes[i]);
				goto fail;
			}
			PyArray_ITER_NEXT(it);
		}		
		PyArray_ITER_RESET(it);
	}
	return;

 fail:
	Py_XDECREF(sub);
	Py_XDECREF(mit->ait);
	mit->ait = NULL;
	return;
}

/* This function takes a Boolean array and constructs index objects and
   iterators as if nonzero(Bool) had been called
*/
static int
_nonzero_indices(PyObject *myBool, PyArrayIterObject **iters)
{
	PyArray_Typecode typecode = {PyArray_BOOL, 0, 0};
	PyArrayObject *ba =NULL, *new=NULL;
	int nd, j;
	intp size, i, count;
	Bool *ptr;
	intp coords[MAX_DIMS], dims_m1[MAX_DIMS];
	intp *dptr[MAX_DIMS];

	ba = (PyArrayObject *)PyArray_FromAny(myBool, &typecode, 0, 0, 
					      CARRAY_FLAGS);
	if (ba == NULL) return -1;
	nd = ba->nd;
	for (j=0; j<nd; j++) iters[j] = NULL;
	size = PyArray_SIZE(ba);
	ptr = (Bool *)ba->data;
	count = 0;

	/* pre-determine how many nonzero entries there are */
	for (i=0; i<size; i++) 
		if (*(ptr++)) count++;

	/* create count-sized index arrays for each dimension */
	for (j=0; j<nd; j++) {
		new = (PyArrayObject *)PyArray_New(&PyArray_Type, 1, &count, 
						   PyArray_INTP, NULL, NULL, 
						   0, 0, NULL);
		if (new == NULL) goto fail;
		iters[j] = (PyArrayIterObject *)	\
			PyArray_IterNew((PyObject *)new);
		Py_DECREF(new);
		if (iters[j] == NULL) goto fail;
		dptr[j] = (intp *)iters[j]->ao->data;
		coords[j] = 0;
		dims_m1[j] = ba->dimensions[j]-1;
	}

	ptr = (Bool *)ba->data;

	if (count == 0) return nd;

	/* Loop through the Boolean array  and copy coordinates
	   for non-zero entries */
	for (i=0; i<size; i++) {
		if (*(ptr++)) {
			for (j=0; j<nd; j++) 
				*(dptr[j]++) = coords[j];
		}
		/* Borrowed from ITER_NEXT macro */
		for (j=nd-1; j>=0; j--) {
			if (coords[j] < dims_m1[j]) {
				coords[j]++;
				break;
			}
			else {
				coords[j] = 0;
			}
		}
	}
	return nd;

 fail:
	for (j=0; j<nd; j++) {
		Py_XDECREF(iters[j]);
	}
	Py_XDECREF(ba);
	return -1;

}

static PyObject *
PyArray_MapIterNew(PyObject *indexobj)
{
        PyArrayMapIterObject *mit;
	int fancy=0;
	PyArray_Typecode indtype = {PyArray_INTP, 0, 0};
	PyObject *arr = NULL;
	int i, n, started, nonindex;

        mit = PyObject_GC_New(PyArrayMapIterObject, &PyArrayMapIter_Type);
        if (mit == NULL)
                return NULL;
	for (i=0; i<MAX_DIMS; i++)
		mit->iters[i] = NULL;
 	mit->view = 0;
 	mit->index = 0;
 	mit->ait = NULL;
 	mit->subspace = NULL;
	mit->numiter = 0;
	mit->consec = 1;
	fancy = fancy_indexing_check(indexobj);
	Py_INCREF(indexobj);
	mit->indexobj = indexobj;
	if (fancy == SOBJ_NOTFANCY) { /* bail out */
		mit->view = 1;
		goto ret;
	}

	if (fancy == SOBJ_BADARRAY) {
		PyErr_SetString(PyExc_TypeError,			\
				"arrays used as indices must be of "    \
				"integer type");
		goto fail;
	}
	if (fancy == SOBJ_TOOMANY) {
		PyErr_SetString(PyExc_TypeError, "too many indices");
		goto fail;
	}

	if (fancy == SOBJ_LISTTUP) {
		PyObject *newobj;
		newobj = PySequence_Tuple(indexobj);
		if (newobj == NULL) goto fail;
		Py_DECREF(indexobj);
		indexobj = newobj;
		mit->indexobj = indexobj;
	}

#undef SOBJ_NOTFANCY 
#undef SOBJ_ISFANCY 
#undef SOBJ_BADARRAY 
#undef SOBJ_TOOMANY 
#undef SOBJ_LISTTUP 


	/* Must have some kind of fancy indexing if we are here */
	/* indexobj is either a list, an arrayobject, or a tuple 
	   (with at least 1 list or arrayobject or Bool object), */
	
	/* convert all inputs to iterators */
	if (PyArray_Check(indexobj) &&			\
	    (PyArray_TYPE(indexobj) == PyArray_BOOL)) {
		mit->numiter = _nonzero_indices(indexobj, mit->iters);
		if (mit->numiter < 0) goto fail;
		mit->nd = 1;
		mit->dimensions[0] = mit->iters[0]->dims_m1[0]+1;
		Py_DECREF(mit->indexobj);
		mit->indexobj = PyTuple_New(mit->numiter);
		for (i=0; i<mit->numiter; i++) {
			PyTuple_SET_ITEM(mit->indexobj, i, 
					 PyInt_FromLong(0));
		}
	}

	else if (PyList_Check(indexobj) || PyArray_Check(indexobj)) {
		mit->numiter = 1;
		arr = PyArray_FromAny(indexobj, &indtype, 0, 0, FORCECAST);
		if (arr == NULL) goto fail;
		mit->iters[0] = (PyArrayIterObject *)PyArray_IterNew(arr);
		Py_DECREF(arr);
		if (mit->iters[0] == NULL) goto fail;
		mit->nd = PyArray_NDIM(arr);
		memcpy(mit->dimensions,PyArray_DIMS(arr),mit->nd*sizeof(intp));
		mit->size = PyArray_SIZE(arr);
		Py_DECREF(mit->indexobj);
		mit->indexobj = Py_BuildValue("(N)", PyInt_FromLong(0));
	}
	else { /* must be a tuple */
		PyObject *obj;
		PyArrayIterObject *iter;
		PyObject *new;
		/* Make a copy of the tuple -- we will be replacing 
		    index objects with 0's */
		n = PyTuple_GET_SIZE(indexobj);
		new = PyTuple_New(n);
		if (new == NULL) goto fail;
		Py_DECREF(mit->indexobj);
		mit->indexobj = new;
		started = 0;
		nonindex = 0;
		for (i=0; i<n; i++) {
			obj = PyTuple_GET_ITEM(indexobj,i);
			if (_convert_obj(obj, &iter) < 0)
				goto fail;
			if (iter!= NULL) { 
				started = 1;
				if (nonindex) mit->consec = 0;
				mit->iters[(mit->numiter)++] = iter;
				PyTuple_SET_ITEM(new,i,
						 PyInt_FromLong(0));
			}
			else {
				if (started) nonindex = 1;
				Py_INCREF(obj);
				PyTuple_SET_ITEM(new,i,obj);
			}
		}
		/* Store the number of iterators actually converted */
		/*  These will be mapped to actual axes at bind time */
		if (PyArray_Broadcast((PyArrayMultiIterObject *)mit) < 0)
			goto fail;
	}

 ret:
        PyObject_GC_Track(mit);
        return (PyObject *)mit;
       
 fail:
	Py_XDECREF(arr);
	Py_XDECREF(mit->indexobj);
	for (i=0; i<mit->numiter; i++)
		Py_XDECREF(mit->iters[i]);
        PyObject_GC_Del(mit);
	return NULL;
}


/* return unbound mapiter object */

static PyObject *
arraymapiter_new(PyTypeObject *type, PyObject *args, PyObject *kwds) 
{
	PyObject *newtup, *res;
	int n;

	if (!PyTuple_Check(args)) {
		PyErr_BadInternalCall();
		return NULL;
	}

	n = PyTuple_GET_SIZE(args);
	
	if (n < 1) {
		PyErr_SetString(PyExc_ValueError, 
				"must be initialized with >= 1 argument");
		return NULL;
	}
	
	if (n > 1) {
		newtup = PyTuple_GetSlice(args, 0, n);
		if (newtup == NULL) return NULL;
		res = PyArray_MapIterNew(newtup);
		Py_DECREF(newtup);
	}
	else {  /* n == 1 */
		newtup = PyTuple_GET_ITEM(args, 0);
		res = PyArray_MapIterNew(newtup);
	}

	return res; 	
}


/* Returns a 0-dim array holding the element desired */
/*
static PyObject *
arraymapiter_next(PyArrayMapIterObject *mit)
{
	PyObject *ret;
	if (mit->ait == NULL) return NULL;
	if (mit->view) return NULL;
	if (mit->index < mit->size) {
		ret = PyArray_ToScalar(mit->dataptr, mit->ait->ao);
		PyArray_MapIterNext(mit);
		return ret;
	}
        return NULL;
}
*/

static void
arraymapiter_dealloc(PyArrayMapIterObject *mit)
{
	int i;
        PyObject_GC_UnTrack(mit);
        Py_XDECREF(mit->ait);
	Py_XDECREF(mit->indexobj);
	Py_XDECREF(mit->subspace);
	for (i=0; i<mit->numiter; i++)
		Py_XDECREF(mit->iters[i]);
        PyObject_GC_Del(mit);
}

static int
arraymapiter_traverse(PyArrayMapIterObject *mit, visitproc visit, void *arg)
{
	int ret, i;
        if (mit->ait != NULL)
                if ((ret = visit((PyObject *)(mit->ait), arg)) != 0) 
			return ret;	

	if (mit->subspace != NULL)
		if ((ret = visit((PyObject *)(mit->subspace), arg)) != 0) 
			return ret;

	if (mit->iters != NULL) 
		for (i=0; i<mit->numiter; i++) 
			if (mit->iters[i] != NULL) 
				if ((ret=visit((PyObject *)mit->iters[i], 
					       arg)) != 0)
					return ret;

	if (mit->indexobj != NULL)
		if ((ret = visit(mit->indexobj, arg)) != 0) return ret;

	
        return 0;
}


/* The mapiter object must be created new each time.  It does not work
   to bind to a new array, and continue.

   This was the orginal intention, but currently MapIterNew must be 
   that does not work.  Do not expose the MapIter_Type to Python.

   It's not very useful anyway, since mapiter(indexobj); mapiter.bind(a); 
   mapiter is equivalent to a[indexobj].flat but the latter gets to use 
   slice syntax.
*/
/* 
static char doc_mapiter_bind[] = "obj.bind(a)\n Bind an array to the "\
	"mapiter object";

static PyObject *
mapiter_bind(PyArrayMapIterObject *mit, PyObject *args)
{
	PyArrayObject *arr;

        if (!PyArg_ParseTuple(args, "O!", &PyArray_Type, &arr)) return NULL;

 	PyArray_MapIterBind(mit, arr);

	if (mit->ait == NULL) return NULL;
	
 	PyArray_MapIterReset(mit);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef mapiter_methods[] = {
	{"bind", (PyCFunction)mapiter_bind, 1, doc_mapiter_bind},
        {NULL,		NULL}		
};
*/


static PyTypeObject PyArrayMapIter_Type = {
        PyObject_HEAD_INIT(NULL)
        0,					 /* ob_size */
        "scipy.mapiter",		    	/* tp_name */
        sizeof(PyArrayIterObject),               /* tp_basicsize */
        0,					 /* tp_itemsize */
        /* methods */
        (destructor)arraymapiter_dealloc,	/* tp_dealloc */
        0,					/* tp_print */
        0,					/* tp_getattr */
        0,					/* tp_setattr */
        0,					/* tp_compare */
        0,					/* tp_repr */
        0,					/* tp_as_number */
        0,					/* tp_as_sequence */
        0,					/* tp_as_mapping */
        0,					/* tp_hash */
        0,					/* tp_call */
        0,					/* tp_str */
        0,                       		/* tp_getattro */
        0,					/* tp_setattro */
        0,					/* tp_as_buffer */
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,/* tp_flags */
        0,					/* tp_doc */
        (traverseproc)arraymapiter_traverse,	/* tp_traverse */
        0,					/* tp_clear */
        0,					/* tp_richcompare */
        0,					/* tp_weaklistoffset */
        0,		 	                /* tp_iter */
        (iternextfunc)0, /*arraymapiter_next,*/	/* tp_iternext */
        0,             	                        /* tp_methods */
        0,					  /* tp_members */
        0,			                  /* tp_getset */
        0,					  /* tp_base */
        0,					  /* tp_dict */
        0,					  /* tp_descr_get */
        0,					  /* tp_descr_set */
        0,					  /* tp_dictoffset */
        (initproc)0,	  	                  /* tp_init */
        0,  	                                  /* tp_alloc */
        (newfunc)arraymapiter_new,	          /* tp_new */
        0,	                                  /* tp_free */
        0,					  /* tp_is_gc */
        0,					  /* tp_bases */
        0,					  /* tp_mro */
        0,					  /* tp_cache */
        0,					  /* tp_subclasses */
        0					  /* tp_weaklist */

};

/** END of Subscript Iterator **/



