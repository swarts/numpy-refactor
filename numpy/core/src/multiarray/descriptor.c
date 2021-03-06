/* Array Descr Object */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#define _MULTIARRAYMODULE
#define NPY_NO_PREFIX
#include "numpy/arrayobject.h"
#include "numpy/arrayscalars.h"
#include "numpy/numpy_api.h"
#include "descriptor.h"

#include "npy_config.h"

#include "npy_3kcompat.h"

#include "common.h"

#define _chk_byteorder(arg) (arg == '>' || arg == '<' ||        \
                             arg == '|' || arg == '=')

static PyObject *typeDict = NULL;   /* Must be explicitly loaded */

static PyArray_Descr *
_use_inherit(PyArray_Descr *type, PyObject *newobj, int *errflag);

NPY_NO_EXPORT PyArray_Descr *
_arraydescr_fromobj(PyObject *obj)
{
    PyObject *dtypedescr;
    PyArray_Descr *new;
    int ret;

    dtypedescr = PyObject_GetAttrString(obj, "dtype");
    PyErr_Clear();
    if (dtypedescr) {
        ret = PyArray_DescrConverter(dtypedescr, &new);
        Py_DECREF(dtypedescr);
        if (ret == PY_SUCCEED) {
            return new;
        }
        PyErr_Clear();
    }
    /* Understand basic ctypes */
    dtypedescr = PyObject_GetAttrString(obj, "_type_");
    PyErr_Clear();
    if (dtypedescr) {
        ret = PyArray_DescrConverter(dtypedescr, &new);
        Py_DECREF(dtypedescr);
        if (ret == PY_SUCCEED) {
            PyObject *length;
            length = PyObject_GetAttrString(obj, "_length_");
            PyErr_Clear();
            if (length) {
                /* derived type */
                PyObject *newtup;
                PyArray_Descr *derived;
                newtup = Py_BuildValue("NO", new, length);
                Py_DECREF(length);
                ret = PyArray_DescrConverter(newtup, &derived);
                Py_DECREF(newtup);
                if (ret == PY_SUCCEED) {
                    return derived;
                }
                PyErr_Clear();
                return NULL;
            }
            return new;
        }
        PyErr_Clear();
        return NULL;
    }
    /* Understand ctypes structures --
       bit-fields are not supported
       automatically aligns */
    dtypedescr = PyObject_GetAttrString(obj, "_fields_");
    PyErr_Clear();
    if (dtypedescr) {
        ret = PyArray_DescrAlignConverter(dtypedescr, &new);
        Py_DECREF(dtypedescr);
        if (ret == PY_SUCCEED) {
            return new;
        }
        PyErr_Clear();
    }
    return NULL;
}

NPY_NO_EXPORT PyObject *
array_set_typeDict(PyObject *NPY_UNUSED(ignored), PyObject *args)
{
    PyObject *dict;

    if (!PyArg_ParseTuple(args, "O", &dict)) {
        return NULL;
    }
    /* Decrement old reference (if any)*/
    Py_XDECREF(typeDict);
    typeDict = dict;

    Py_INCREF(Py_None);
    return Py_None;
}

static int
_check_for_commastring(char *type, int len)
{
    int i;

    /* Check for ints at start of string */
    if ((type[0] >= '0'
                && type[0] <= '9')
            || ((len > 1)
                && _chk_byteorder(type[0])
                && (type[1] >= '0'
                && type[1] <= '9'))) {
        return 1;
    }
    /* Check for empty tuple */
    if (((len > 1)
                && (type[0] == '('
                && type[1] == ')'))
            || ((len > 3)
                && _chk_byteorder(type[0])
                && (type[1] == '('
                && type[2] == ')'))) {
        return 1;
    }
    /* Check for presence of commas */
    for (i = 1; i < len; i++) {
        if (type[i] == ',') {
            return 1;
        }
    }
    return 0;
}

static int
_check_for_datetime(char *type, int len)
{
    if (len < 1) {
        return 0;
    }
    if (type[1] == '8' && (type[0] == 'M' || type[0] == 'm')) {
        return 1;
    }
    if (len < 10) {
        return 0;
    }
    if (strncmp(type, "datetime64", 10) == 0) {
        return 1;
    }
    if (len < 11) {
        return 0;
    }
    if (strncmp(type, "timedelta64", 11) == 0) {
        return 1;
    }
    return 0;
}



#undef _chk_byteorder

static PyArray_Descr *
_convert_from_tuple(PyObject *obj)
{
    PyArray_Descr *type, *res;
    PyObject *val;
    int errflag;

    if (PyTuple_GET_SIZE(obj) != 2) {
        return NULL;
    }
    if (!PyArray_DescrConverter(PyTuple_GET_ITEM(obj,0), &type)) {
        return NULL;
    }
    val = PyTuple_GET_ITEM(obj,1);
    /* try to interpret next item as a type */
    res = _use_inherit(type, val, &errflag);
    if (res || errflag) {
        Py_DECREF(type);
        if (res) {
            return res;
        }
        else {
            return NULL;
        }
    }
    PyErr_Clear();
    /*
     * We get here if res was NULL but errflag wasn't set
     * --- i.e. the conversion to a data-descr failed in _use_inherit
     */
    if (type->elsize == 0) {
        /* interpret next item as a typesize */
        int itemsize = PyArray_PyIntAsInt(PyTuple_GET_ITEM(obj,1));

        if (error_converting(itemsize)) {
            PyErr_SetString(PyExc_ValueError,
                    "invalid itemsize in generic type tuple");
            goto fail;
        }
        PyArray_DESCR_REPLACE(type);
        if (type->type_num == PyArray_UNICODE) {
            type->elsize = itemsize << 2;
        }
        else {
            type->elsize = itemsize;
        }
    }
    else if (PyDict_Check(val)) {
        /* Assume it's a metadata dictionary */
        if (PyDict_Merge(type->metadata, val, 0) == -1) {
            Py_DECREF(type);
            return NULL;
        }
    }
    else {
        /*
         * interpret next item as shape (if it's a tuple)
         * and reset the type to PyArray_VOID with
         * a new fields attribute.
         */
        PyArray_Dims shape = {NULL, -1};
        PyArray_Descr *newdescr;

        if (!(PyArray_IntpConverter(val, &shape)) || (shape.len > MAX_DIMS)) {
            PyDimMem_FREE(shape.ptr);
            PyErr_SetString(PyExc_ValueError,
                    "invalid shape in fixed-type tuple.");
            goto fail;
        }
        /*
         * If (type, 1) was given, it is equivalent to type...
         * or (type, ()) was given it is equivalent to type...
         */
        if ((shape.len == 1
                    && shape.ptr[0] == 1
                    && PyNumber_Check(val))
                || (shape.len == 0
                    && PyTuple_Check(val))) {
            PyDimMem_FREE(shape.ptr);
            return type;
        }
        newdescr = PyArray_DescrNewFromType(PyArray_VOID);
        if (newdescr == NULL) {
            PyDimMem_FREE(shape.ptr);
            goto fail;
        }
        newdescr->elsize = type->elsize;
        newdescr->elsize *= PyArray_MultiplyList(shape.ptr, shape.len);
        newdescr->subarray = NpyArray_malloc(sizeof(PyArray_ArrayDescr));
        newdescr->subarray->base = type;
        newdescr->subarray->shape_num_dims = shape.len;
        newdescr->subarray->shape_dims = shape.ptr;
        shape.ptr = NULL; /* Stole shape.ptr, do not free. */
        newdescr->flags = type->flags;
        NpyArray_DescrDeallocNamesAndFields(newdescr);
        type = newdescr;
        assert(0 == newdescr->subarray->shape_num_dims && NULL == newdescr->subarray->shape_dims || 
               0 < newdescr->subarray->shape_num_dims && NULL != newdescr->subarray->shape_dims);
    }
    return type;

 fail:
    Py_XDECREF(type);
    return NULL;
}

/*
 * obj is a list.  Each item is a tuple with
 *
 * (field-name, data-type (either a list or a string), and an optional
 * shape parameter).
 *
 * field-name can be a string or a 2-tuple
 * data-type can now be a list, string, or 2-tuple (string, metadata dictionary))
 */

static PyArray_Descr *
_convert_from_array_descr(PyObject *obj, int align)
{
    int n, i, totalsize;
    int ret;
    PyObject *item, *newobj;
    PyObject *name, *title;
    NpyDict *fields = NULL;
    char **nameslist = NULL;
    int offset = 0;
    PyArray_Descr *new;
    PyArray_Descr *conv;
    int dtypeflags = 0;
    int maxalign = 0;

    n = PyList_GET_SIZE(obj);
    totalsize = 0;
    
    nameslist = NpyArray_DescrAllocNames(n);
    if (NULL == nameslist) {
        return NULL;
    }
    fields = NpyArray_DescrAllocFields();
    if (NULL == fields) {
        free(nameslist);
        return NULL;
    }
    
    for (i = 0; i < n; i++) {
        item = PyList_GET_ITEM(obj, i);
        if (!PyTuple_Check(item) || (PyTuple_GET_SIZE(item) < 2)) {
            goto fail;
        }
        name = PyTuple_GET_ITEM(item, 0);
        if (PyUString_Check(name)) {
            title = NULL;
        }
        else if (PyTuple_Check(name)) {
            if (PyTuple_GET_SIZE(name) != 2) {
                goto fail;
            }
            title = PyTuple_GET_ITEM(name, 0);
            name = PyTuple_GET_ITEM(name, 1);
            if (!PyUString_Check(name)) {
                goto fail;
            }
        }
        else {
            goto fail;
        }

        /* Insert name into nameslist */
        Py_INCREF(name);  

        if (PyUString_GET_SIZE(name) == 0) {
            Py_DECREF(name);
            if (title == NULL) {
                name = PyUString_FromFormat("f%d", i);
            }
#if defined(NPY_PY3K)
            /* On Py3, allow only non-empty Unicode strings as field names */
            else if (PyUString_Check(title) && PyUString_GET_SIZE(title) > 0) {
                name = title;
                Py_INCREF(name);
            }
            else {
                goto fail;
            }
#else
            else {
                name = title;
                Py_INCREF(name);
            }
#endif
        }
        nameslist[i] = strdup(PyString_AsString(name));
        Py_DECREF(name);

        /* Process rest */

        if (PyTuple_GET_SIZE(item) == 2) {
            ret = PyArray_DescrConverter(PyTuple_GET_ITEM(item, 1), &conv);
            if (ret == PY_FAIL) {
                PyObject_Print(PyTuple_GET_ITEM(item, 1), stderr, 0);
            }
        }
        else if (PyTuple_GET_SIZE(item) == 3) {
            newobj = PyTuple_GetSlice(item, 1, 3);
            ret = PyArray_DescrConverter(newobj, &conv);
            Py_DECREF(newobj);
        }
        else {
            goto fail;
        }
        if (ret == PY_FAIL) {
            goto fail;
        }
        
        if (NpyDict_ContainsKey(fields, nameslist[i])
            || (title
#if defined(NPY_PY3K)
                 && PyUString_Check(title)
#else
                 && (PyUString_Check(title) || PyUnicode_Check(title))
#endif
                 && NpyDict_ContainsKey(fields, (void *)PyString_AsString(title)))) {
            PyErr_SetString(PyExc_ValueError,
                    "two fields with the same name");
            Py_DECREF(conv);
            goto fail;
        }
        dtypeflags |= (conv->flags & NPY_FROM_FIELDS);
                 
        
        if (align) {
            int _align;

            _align = conv->alignment;
            if (_align > 1) {
                totalsize = ((totalsize + _align - 1)/_align)*_align;
            }
            maxalign = MAX(maxalign, _align);
        }
        offset = totalsize;

        /*
         * Title can be "meta-data".  Only insert it
         * into the fields dictionary if it is a string
         * and if it is not the same as the name.
         */
        if (title != NULL) {
#if defined(NPY_PY3K)
            if (PyUString_Check(title)) 
#else
            if (PyUString_Check(title) || PyUnicode_Check(title)) 
#endif
            {
                char *titleStr = PyString_AsString(title);
                if (!strcmp(nameslist[i], titleStr) || NpyDict_ContainsKey(fields, titleStr)) {
                    PyErr_SetString(PyExc_ValueError,
                            "title already used as a name or title.");
                    Py_DECREF(conv);
                    goto fail;
                }
                NpyArray_DescrSetField(fields, nameslist[i], conv, offset, titleStr);
                Npy_INCREF(conv);       /* First DescrSetField call steals the reference, need a second to steal. */
                NpyArray_DescrSetField(fields, titleStr, conv, offset, titleStr);
            }
        } else {
            NpyArray_DescrSetField(fields, nameslist[i], conv, offset, NULL);
        }
        totalsize += conv->elsize;
    }
    new = PyArray_DescrNewFromType(PyArray_VOID);
    new->fields = fields;
    new->names = nameslist;
    new->elsize = totalsize;
    new->flags=dtypeflags;
    if (maxalign > 1) {
        totalsize = ((totalsize + maxalign - 1)/maxalign)*maxalign;
    }
    if (align) {
        new->alignment = maxalign;
    }
    return new;

 fail:
    if (NULL != nameslist) {
        for (i=0; i < n; i++) {
            if (NULL != nameslist[i]) {
                free(nameslist[i]);
            }
        }
        free(nameslist);
    }
    
    if (NULL != fields) {
        NpyDict_Destroy(fields);   
    }
    return NULL;
}

    
    
/*
 * a list specifying a data-type can just be
 * a list of formats.  The names for the fields
 * will default to f0, f1, f2, and so forth.
 */
static PyArray_Descr *
_convert_from_list(PyObject *obj, int align)
{
    int n, i;
    int totalsize;
    PyArray_Descr *conv = NULL;
    PyArray_Descr *new;
    PyObject *key;
    NpyDict *fields = NULL;
    char **nameslist = NULL;
    int ret;
    int maxalign = 0;
    int dtypeflags = 0;

    n = PyList_GET_SIZE(obj);
    /*
     * Ignore any empty string at end which _internal._commastring
     * can produce
     */
    key = PyList_GET_ITEM(obj, n-1);
    if (PyBytes_Check(key) && PyBytes_GET_SIZE(key) == 0) {
        n = n - 1;
    }
    /* End ignore code.*/
    totalsize = 0;
    if (n == 0) {
        return NULL;
    }
    nameslist = NpyArray_DescrAllocNames(n);
    if (NULL == nameslist) {
        return NULL;
    }
    fields = NpyArray_DescrAllocFields();
    if (NULL == fields) {
        free(nameslist);
        return NULL;
    }
    
    for (i = 0; i < n; i++) {
        key = PyUString_FromFormat("f%d", i);
        ret = PyArray_DescrConverter(PyList_GET_ITEM(obj, i), &conv);
        if (ret == PY_FAIL) {
            Py_DECREF(key);
            goto fail;
        }
        dtypeflags |= (conv->flags & NPY_FROM_FIELDS);
        if (align) {
            int _align;

            _align = conv->alignment;
            if (_align > 1) {
                totalsize = ((totalsize + _align - 1)/_align)*_align;
            }
            maxalign = MAX(maxalign, _align);
        }
        NpyArray_DescrSetField(fields, PyString_AsString(key), conv, totalsize, NULL);
        nameslist[i] = strdup(PyString_AsString(key));
        totalsize += conv->elsize;
        Py_DECREF(key);
    }
    new = PyArray_DescrNewFromType(PyArray_VOID);
    new->fields = fields;
    new->names = nameslist;
    new->flags=dtypeflags;
    if (maxalign > 1) {
        totalsize = ((totalsize+maxalign-1)/maxalign)*maxalign;
    }
    if (align) {
        new->alignment = maxalign;
    }
    new->elsize = totalsize;
    return new;

 fail:
    if (NULL != nameslist) {
        for (i=0; i < n; i++) {
            if (NULL != nameslist[i]) {
                free(nameslist[i]);
            }
        }
        free(nameslist);
    }
    
    if (NULL != fields) {
        NpyDict_Destroy(fields);   
    }
    return NULL;
}

/* Exported as DATETIMEUNITS in multiarraymodule.c */
NPY_NO_EXPORT char *_datetime_strings[] = {
    NPY_STR_Y,
    NPY_STR_M,
    NPY_STR_W,
    NPY_STR_B,
    NPY_STR_D,
    NPY_STR_h,
    NPY_STR_m,
    NPY_STR_s,
    NPY_STR_ms,
    NPY_STR_us,
    NPY_STR_ns,
    NPY_STR_ps,
    NPY_STR_fs,
    NPY_STR_as
};

static NPY_DATETIMEUNIT
 _unit_from_str(char *base)
{
    NPY_DATETIMEUNIT unit;

    if (base == NULL) {
        return NPY_DATETIME_DEFAULTUNIT;
    }

    unit = NPY_FR_Y;
    while (unit < NPY_DATETIME_NUMUNITS) {
        if (strcmp(base, _datetime_strings[unit]) == 0) {
            break;
        }
        unit++;
    }
    if (unit == NPY_DATETIME_NUMUNITS) {
        return NPY_DATETIME_DEFAULTUNIT;
    }

    return unit;
}

static int _multiples_table[16][4] = {
    {12, 52, 365},                            /* NPY_FR_Y */
    {NPY_FR_M, NPY_FR_W, NPY_FR_D},
    {4,  30, 720},                            /* NPY_FR_M */
    {NPY_FR_W, NPY_FR_D, NPY_FR_h},
    {5,  7,  168, 10080},                     /* NPY_FR_W */
    {NPY_FR_B, NPY_FR_D, NPY_FR_h, NPY_FR_m},
    {24, 1440, 86400},                        /* NPY_FR_B */
    {NPY_FR_h, NPY_FR_m, NPY_FR_s},
    {24, 1440, 86400},                        /* NPY_FR_D */
    {NPY_FR_h, NPY_FR_m, NPY_FR_s},
    {60, 3600},                               /* NPY_FR_h */
    {NPY_FR_m, NPY_FR_s},
    {60, 60000},                              /* NPY_FR_m */
    {NPY_FR_s, NPY_FR_ms},
    {1000, 1000000},                          /* >=NPY_FR_s */
    {0, 0}
};


/* Translate divisors into multiples of smaller units */
static int
_convert_divisor_to_multiple(PyArray_DateTimeInfo *dtinfo) 
{
    int i, num, ind;
    int *totry;
    NPY_DATETIMEUNIT *baseunit;
    int q, r;
    
    ind = ((int)dtinfo->base - (int)NPY_FR_Y)*2;
    totry = _multiples_table[ind];
    baseunit = (NPY_DATETIMEUNIT *)_multiples_table[ind + 1];

    num = 3;
    if (dtinfo->base == NPY_FR_W) {
        num = 4;
    }
    else if (dtinfo->base > NPY_FR_D) {
        num = 2;
    }
    if (dtinfo->base >= NPY_FR_s) {
        ind = ((int)NPY_FR_s - (int)NPY_FR_Y)*2;
        totry = _multiples_table[ind];
        baseunit = (NPY_DATETIMEUNIT *)_multiples_table[ind + 1];
        baseunit[0] = dtinfo->base + 1;
        baseunit[1] = dtinfo->base + 2;
        if (dtinfo->base == NPY_DATETIME_NUMUNITS - 2) {
            num = 1;
        }
        if (dtinfo->base == NPY_DATETIME_NUMUNITS - 1) {
            num = 0;
        }
    }

    for (i = 0; i < num; i++) {
        q = totry[i] / dtinfo->den;
        r = totry[i] % dtinfo->den;
        if (r == 0) {
            break;
        }
    }
    if (i == num) {
        PyErr_Format(PyExc_ValueError,
                "divisor (%d) is not a multiple of a lower-unit", dtinfo->den);
        return -1;
    }
    dtinfo->base = baseunit[i];
    dtinfo->den = 1;
    dtinfo->num *= q;

    return 0;
}


static PyObject * 
_get_datetime_tuple_from_datetimeinfo(PyArray_DateTimeInfo *dt_data) 
{
    PyObject *dt_tuple; 

    dt_tuple = PyTuple_New(4);

    PyTuple_SET_ITEM(dt_tuple, 0,
            PyBytes_FromString(_datetime_strings[dt_data->base]));
    PyTuple_SET_ITEM(dt_tuple, 1,
            PyInt_FromLong(dt_data->num));
    PyTuple_SET_ITEM(dt_tuple, 2,
            PyInt_FromLong(dt_data->den));
    PyTuple_SET_ITEM(dt_tuple, 3,
            PyInt_FromLong(dt_data->events));

    return dt_tuple;
}



static PyArray_DateTimeInfo *
_convert_datetime_tuple_to_datetimeinfo(PyObject *tuple) 
{
    PyArray_DateTimeInfo *dt_data; 
    PyObject *ret;

    dt_data = _pya_malloc(sizeof(PyArray_DateTimeInfo));
    dt_data->base = _unit_from_str(
            PyBytes_AsString(PyTuple_GET_ITEM(tuple, 0)));

    /* Assumes other objects are Python integers */
    dt_data->num = PyInt_AS_LONG(PyTuple_GET_ITEM(tuple, 1));
    dt_data->den = PyInt_AS_LONG(PyTuple_GET_ITEM(tuple, 2));
    dt_data->events = PyInt_AS_LONG(PyTuple_GET_ITEM(tuple, 3));

    if (dt_data->den > 1) {
        if (_convert_divisor_to_multiple(dt_data) < 0) {
            _pya_free(dt_data);
            return NULL;
        }
    }

    return dt_data;
}

static PyArray_Descr *
_convert_from_datetime_tuple(PyObject *obj)
{
    PyArray_Descr *new;
    PyObject *dt_tuple;
    PyArray_DateTimeInfo *dtinfo;
    PyObject *datetime;

    if (!PyTuple_Check(obj) || PyTuple_GET_SIZE(obj)!=2) {
        PyErr_SetString(PyExc_RuntimeError,
                "_datetimestring is not returning a tuple with length 2");
        return NULL;
    }

    dt_tuple = PyTuple_GET_ITEM(obj, 0);
    datetime = PyTuple_GET_ITEM(obj, 1);
    if (!PyTuple_Check(dt_tuple)
	  || PyTuple_GET_SIZE(dt_tuple) != 4
	  || !PyInt_Check(datetime)) {
      PyErr_SetString(PyExc_RuntimeError,
		      "_datetimestring is not returning a length 4 tuple" \
		      " and an integer");
      return NULL;
    }

    /* Create new timedelta or datetime dtype */ 
    if (PyObject_IsTrue(datetime)) {
        new = PyArray_DescrNewFromType(PyArray_DATETIME);
    }
    else {
        new = PyArray_DescrNewFromType(PyArray_TIMEDELTA);
    }

    if (new == NULL) {
        return NULL;
    }

    dtinfo = _convert_datetime_tuple_to_datetimeinfo(dt_tuple);

    if (dtinfo == NULL) {
        /* Failure in conversion */
        Py_DECREF(new);
        return NULL;
    }


    new->dtinfo = dtinfo;

    return new;
}


static PyArray_Descr *
_convert_from_datetime(PyObject *obj)
{
    PyObject *tupleobj;
    PyArray_Descr *res;
    PyObject *_numpy_internal;

    if (!PyBytes_Check(obj)) {
        return NULL;
    }
    _numpy_internal = PyImport_ImportModule("numpy.core._internal");
    if (_numpy_internal == NULL) {
        return NULL;
    }
    tupleobj = PyObject_CallMethod(_numpy_internal,
            "_datetimestring", "O", obj);
    Py_DECREF(_numpy_internal);
    if (!tupleobj) {
        return NULL;
    }
    /*
     * tuple of a standard tuple (baseunit, num, den, events) and a timedelta
     * boolean
     */
    res = _convert_from_datetime_tuple(tupleobj);
    Py_DECREF(tupleobj);
    if (!res && !PyErr_Occurred()) {
        PyErr_SetString(PyExc_ValueError,
                "invalid data-type");
        return NULL;
    }
    return res;
}


/*
 * comma-separated string
 * this is the format developed by the numarray records module and implemented
 * by the format parser in that module this is an alternative implementation
 * found in the _internal.py file patterned after that one -- the approach is
 * to try to convert to a list (with tuples if any repeat information is
 * present) and then call the _convert_from_list)
 */
static PyArray_Descr *
_convert_from_commastring(PyObject *obj, int align)
{
    PyObject *listobj;
    PyArray_Descr *res;
    PyObject *_numpy_internal;

    if (!PyBytes_Check(obj)) {
        return NULL;
    }
    _numpy_internal = PyImport_ImportModule("numpy.core._internal");
    if (_numpy_internal == NULL) {
        return NULL;
    }
    listobj = PyObject_CallMethod(_numpy_internal, "_commastring", "O", obj);
    Py_DECREF(_numpy_internal);
    if (!listobj) {
        return NULL;
    }
    if (!PyList_Check(listobj) || PyList_GET_SIZE(listobj) < 1) {
        PyErr_SetString(PyExc_RuntimeError,
                "_commastring is not returning a list with len >= 1");
        Py_DECREF(listobj);
        return NULL;
    }
    if (PyList_GET_SIZE(listobj) == 1) {
        if (PyArray_DescrConverter(
                    PyList_GET_ITEM(listobj, 0), &res) == NPY_FAIL) {
            res = NULL;
        }
    }
    else {
        res = _convert_from_list(listobj, align);
    }
    Py_DECREF(listobj);
    if (!res && !PyErr_Occurred()) {
        PyErr_SetString(PyExc_ValueError,
                "invalid data-type");
        return NULL;
    }
    return res;
}

static int
_is_tuple_of_integers(PyObject *obj)
{
    int i;

    if (!PyTuple_Check(obj)) {
        return 0;
    }
    for (i = 0; i < PyTuple_GET_SIZE(obj); i++) {
        if (!PyArray_IsIntegerScalar(PyTuple_GET_ITEM(obj, i))) {
            return 0;
        }
    }
    return 1;
}

/*
 * A tuple type would be either (generic typeobject, typesize)
 * or (fixed-length data-type, shape)
 *
 * or (inheriting data-type, new-data-type)
 * The new data-type must have the same itemsize as the inheriting data-type
 * unless the latter is 0
 *
 * Thus (int32, {'real':(int16,0),'imag',(int16,2)})
 *
 * is one way to specify a descriptor that will give
 * a['real'] and a['imag'] to an int32 array.
 *
 * leave type reference alone
 */
static PyArray_Descr *
_use_inherit(PyArray_Descr *type, PyObject *newobj, int *errflag)
{
    PyArray_Descr *new;
    PyArray_Descr *conv;

    *errflag = 0;
    if (PyArray_IsScalar(newobj, Integer)
            || _is_tuple_of_integers(newobj)
            || !PyArray_DescrConverter(newobj, &conv)) {
        return NULL;
    }
    *errflag = 1;
    new = PyArray_DescrNew(type);
    if (new == NULL) {
        goto fail;
    }
    if (new->elsize && new->elsize != conv->elsize) {
        PyErr_SetString(PyExc_ValueError,
                "mismatch in size of old and new data-descriptor");
        Py_DECREF(new);
        goto fail;
    }
    new->elsize = conv->elsize;
    if (NULL != conv->names) {
        new->names = NpyArray_DescrNamesCopy(conv->names);
        new->fields = NpyArray_DescrFieldsCopy(conv->fields);
    }
    new->flags = conv->flags;
    Py_DECREF(conv);
    *errflag = 0;
    return new;

 fail:
    Py_DECREF(conv);
    return NULL;
}



/*
 * a dictionary specifying a data-type
 * must have at least two and up to four
 * keys These must all be sequences of the same length.
 *
 * can also have an additional key called "metadata" which can be any dictionary
 *
 * "names" --- field names
 * "formats" --- the data-type descriptors for the field.
 *
 * Optional:
 *
 * "offsets" --- integers indicating the offset into the
 * record of the start of the field.
 * if not given, then "consecutive offsets"
 * will be assumed and placed in the dictionary.
 *
 * "titles" --- Allows the use of an additional key
 * for the fields dictionary.(if these are strings
 * or unicode objects) or
 * this can also be meta-data to
 * be passed around with the field description.
 *
 * Attribute-lookup-based field names merely has to query the fields
 * dictionary of the data-descriptor.  Any result present can be used
 * to return the correct field.
 *
 * So, the notion of what is a name and what is a title is really quite
 * arbitrary.
 *
 * What does distinguish a title, however, is that if it is not None,
 * it will be placed at the end of the tuple inserted into the
 * fields dictionary.and can therefore be used to carry meta-data around.
 *
 * If the dictionary does not have "names" and "formats" entries,
 * then it will be checked for conformity and used directly.
 */
static PyArray_Descr *
_use_fields_dict(PyObject *obj, int align)
{
    PyObject *_numpy_internal;
    PyArray_Descr *res;

    _numpy_internal = PyImport_ImportModule("numpy.core._internal");
    if (_numpy_internal == NULL) {
        return NULL;
    }
    res = (PyArray_Descr *)PyObject_CallMethod(_numpy_internal,
            "_usefields", "Oi", obj, align);
    Py_DECREF(_numpy_internal);
    return res;
}

static PyArray_Descr *
_convert_from_dict(PyObject *obj, int align)
{
    PyArray_Descr *new;
    NpyDict *fields = NULL;
    char **nameslist = NULL;
    PyObject *names, *offsets, *descrs, *titles;
    int n, i;
    int totalsize;
    int maxalign = 0;
    int dtypeflags = 0;

    names = PyDict_GetItemString(obj, "names");
    descrs = PyDict_GetItemString(obj, "formats");
    if (!names || !descrs) {
        return _use_fields_dict(obj, align);
    }
    n = PyObject_Length(names);

    fields = NpyArray_DescrAllocFields();
    if (fields == NULL) {
        return (PyArray_Descr *)PyErr_NoMemory();
    }
    nameslist = NpyArray_DescrAllocNames(n);
    if (NULL == nameslist) {
        NpyDict_Destroy(fields);
        return NULL;
    }
    
    offsets = PyDict_GetItemString(obj, "offsets");
    titles = PyDict_GetItemString(obj, "titles");
    if ((n > PyObject_Length(descrs))
        || (offsets && (n > PyObject_Length(offsets)))
        || (titles && (n > PyObject_Length(titles)))) {
        PyErr_SetString(PyExc_ValueError,
                "all items in the dictionary must have the same length.");
        goto fail;
    }

    totalsize = 0;
    for (i = 0; i < n; i++) {
        PyObject *descr, *index, *item, *name, *off;
        long offset = 0;
        int len, ret, _align = 1;
        PyArray_Descr *newdescr;

        /* Build item to insert (descr, offset, [title])*/
        len = 2;
        item = NULL;
        index = PyInt_FromLong(i);
        if (titles) {
            item=PyObject_GetItem(titles, index);
            if (item && item != Py_None) {
                len = 3;
            }
            else {
                Py_XDECREF(item);
            }
            PyErr_Clear();
        }

        descr = PyObject_GetItem(descrs, index);
        if (!descr) {
            goto fail;
        }
        ret = PyArray_DescrConverter(descr, &newdescr);
        Py_DECREF(descr);
        if (ret == PY_FAIL) {
            Py_DECREF(index);
            goto fail;
        }
        if (align) {
            _align = newdescr->alignment;
            maxalign = MAX(maxalign,_align);
        }
        if (offsets) {
            off = PyObject_GetItem(offsets, index);
            if (!off) {
                goto fail;
            }
            offset = PyInt_AsLong(off);
            if (offset < totalsize) {
                PyErr_SetString(PyExc_ValueError,
                        "invalid offset (must be ordered)");
                ret = PY_FAIL;
            }
            if (offset > totalsize) {
                totalsize = offset;
            }
        }
        else {
            if (align && _align > 1) {
                totalsize = ((totalsize + _align - 1)/_align)*_align;
            }
            offset = totalsize;
        }
        name = PyObject_GetItem(names, index);
        if (!name) {
            goto fail;
        }
        Py_DECREF(index);
#if defined(NPY_PY3K)
        if (!PyUString_Check(name)) 
#else
        if (!(PyUString_Check(name) || PyUnicode_Check(name))) 
#endif
        {
            PyErr_SetString(PyExc_ValueError,
                    "field names must be strings");
            ret = PY_FAIL;
        } else {
            nameslist[i] = strdup(PyString_AsString(name));
        }

        /* Insert into dictionary */
        if (NpyDict_ContainsKey(fields, PyString_AsString(name))) {
            PyErr_SetString(PyExc_ValueError,
                    "name already used as a name or title");
            ret = PY_FAIL;
        }
        NpyArray_DescrSetField(fields, PyString_AsString(name), newdescr, offset, (3 == len) ? PyString_AsString(item) : NULL);
        Py_DECREF(name);
        if (len == 3) {
#if defined(NPY_PY3K)
            if (PyUString_Check(item)) 
#else
            if (PyUString_Check(item) || PyUnicode_Check(item)) 
#endif
            {
                if (NpyDict_ContainsKey(fields, PyString_AsString(item))) {
                    PyErr_SetString(PyExc_ValueError,
                            "title already used as a name or title.");
                    ret=PY_FAIL;
                }
                Npy_INCREF(newdescr);           /* First DescrSetField stole the ref, need a second */
                NpyArray_DescrSetField(fields, PyString_AsString(item), newdescr, offset, PyString_AsString(item));
            }
            Py_DECREF(item);
        }
        if ((ret == PY_FAIL) || (newdescr->elsize == 0)) {
            goto fail;
        }
        dtypeflags |= (newdescr->flags & NPY_FROM_FIELDS);
        totalsize += newdescr->elsize;
    }

    new = PyArray_DescrNewFromType(PyArray_VOID);
    if (new == NULL) {
        goto fail;
    }
    if (maxalign > 1) {
        totalsize = ((totalsize + maxalign - 1)/maxalign)*maxalign;
    }
    if (align) {
        new->alignment = maxalign;
    }
    new->elsize = totalsize;
    new->names = nameslist;
    new->fields = fields;
    new->flags = dtypeflags;

    return new;

 fail:
    if (NULL != nameslist) {
        for (i=0; i < n; i++) {
            if (NULL != nameslist[i]) {
                free(nameslist[i]);
            }
        }
        free(nameslist);
    }
    
    if (NULL != fields) {
        NpyDict_Destroy(fields);   
    }
    return NULL;
}


/*NUMPY_API*/
NPY_NO_EXPORT PyArray_Descr *
PyArray_DescrNewFromType(int type_num)
{
    /* TODO: Wrap result in pyobject. */
    return NpyArray_DescrNewFromType(type_num);
}

/*NUMPY_API
 * Get typenum from an object -- None goes to NULL
 */
NPY_NO_EXPORT int
PyArray_DescrConverter2(PyObject *obj, PyArray_Descr **at)
{
    if (obj == Py_None) {
        *at = NULL;
        return PY_SUCCEED;
    }
    else {
        return PyArray_DescrConverter(obj, at);
    }
}

/*NUMPY_API
 * Get typenum from an object -- None goes to PyArray_DEFAULT
 * This function takes a Python object representing a type and converts it
 * to a the correct PyArray_Descr * structure to describe the type.
 *
 * Many objects can be used to represent a data-type which in NumPy is
 * quite a flexible concept.
 *
 * This is the central code that converts Python objects to
 * Type-descriptor objects that are used throughout numpy.
 * new reference in *at
 */
NPY_NO_EXPORT int
PyArray_DescrConverter(PyObject *obj, PyArray_Descr **at)
{
    char *type;
    int check_num = PyArray_NOTYPE + 10;
    int len;
    PyObject *item;
    int elsize = 0;
    char endian = '=';

    *at = NULL;
    /* default */
    if (obj == Py_None) {
        *at = PyArray_DescrFromType(PyArray_DEFAULT);
        return PY_SUCCEED;
    }
    if (PyArray_DescrCheck(obj)) {
        *at = (PyArray_Descr *)obj;
        Py_INCREF(*at);
        return PY_SUCCEED;
    }

    if (PyType_Check(obj)) {
        if (PyType_IsSubtype((PyTypeObject *)obj, &PyGenericArrType_Type)) {
            *at = PyArray_DescrFromTypeObject(obj);
            if (*at) {
                return PY_SUCCEED;
            }
            else {
                return PY_FAIL;
            }
        }
        check_num = PyArray_OBJECT;
#if !defined(NPY_PY3K)
        if (obj == (PyObject *)(&PyInt_Type)) {
            check_num = PyArray_LONG;
        }
        else if (obj == (PyObject *)(&PyLong_Type)) {
            check_num = PyArray_LONGLONG;
        }
#else
        if (obj == (PyObject *)(&PyLong_Type)) {
            check_num = PyArray_LONG;
        }
#endif
        else if (obj == (PyObject *)(&PyFloat_Type)) {
            check_num = PyArray_DOUBLE;
        }
        else if (obj == (PyObject *)(&PyComplex_Type)) {
            check_num = PyArray_CDOUBLE;
        }
        else if (obj == (PyObject *)(&PyBool_Type)) {
            check_num = PyArray_BOOL;
        }
        else if (obj == (PyObject *)(&PyBytes_Type)) {
            check_num = PyArray_STRING;
        }
        else if (obj == (PyObject *)(&PyUnicode_Type)) {
            check_num = PyArray_UNICODE;
        }
#if defined(NPY_PY3K)
        else if (obj == (PyObject *)(&PyMemoryView_Type)) {
            check_num = PyArray_VOID;
        }
#else
        else if (obj == (PyObject *)(&PyBuffer_Type)) {
            check_num = PyArray_VOID;
        }
#endif
        else {
            *at = _arraydescr_fromobj(obj);
            if (*at) {
                return PY_SUCCEED;
            }
        }
        goto finish;
    }

    /* or a typecode string */

    if (PyUnicode_Check(obj)) {
        /* Allow unicode format strings: convert to bytes */
        int retval;
        PyObject *obj2;
        obj2 = PyUnicode_AsASCIIString(obj);
        if (obj2 == NULL) {
            goto fail;
        }
        retval = PyArray_DescrConverter(obj2, at);
        Py_DECREF(obj2);
        return retval;
    }

    if (PyBytes_Check(obj)) {
        /* Check for a string typecode. */
        type = PyBytes_AS_STRING(obj);
        len = PyBytes_GET_SIZE(obj);
        if (len <= 0) {
            goto fail;
        }
        /* check for datetime format */
        if ((len > 1) && _check_for_datetime(type, len)) {
            *at = _convert_from_datetime(obj);
            if (*at) {
                return PY_SUCCEED;
            }
            return PY_FAIL;
        }
        /* check for commas present or first (or second) element a digit */
        if (_check_for_commastring(type, len)) {
            *at = _convert_from_commastring(obj, 0);
            if (*at) {
                return PY_SUCCEED;
            }
            return PY_FAIL;
        }
        check_num = (int) type[0];
        if ((char) check_num == '>'
                || (char) check_num == '<'
                || (char) check_num == '|'
                || (char) check_num == '=') {
            if (len <= 1) {
                goto fail;
            }
            endian = (char) check_num;
            type++; len--;
            check_num = (int) type[0];
            if (endian == '|') {
                endian = '=';
            }
        }
        if (len > 1) {
            elsize = atoi(type + 1);
            if (elsize == 0) {
                check_num = PyArray_NOTYPE+10;
            }
            /*
             * When specifying length of UNICODE
             * the number of characters is given to match
             * the STRING interface.  Each character can be
             * more than one byte and itemsize must be
             * the number of bytes.
             */
            else if (check_num == PyArray_UNICODELTR) {
                elsize <<= 2;
            }
            /* Support for generic processing c4, i4, f8, etc...*/
            else if ((check_num != PyArray_STRINGLTR)
                     && (check_num != PyArray_VOIDLTR)
                     && (check_num != PyArray_STRINGLTR2)) {
                check_num = PyArray_TypestrConvert(elsize, check_num);
                if (check_num == PyArray_NOTYPE) {
                    check_num += 10;
                }
                elsize = 0;
            }
        }
    }
    else if (PyTuple_Check(obj)) {
        /* or a tuple */
        *at = _convert_from_tuple(obj);
        if (*at == NULL){
            if (PyErr_Occurred()) {
                return PY_FAIL;
            }
            goto fail;
        }
        return PY_SUCCEED;
    }
    else if (PyList_Check(obj)) {
        /* or a list */
        *at = _convert_from_array_descr(obj,0);
        if (*at == NULL) {
            if (PyErr_Occurred()) {
                return PY_FAIL;
            }
            goto fail;
        }
        return PY_SUCCEED;
    }
    else if (PyDict_Check(obj)) {
        /* or a dictionary */
        *at = _convert_from_dict(obj,0);
        if (*at == NULL) {
            if (PyErr_Occurred()) {
                return PY_FAIL;
            }
            goto fail;
        }
        return PY_SUCCEED;
    }
    else if (PyArray_Check(obj)) {
        goto fail;
    }
    else {
        *at = _arraydescr_fromobj(obj);
        if (*at) {
            return PY_SUCCEED;
        }
        if (PyErr_Occurred()) {
            return PY_FAIL;
        }
        goto fail;
    }
    if (PyErr_Occurred()) {
        goto fail;
    }
    /* if (check_num == PyArray_NOTYPE) {
           return PY_FAIL;
       }
    */

 finish:
    if ((check_num == PyArray_NOTYPE + 10)
        || (*at = PyArray_DescrFromType(check_num)) == NULL) {
        PyErr_Clear();
        /* Now check to see if the object is registered in typeDict */
        if (typeDict != NULL) {
            item = PyDict_GetItem(typeDict, obj);
#if defined(NPY_PY3K)
            if (!item && PyBytes_Check(obj)) {
                PyObject *tmp;
                tmp = PyUnicode_FromEncodedObject(obj, "ascii", "strict");
                if (tmp != NULL) {
                    item = PyDict_GetItem(typeDict, tmp);
                    Py_DECREF(tmp);
                }
            }
#endif
            if (item) {
                return PyArray_DescrConverter(item, at);
            }
        }
        goto fail;
    }

    if (((*at)->elsize == 0) && (elsize != 0)) {
        PyArray_DESCR_REPLACE(*at);
        (*at)->elsize = elsize;
    }
    if (endian != '=' && PyArray_ISNBO(endian)) {
        endian = '=';
    }
    if (endian != '=' && (*at)->byteorder != '|'
        && (*at)->byteorder != endian) {
        PyArray_DESCR_REPLACE(*at);
        (*at)->byteorder = endian;
    }
    return PY_SUCCEED;

 fail:
    PyErr_SetString(PyExc_TypeError, "data type not understood");
    *at = NULL;
    return PY_FAIL;
}

/** Array Descr Objects for dynamic types **/

/*
 * There are some statically-defined PyArray_Descr objects corresponding
 * to the basic built-in types.
 * These can and should be DECREF'd and INCREF'd as appropriate, anyway.
 * If a mistake is made in reference counting, deallocation on these
 * builtins will be attempted leading to problems.
 *
 * This let's us deal with all PyArray_Descr objects using reference
 * counting (regardless of whether they are statically or dynamically
 * allocated).
 */

/*NUMPY_API
 * base cannot be NULL
 */
NPY_NO_EXPORT PyArray_Descr *
PyArray_DescrNew(PyArray_Descr *base)
{
    return NpyArray_DescrNew(base);           /* TODO: Fix conversion of base->npy descr, wrap result in pyobject. */
}

/*
 * should never be called for builtin-types unless
 * there is a reference-count problem
 */
static void
arraydescr_dealloc(PyArray_Descr *self)
{
    NpyArray_DescrDestroy(self);             /* TODO: Unwrap core descr object */

    Py_TYPE(self)->tp_free((PyObject *)self);
}

/*
 * we need to be careful about setting attributes because these
 * objects are pointed to by arrays that depend on them for interpreting
 * data.  Currently no attributes of data-type objects can be set
 * directly except names.
 */
static PyMemberDef arraydescr_members[] = {
    {"type",
        T_OBJECT, offsetof(PyArray_Descr, typeobj), READONLY, NULL},
    {"kind",
        T_CHAR, offsetof(PyArray_Descr, kind), READONLY, NULL},
    {"char",
        T_CHAR, offsetof(PyArray_Descr, type), READONLY, NULL},
    {"num",
        T_INT, offsetof(PyArray_Descr, type_num), READONLY, NULL},
    {"byteorder",
        T_CHAR, offsetof(PyArray_Descr, byteorder), READONLY, NULL},
    {"itemsize",
        T_INT, offsetof(PyArray_Descr, elsize), READONLY, NULL},
    {"alignment",
        T_INT, offsetof(PyArray_Descr, alignment), READONLY, NULL},
    {"flags",
        T_INT, offsetof(PyArray_Descr, flags), READONLY, NULL},
    {NULL, 0, 0, 0, NULL},
};


static PyObject *
arraydescr_subdescr_get(PyArray_Descr *self)
{
    PyObject *shape = NULL;
    PyObject *ret = NULL;

    if (self->subarray == NULL) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    
    shape = PyArray_IntTupleFromIntp(self->subarray->shape_num_dims, self->subarray->shape_dims);
    ret = Py_BuildValue("OO", (PyObject *)self->subarray->base, shape);
    Py_DECREF(shape);
    return ret;
}



static PyObject *
_append_to_datetime_typestr(PyArray_Descr *self, PyObject *ret)
{
    PyObject *tmp;
    PyObject *res;
    int num, den, events;
    char *basestr;
    PyArray_DateTimeInfo *dt_data;

    /* This shouldn't happen */
    if (self->dtinfo == NULL) {
        return ret;
    }
    dt_data = self->dtinfo;
    num = dt_data->num;
    den = dt_data->den;
    events = dt_data->events;
    basestr = _datetime_strings[dt_data->base];

    if (num == 1) {
        tmp = PyUString_FromString(basestr);
    }
    else {
        tmp = PyUString_FromFormat("%d%s", num, basestr);
    }
    if (den != 1) {
        res = PyUString_FromFormat("/%d", den);
        PyUString_ConcatAndDel(&tmp, res);
    }

    res = PyUString_FromString("[");
    PyUString_ConcatAndDel(&res, tmp);
    PyUString_ConcatAndDel(&res, PyUString_FromString("]"));
    if (events != 1) {
        tmp = PyUString_FromFormat("//%d", events);
        PyUString_ConcatAndDel(&res, tmp);
    }
    PyUString_ConcatAndDel(&ret, res);
    return ret;
}

NPY_NO_EXPORT PyObject *
arraydescr_protocol_typestr_get(PyArray_Descr *self)
{
    char basic_ = self->kind;
    char endian = self->byteorder;
    int size = self->elsize;
    PyObject *ret;

    if (endian == '=') {
        endian = '<';
        if (!PyArray_IsNativeByteOrder(endian)) {
            endian = '>';
        }
    }
    if (self->type_num == PyArray_UNICODE) {
        size >>= 2;
    }

    ret = PyUString_FromFormat("%c%c%d", endian, basic_, size);
    if (PyDataType_ISDATETIME(self)) {
        ret = _append_to_datetime_typestr(self, ret);
    }

    return ret;
}

static PyObject *
arraydescr_typename_get(PyArray_Descr *self)
{
    int len;
    PyTypeObject *typeobj = self->typeobj;
    PyObject *res;
    char *s;
    /* fixme: not reentrant */
    static int prefix_len = 0;

    if (PyTypeNum_ISUSERDEF(self->type_num)) {
        s = strrchr(typeobj->tp_name, '.');
        if (s == NULL) {
            res = PyUString_FromString(typeobj->tp_name);
        }
        else {
            res = PyUString_FromStringAndSize(s + 1, strlen(s) - 1);
        }
        return res;
    }
    else {
        if (prefix_len == 0) {
            prefix_len = strlen("numpy.");
        }
        len = strlen(typeobj->tp_name);
        if (*(typeobj->tp_name + (len-1)) == '_') {
            len -= 1;
        }
        len -= prefix_len;
        res = PyUString_FromStringAndSize(typeobj->tp_name+prefix_len, len);
    }
    if (PyTypeNum_ISFLEXIBLE(self->type_num) && self->elsize != 0) {
        PyObject *p;
        p = PyUString_FromFormat("%d", self->elsize * 8);
        PyUString_ConcatAndDel(&res, p);
    }
    if (PyDataType_ISDATETIME(self)) {
        res = _append_to_datetime_typestr(self, res);
    }

    return res;
}

static PyObject *
arraydescr_base_get(PyArray_Descr *self)
{
    if (self->subarray == NULL) {
        Py_INCREF(self);
        return (PyObject *)self;
    }
    Py_INCREF(self->subarray->base);
    return (PyObject *)(self->subarray->base);
}

static PyObject *
arraydescr_shape_get(PyArray_Descr *self)
{
    if (self->subarray == NULL) {
        return PyTuple_New(0);
    }
    return PyArray_IntTupleFromIntp(self->subarray->shape_num_dims, self->subarray->shape_dims);
}

NPY_NO_EXPORT PyObject *
arraydescr_protocol_descr_get(PyArray_Descr *self)
{
    PyObject *dobj, *res;
    PyObject *_numpy_internal;

    if (self->names == NULL) {
        /* get default */
        dobj = PyTuple_New(2);
        if (dobj == NULL) {
            return NULL;
        }
        PyTuple_SET_ITEM(dobj, 0, PyUString_FromString(""));
        PyTuple_SET_ITEM(dobj, 1, arraydescr_protocol_typestr_get(self));
        res = PyList_New(1);
        if (res == NULL) {
            Py_DECREF(dobj);
            return NULL;
        }
        PyList_SET_ITEM(res, 0, dobj);
        return res;
    }

    _numpy_internal = PyImport_ImportModule("numpy.core._internal");
    if (_numpy_internal == NULL) {
        return NULL;
    }
    res = PyObject_CallMethod(_numpy_internal, "_array_descr", "O", self);
    Py_DECREF(_numpy_internal);
    return res;
}

/*
 * returns 1 for a builtin type
 * and 2 for a user-defined data-type descriptor
 * return 0 if neither (i.e. it's a copy of one)
 */
static PyObject *
arraydescr_isbuiltin_get(PyArray_Descr *self)
{
    long val;
    val = 0;
    if (NULL != self->fields) {
        val = 1;
    }
    if (PyTypeNum_ISUSERDEF(self->type_num)) {
        val = 2;
    }
    return PyInt_FromLong(val);
}

static int
_arraydescr_isnative(PyArray_Descr *self)
{
    if (self->names == NULL) {
        return PyArray_ISNBO(self->byteorder);
    }
    else {
        const char *key;
        NpyArray_DescrField *value;
        NpyDict_Iter pos;

        NpyDict_IterInit(&pos);
        while (NpyDict_IterNext(self->fields, &pos, (void **)&key, (void **)&value)) {
            if (NULL != value->title && !strcmp(value->title, key)) {
                continue;
            }
            if (0 == _arraydescr_isnative(value->descr)) {
                return 0;
            }
        }
    }
    return 1;
}

/*
 * return Py_True if this data-type descriptor
 * has native byteorder if no fields are defined
 *
 * or if all sub-fields have native-byteorder if
 * fields are defined
 */
static PyObject *
arraydescr_isnative_get(PyArray_Descr *self)
{
    PyObject *ret;
    int retval;
    retval = _arraydescr_isnative(self);
    if (retval == -1) {
        return NULL;
    }
    ret = retval ? Py_True : Py_False;
    Py_INCREF(ret);
    return ret;
}

static PyObject *
arraydescr_fields_get(PyArray_Descr *self)
{
    PyObject *dict = NULL;
    NpyDict_Iter pos;
    const char *key;
    NpyArray_DescrField *value;
    
    if (NULL == self->names) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    
    dict = PyDict_New();
    NpyDict_IterInit(&pos);
    while (NpyDict_IterNext(self->fields, &pos, (void **)&key, (void **)&value)) {
        PyObject *tup = PyTuple_New( (NULL == value->title) ? 2 : 3 );
        PyTuple_SET_ITEM(tup, 0, (PyObject *)value->descr);
        Py_INCREF(value->descr);                    /* TODO: Wrap descr object */
        PyTuple_SET_ITEM(tup, 1, PyInt_FromLong(value->offset));
        if (NULL != value->title) {
            PyTuple_SET_ITEM(tup, 2, PyString_FromString(value->title));
        }
        
        PyDict_SetItemString(dict, key, tup);
        Py_DECREF(tup);
    }
    //roDict = PyDictProxy_New(dict);     /* read-only version for consistency with prior versions
    //                                       where dict was an internal data structure. */
    //Py_DECREF(dict);
    return dict;
}



static PyObject *
arraydescr_metadata_get(PyArray_Descr *self)
{
    if (self->metadata == NULL) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    return PyDictProxy_New(self->metadata);
}

static PyObject *
arraydescr_hasobject_get(PyArray_Descr *self)
{
    PyObject *res;
    if (PyDataType_FLAGCHK(self, NPY_ITEM_HASOBJECT)) {
        res = Py_True;
    }
    else {
        res = Py_False;
    }
    Py_INCREF(res);
    return res;
}

static PyObject *
arraydescr_names_get(PyArray_Descr *self)
{
    PyObject *names;
    int i, n;
    
    if (NULL == self->names) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    
    for (n = 0; NULL != self->names[n]; n++) ;
    
    names = PyTuple_New(n);
    for (i = 0; i < n; i++) {
        PyTuple_SET_ITEM(names, i, PyString_FromString(self->names[i]));
    }
    return names;
}



static int
arraydescr_names_set(PyArray_Descr *self, PyObject *val)
{
    int n = 0;
    int i;
    char **nameslist = NULL;
    if (self->names == NULL) {
        PyErr_SetString(PyExc_ValueError,
                "there are no fields defined");
        return -1;
    }

    for (n = 0; NULL != self->names[n]; n++) ;
    if (!PySequence_Check(val) || PyObject_Size((PyObject *)val) != n) {
        PyErr_Format(PyExc_ValueError,
                "must replace all names at once with a sequence of length %d",
                n);
        return -1;
    }
    /* Make sure all entries are strings */
    for (i = 0; i < n; i++) {
        PyObject *item;
        int valid = 1;
        item = PySequence_GetItem(val, i);
        valid = PyUString_Check(item);
        Py_DECREF(item);
        if (!valid) {
            PyErr_Format(PyExc_ValueError,
                    "item #%d of names is of type %s and not string",
                    i, Py_TYPE(item)->tp_name);
            return -1;
        }
    }
    /* Update dictionary keys in fields */
    nameslist = arraydescr_seq_to_nameslist(val);
    NpyArray_DescrReplaceNames(self, nameslist);

    return 0;
}



/* Takes a sequence of strings and returns an array of char**.  Each string is allocated
   and must be free'd eventually. */
NPY_NO_EXPORT char **
arraydescr_seq_to_nameslist(PyObject *seq)
{
    char **nameslist = NULL;
    int n = PySequence_Length(seq);
    int i;
    
    nameslist = NpyArray_DescrAllocNames(n);
    if (NULL != nameslist) {
        for (i = 0; i < n; i++) {
            PyObject *key = PySequence_GetItem(seq, i);
            nameslist[i] = strdup(PyString_AsString(key));
            Py_DECREF(key);
        }
        nameslist[i] = NULL;
    }
    return nameslist;
}



/* Converts a PyDict structure defining a set of PyArray_Descr fields into a
   NpyDict describing the same fields. The PyDict values are 2-tuples or 3-tuples
   containing another descr object, an offset, and an optional title string. */
NPY_NO_EXPORT NpyDict *
arraydescr_fields_from_pydict(PyObject *dict)
{
    NpyDict *fields = NpyArray_DescrAllocFields();
    PyObject *value;
    PyObject *key = NULL;
    NpyArray_Descr *descr;
    int offset;
    const char *title;
    Py_ssize_t pos;
    
    /* Extract dict of tuples of { sub descriptor, offset, [title] } and set as fields on the
       descriptor */
    pos = 0;
    while (PyDict_Next(dict, &pos, &key, &value)) {
        /* TODO: Unwrap descr object. Do we need type checking? */
        descr = (NpyArray_Descr *) PyTuple_GetItem(value, 0);                   
        offset = PyInt_AsLong(PyTuple_GetItem(value, 1));
        title = (2 < PyTuple_Size(value)) ? PyString_AsString(PyTuple_GetItem(value, 2)) : NULL;
        
        /* DescrSetField will incref subDescr, copy strings */
        Npy_INCREF(descr);
        NpyArray_DescrSetField(fields, PyString_AsString(key), descr, offset, title);
    }
    return fields;
}




static PyGetSetDef arraydescr_getsets[] = {
    {"subdtype",
        (getter)arraydescr_subdescr_get,
        NULL, NULL, NULL},
    {"descr",
        (getter)arraydescr_protocol_descr_get,
        NULL, NULL, NULL},
    {"str",
        (getter)arraydescr_protocol_typestr_get,
        NULL, NULL, NULL},
    {"name",
        (getter)arraydescr_typename_get,
        NULL, NULL, NULL},
    {"base",
        (getter)arraydescr_base_get,
        NULL, NULL, NULL},
    {"shape",
        (getter)arraydescr_shape_get,
        NULL, NULL, NULL},
    {"isbuiltin",
        (getter)arraydescr_isbuiltin_get,
        NULL, NULL, NULL},
    {"isnative",
        (getter)arraydescr_isnative_get,
        NULL, NULL, NULL},
    {"fields",
        (getter)arraydescr_fields_get,
        NULL, NULL, NULL},
    {"dtinfo",
        (getter)arraydescr_dtinfo_get,
        NULL, NULL, NULL},
    {"names",
        (getter)arraydescr_names_get,
        (setter)arraydescr_names_set,
        NULL, NULL},
    {"hasobject",
        (getter)arraydescr_hasobject_get,
        NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL},
};



static PyObject *
arraydescr_new(PyTypeObject *NPY_UNUSED(subtype), PyObject *args, PyObject *kwds)
{
    PyObject *odescr=NULL;
    PyArray_Descr *descr, *conv;
    Bool align = FALSE;
    Bool copy = FALSE;
    Bool copied = FALSE;
    static char *kwlist[] = {"dtype", "align", "copy", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O&O&", kwlist,
                &odescr, PyArray_BoolConverter, &align,
		PyArray_BoolConverter, &copy )) {
        return NULL;
    }

    if (align) {
        if (!PyArray_DescrAlignConverter(odescr, &conv)) {
            return NULL;
        }
    }
    else if (!PyArray_DescrConverter(odescr, &conv)) {
        return NULL;
    }
    /* Get a new copy of it unless it's already a copy */
    if (copy && NULL == conv->fields) {
        descr = PyArray_DescrNew(conv);
        Py_DECREF(conv);
        conv = descr;
        copied = TRUE;
    }

    return (PyObject *)conv;
}


/* return a tuple of (callable object, args, state). */
static PyObject *
arraydescr_reduce(PyArray_Descr *self, PyObject *NPY_UNUSED(args))
{
    /*
     * version number of this pickle type. Increment if we need to
     * change the format. Be sure to handle the old versions in
     * arraydescr_setstate.
    */
    const int version = 4;
    PyObject *ret, *mod, *obj;
    PyObject *state;
    char endian;
    int elsize, alignment;

    ret = PyTuple_New(3);
    if (ret == NULL) {
        return NULL;
    }
    mod = PyImport_ImportModule("numpy.core.multiarray");
    if (mod == NULL) {
        Py_DECREF(ret);
        return NULL;
    }
    obj = PyObject_GetAttrString(mod, "dtype");
    Py_DECREF(mod);
    if (obj == NULL) {
        Py_DECREF(ret);
        return NULL;
    }
    PyTuple_SET_ITEM(ret, 0, obj);
    if (PyTypeNum_ISUSERDEF(self->type_num)
            || ((self->type_num == PyArray_VOID
                    && self->typeobj != &PyVoidArrType_Type))) {
        obj = (PyObject *)self->typeobj;
        Py_INCREF(obj);
    }
    else {
        elsize = self->elsize;
        if (self->type_num == PyArray_UNICODE) {
            elsize >>= 2;
        }
        obj = PyUString_FromFormat("%c%d",self->kind, elsize);
    }
    PyTuple_SET_ITEM(ret, 1, Py_BuildValue("(Nii)", obj, 0, 1));

    /*
     * Now return the state which is at least byteorder,
     * subarray, and fields
     */
    endian = self->byteorder;
    if (endian == '=') {
        endian = '<';
        if (!PyArray_IsNativeByteOrder(endian)) {
            endian = '>';
        }
    }

    if (PyDataType_ISDATETIME(self)) {
      /* newobj is a tuple date_time info (str, num, den, events) */
      PyObject *newobj;
      
      state = PyTuple_New(9);
      PyTuple_SET_ITEM(state, 0, PyInt_FromLong(version));

      newobj = _get_datetime_tuple_from_datetimeinfo(self->dtinfo);
      PyTuple_SET_ITEM(state, 8, newobj);
    }
    else { /* Use version 3 pickle format */
        state = PyTuple_New(8);
        PyTuple_SET_ITEM(state, 0, PyInt_FromLong(3));
    }

    PyTuple_SET_ITEM(state, 1, PyUString_FromFormat("%c", endian));
    PyTuple_SET_ITEM(state, 2, arraydescr_subdescr_get(self));
    if (NULL != self->names) {
        PyTuple_SET_ITEM(state, 3, arraydescr_names_get(self));
        PyTuple_SET_ITEM(state, 4, arraydescr_fields_get(self));
    }
    else {
        PyTuple_SET_ITEM(state, 3, Py_None);
        PyTuple_SET_ITEM(state, 4, Py_None);
        Py_INCREF(Py_None);
        Py_INCREF(Py_None);
    }

    /* for extended types it also includes elsize and alignment */
    if (PyTypeNum_ISEXTENDED(self->type_num)) {
        elsize = self->elsize;
        alignment = self->alignment;
    }
    else {
        elsize = -1;
        alignment = -1;
    }
    PyTuple_SET_ITEM(state, 5, PyInt_FromLong(elsize));
    PyTuple_SET_ITEM(state, 6, PyInt_FromLong(alignment));
    PyTuple_SET_ITEM(state, 7, PyInt_FromLong(self->flags));

    PyTuple_SET_ITEM(ret, 2, state);
    return ret;
}

/*
 * returns 1 if this data-type has an object portion
 * used when setting the state because hasobject is not stored.
 */
static int
_descr_find_object(PyArray_Descr *self)
{
    if (self->flags
            || self->type_num == PyArray_OBJECT
            || self->kind == 'O') {
        return NPY_OBJECT_DTYPE_FLAGS;
    }
    if (PyDescr_HASFIELDS(self)) {
        const char *key = NULL;
        NpyArray_DescrField *value;
        NpyDict_Iter pos;
        
        NpyDict_IterInit(&pos);
        while (NpyDict_IterNext(self->fields, &pos, (void **)&key, (void **)&value)) {
            if (NULL != value->title && !strcmp(value->title, key)) {
                continue;
            }
            if (_descr_find_object(value->descr)) {
                value->descr->flags = NPY_OBJECT_DTYPE_FLAGS;
                return NPY_OBJECT_DTYPE_FLAGS;
            }
        }
    }
    return 0;
}

/*
 * state is at least byteorder, subarray, and fields but could include elsize
 * and alignment for EXTENDED arrays
 */
static PyObject *
arraydescr_setstate(PyArray_Descr *self, PyObject *args)
{
    int elsize = -1, alignment = -1;
    int version = 4;
#if defined(NPY_PY3K)
    int endian;
#else
    char endian;
#endif
    PyObject *subarray, *fields, *names = NULL, *dtinfo=NULL;
    int incref_names = 1;
    int dtypeflags = 0;

/*    if (NULL == self->fields) {
        Py_INCREF(Py_None);
        return Py_None;
    } */
    if (PyTuple_GET_SIZE(args) != 1
            || !(PyTuple_Check(PyTuple_GET_ITEM(args, 0)))) {
        PyErr_BadInternalCall();
        return NULL;
    }
    switch (PyTuple_GET_SIZE(PyTuple_GET_ITEM(args,0))) {
    case 9:
#if defined(NPY_PY3K)
#define _ARGSTR_ "(iCOOOiiiO)"
#else
#define _ARGSTR_ "(icOOOiiiO)"
#endif
        if (!PyArg_ParseTuple(args, _ARGSTR_, &version, &endian,
                    &subarray, &names, &fields, &elsize,
                    &alignment, &dtypeflags, &dtinfo)) {
            return NULL;
#undef _ARGSTR_
        }
        break;
    case 8:
#if defined(NPY_PY3K)
#define _ARGSTR_ "(iCOOOiii)"
#else
#define _ARGSTR_ "(icOOOiii)"
#endif
        if (!PyArg_ParseTuple(args, _ARGSTR_, &version, &endian,
                    &subarray, &names, &fields, &elsize,
                    &alignment, &dtypeflags)) {
            return NULL;
#undef _ARGSTR_
        }
        break;
    case 7:
#if defined(NPY_PY3K)
#define _ARGSTR_ "(iCOOOii)"
#else
#define _ARGSTR_ "(icOOOii)"
#endif
        if (!PyArg_ParseTuple(args, _ARGSTR_, &version, &endian,
                    &subarray, &names, &fields, &elsize,
                    &alignment)) {
            return NULL;
#undef _ARGSTR_
        }
        break;
    case 6:
#if defined(NPY_PY3K)
#define _ARGSTR_ "(iCOOii)"
#else
#define _ARGSTR_ "(icOOii)"
#endif
        if (!PyArg_ParseTuple(args, _ARGSTR_, &version,
                    &endian, &subarray, &fields,
                    &elsize, &alignment)) {
            PyErr_Clear();
#undef _ARGSTR_
        }
        break;
    case 5:
        version = 0;
#if defined(NPY_PY3K)
#define _ARGSTR_ "(COOii)"
#else
#define _ARGSTR_ "(cOOii)"
#endif
        if (!PyArg_ParseTuple(args, _ARGSTR_,
                    &endian, &subarray, &fields, &elsize,
                    &alignment)) {
#undef _ARGSTR_
            return NULL;
        }
        break;
    default:
        /* raise an error */
        if (PyTuple_GET_SIZE(PyTuple_GET_ITEM(args,0)) > 5) {
            version = PyInt_AsLong(PyTuple_GET_ITEM(args, 0));
        }
        else {
            version = -1;
        }
    }

    /*
     * If we ever need another pickle format, increment the version
     * number. But we should still be able to handle the old versions.
     */
    if (version < 0 || version > 4) {
        PyErr_Format(PyExc_ValueError,
                     "can't handle version %d of numpy.dtype pickle",
                     version);
        return NULL;
    }

    if (version == 1 || version == 0) {
        if (fields != Py_None) {
            PyObject *key, *list;
            key = PyInt_FromLong(-1);
            list = PyDict_GetItem(fields, key);
            if (!list) {
                return NULL;
            }
            Py_INCREF(list);
            names = list;
            PyDict_DelItem(fields, key);
            incref_names = 0;
        }
        else {
            names = Py_None;
        }
    }


    if ((fields == Py_None && names != Py_None) ||
        (names == Py_None && fields != Py_None)) {
        PyErr_Format(PyExc_ValueError,
                "inconsistent fields and names");
        return NULL;
    }

    if (endian != '|' && PyArray_IsNativeByteOrder(endian)) {
        endian = '=';
    }
    self->byteorder = endian;
    if (self->subarray) {
        NpyArray_DestroySubarray(self->subarray);
    }
    self->subarray = NULL;

    if (subarray != Py_None) {
        PyObject *shape = NULL;
        int len = 0;
        
        self->subarray = NpyArray_malloc(sizeof(NpyArray_ArrayDescr));
        self->subarray->base = (PyArray_Descr *)PyTuple_GET_ITEM(subarray, 0);  /* TODO: Unwrap descr */
        Py_INCREF(self->subarray->base);
        
        shape = PyTuple_GET_ITEM(subarray, 1);
        len = PySequence_Check(shape) ? PySequence_Length(shape) : 1;
        self->subarray->shape_dims = NpyArray_malloc(len * sizeof(npy_intp));
        if (PyArray_IntpFromSequence(shape, self->subarray->shape_dims, len) == -1) {
            NpyArray_free(self->subarray->shape_dims);
            NpyArray_free(self->subarray);
            self->subarray = NULL;
            return PY_FAIL;
        }
        self->subarray->shape_num_dims = len;
        assert(0 == self->subarray->shape_num_dims && NULL == self->subarray->shape_dims || 
               0 < self->subarray->shape_num_dims && NULL != self->subarray->shape_dims);
    }

    if (fields != Py_None) {
        /* Convert sequence of strings to array of char *. self will managed memory in the end. */
        NpyArray_DescrDeallocNamesAndFields(self);
        NpyArray_DescrSetNames(self, arraydescr_seq_to_nameslist(names));         
        self->fields = arraydescr_fields_from_pydict(fields);
        if (!incref_names) {
            Py_DECREF(names);
        }
    }

    if (PyTypeNum_ISEXTENDED(self->type_num)) {
        self->elsize = elsize;
        self->alignment = alignment;
    }

    self->flags = dtypeflags;
    if (version < 3) {
        self->flags = _descr_find_object(self);
    }

    
    if (PyDataType_ISDATETIME(self)
	&& (dtinfo != Py_None)
	&& (dtinfo != NULL)) {
      
      self->dtinfo = _convert_datetime_tuple_to_datetimeinfo( dtinfo );
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/*NUMPY_API
 *
 * Get type-descriptor from an object forcing alignment if possible
 * None goes to DEFAULT type.
 *
 * any object with the .fields attribute and/or .itemsize attribute (if the
 *.fields attribute does not give the total size -- i.e. a partial record
 * naming).  If itemsize is given it must be >= size computed from fields
 *
 * The .fields attribute must return a convertible dictionary if present.
 * Result inherits from PyArray_VOID.
*/
NPY_NO_EXPORT int
PyArray_DescrAlignConverter(PyObject *obj, PyArray_Descr **at)
{
    if (PyDict_Check(obj)) {
        *at =  _convert_from_dict(obj, 1);
    }
    else if (PyBytes_Check(obj)) {
        *at = _convert_from_commastring(obj, 1);
    }
    else if (PyUnicode_Check(obj)) {
        PyObject *tmp;
        tmp = PyUnicode_AsASCIIString(obj);
        *at = _convert_from_commastring(tmp, 1);
        Py_DECREF(tmp);
    }
    else if (PyList_Check(obj)) {
        *at = _convert_from_array_descr(obj, 1);
    }
    else {
        return PyArray_DescrConverter(obj, at);
    }
    if (*at == NULL) {
        if (!PyErr_Occurred()) {
            PyErr_SetString(PyExc_ValueError,
                    "data-type-descriptor not understood");
        }
        return PY_FAIL;
    }
    return PY_SUCCEED;
}

/*NUMPY_API
 *
 * Get type-descriptor from an object forcing alignment if possible
 * None goes to NULL.
 */
NPY_NO_EXPORT int
PyArray_DescrAlignConverter2(PyObject *obj, PyArray_Descr **at)
{
    if (PyDict_Check(obj)) {
        *at =  _convert_from_dict(obj, 1);
    }
    else if (PyBytes_Check(obj)) {
        *at = _convert_from_commastring(obj, 1);
    }
    else if (PyUnicode_Check(obj)) {
        PyObject *tmp;
        tmp = PyUnicode_AsASCIIString(obj);
        *at = _convert_from_commastring(tmp, 1);
        Py_DECREF(tmp);
    }
    else if (PyList_Check(obj)) {
        *at = _convert_from_array_descr(obj, 1);
    }
    else {
        return PyArray_DescrConverter2(obj, at);
    }
    if (*at == NULL) {
        if (!PyErr_Occurred()) {
            PyErr_SetString(PyExc_ValueError,
                    "data-type-descriptor not understood");
        }
        return PY_FAIL;
    }
    return PY_SUCCEED;
}



/*NUMPY_API
 *
 * returns a copy of the PyArray_Descr structure with the byteorder
 * altered:
 * no arguments:  The byteorder is swapped (in all subfields as well)
 * single argument:  The byteorder is forced to the given state
 * (in all subfields as well)
 *
 * Valid states:  ('big', '>') or ('little' or '<')
 * ('native', or '=')
 *
 * If a descr structure with | is encountered it's own
 * byte-order is not changed but any fields are:
 *
 *
 * Deep bytorder change of a data-type descriptor
 * *** Leaves reference count of self unchanged --- does not DECREF self ***
 */
NPY_NO_EXPORT PyArray_Descr *
PyArray_DescrNewByteorder(PyArray_Descr *self, char newendian)
{
    return NpyArray_DescrNewByteorder(self, newendian); /* TODO: Wrap returned descr */
}


static PyObject *
arraydescr_newbyteorder(PyArray_Descr *self, PyObject *args)
{
    char endian=PyArray_SWAP;

    if (!PyArg_ParseTuple(args, "|O&", PyArray_ByteorderConverter,
                &endian)) {
        return NULL;
    }
    return (PyObject *)PyArray_DescrNewByteorder(self, endian);
}

static PyMethodDef arraydescr_methods[] = {
    /* for pickling */
    {"__reduce__",
        (PyCFunction)arraydescr_reduce,
        METH_VARARGS, NULL},
    {"__setstate__",
        (PyCFunction)arraydescr_setstate,
        METH_VARARGS, NULL},
    {"newbyteorder",
        (PyCFunction)arraydescr_newbyteorder,
        METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}           /* sentinel */
};

static PyObject *
arraydescr_str(PyArray_Descr *self)
{
    PyObject *sub;

    if (self->names) {
        PyObject *lst;
        lst = arraydescr_protocol_descr_get(self);
        if (!lst) {
            sub = PyUString_FromString("<err>");
            PyErr_Clear();
        }
        else {
            sub = PyObject_Str(lst);
        }
        Py_XDECREF(lst);
        if (self->type_num != PyArray_VOID) {
            PyObject *p, *t;
            t=PyUString_FromString("'");
            p = arraydescr_protocol_typestr_get(self);
            PyUString_Concat(&p, t);
            PyUString_ConcatAndDel(&t, p);
            p = PyUString_FromString("(");
            PyUString_ConcatAndDel(&p, t);
            PyUString_ConcatAndDel(&p, PyUString_FromString(", "));
            PyUString_ConcatAndDel(&p, sub);
            PyUString_ConcatAndDel(&p, PyUString_FromString(")"));
            sub = p;
        }
    }
    else if (self->subarray) {
        PyObject *p;
        PyObject *t = PyUString_FromString("(");
        PyObject *sh;
        p = arraydescr_str(self->subarray->base);
        if (!self->subarray->base->names && !self->subarray->base->subarray) {
            PyObject *t=PyUString_FromString("'");
            PyUString_Concat(&p, t);
            PyUString_ConcatAndDel(&t, p);
            p = t;
        }
        PyUString_ConcatAndDel(&t, p);
        PyUString_ConcatAndDel(&t, PyUString_FromString(","));
        sh = PyArray_IntTupleFromIntp(self->subarray->shape_num_dims, self->subarray->shape_dims);
        PyUString_ConcatAndDel(&t, PyObject_Str(sh));
        Py_DECREF(sh);
        PyUString_ConcatAndDel(&t, PyUString_FromString(")"));
        sub = t;
    }
    else if (PyDataType_ISFLEXIBLE(self) || !PyArray_ISNBO(self->byteorder)) {
        sub = arraydescr_protocol_typestr_get(self);
    }
    else {
        sub = arraydescr_typename_get(self);
    }
    return sub;
}

static PyObject *
arraydescr_repr(PyArray_Descr *self)
{
    PyObject *sub, *s;
    s = PyUString_FromString("dtype(");
    sub = arraydescr_str(self);
    if (sub == NULL) {
        return sub;
    }
    if (!self->names && !self->subarray) {
        PyObject *t=PyUString_FromString("'");
        PyUString_Concat(&sub, t);
        PyUString_ConcatAndDel(&t, sub);
        sub = t;
    }
    PyUString_ConcatAndDel(&s, sub);
    sub = PyUString_FromString(")");
    PyUString_ConcatAndDel(&s, sub);
    return s;
}

static PyObject *
arraydescr_richcompare(PyArray_Descr *self, PyObject *other, int cmp_op)
{
    PyArray_Descr *new = NULL;
    PyObject *result = Py_NotImplemented;
    if (!PyArray_DescrCheck(other)) {
        if (PyArray_DescrConverter(other, &new) == PY_FAIL) {
            return NULL;
        }
    }
    else {
        new = (PyArray_Descr *)other;
        Py_INCREF(new);
    }
    switch (cmp_op) {
    case Py_LT:
        if (!PyArray_EquivTypes(self, new) && PyArray_CanCastTo(self, new)) {
            result = Py_True;
        }
        else {
            result = Py_False;
        }
        break;
    case Py_LE:
        if (PyArray_CanCastTo(self, new)) {
            result = Py_True;
        }
        else {
            result = Py_False;
        }
        break;
    case Py_EQ:
        if (PyArray_EquivTypes(self, new)) {
            result = Py_True;
        }
        else {
            result = Py_False;
        }
        break;
    case Py_NE:
        if (PyArray_EquivTypes(self, new))
            result = Py_False;
        else
            result = Py_True;
        break;
    case Py_GT:
        if (!PyArray_EquivTypes(self, new) && PyArray_CanCastTo(new, self)) {
            result = Py_True;
        }
        else {
            result = Py_False;
        }
        break;
    case Py_GE:
        if (PyArray_CanCastTo(new, self)) {
            result = Py_True;
        }
        else {
            result = Py_False;
        }
        break;
    default:
        result = Py_NotImplemented;
    }

    Py_XDECREF(new);
    Py_INCREF(result);
    return result;
}

/*************************************************************************
 ****************   Implement Mapping Protocol ***************************
 *************************************************************************/

static Py_ssize_t
descr_length(PyObject *self0)
{
    PyArray_Descr *self = (PyArray_Descr *)self0;

    if (NULL != self->names) {
        int i;
        
        for (i=0; NULL != self->names[i]; i++) ;
        return i;
    }
    else {
        return 0;
    }
}

static PyObject *
descr_repeat(PyObject *self, Py_ssize_t length)
{
    PyObject *tup;
    PyArray_Descr *new;
    if (length < 0) {
        return PyErr_Format(PyExc_ValueError,
                "Array length must be >= 0, not %"INTP_FMT, length);
    }
    tup = Py_BuildValue("O" NPY_SSIZE_T_PYFMT, self, length);
    if (tup == NULL) {
        return NULL;
    }
    PyArray_DescrConverter(tup, &new);
    Py_DECREF(tup);
    return (PyObject *)new;
}

static PyObject *
descr_subscript(PyArray_Descr *self, PyObject *op)
{
    PyObject *retval;

    if (!self->names) {
        PyObject *astr = arraydescr_str(self);
#if defined(NPY_PY3K)
        PyObject *bstr = PyUnicode_AsUnicodeEscapeString(astr);
        Py_DECREF(astr);
        astr = bstr;
#endif
        PyErr_Format(PyExc_KeyError,
                "There are no fields in dtype %s.", PyBytes_AsString(astr));
        Py_DECREF(astr);
        return NULL;
    }
#if defined(NPY_PY3K)
    if (PyUString_Check(op)) {
#else
    if (PyUString_Check(op) || PyUnicode_Check(op)) {
#endif
        NpyArray_DescrField *value = NpyDict_Get(self->fields, PyString_AsString(op));
        PyObject *s;

        if (NULL == value) {
            if (PyUnicode_Check(op)) {
                s = PyUnicode_AsUnicodeEscapeString(op);
            }
            else {
                s = op;
            }

            PyErr_Format(PyExc_KeyError,
                    "Field named \'%s\' not found.", PyBytes_AsString(s));
            if (s != op) {
                Py_DECREF(s);
            }
            return NULL;
        }
        Npy_INCREF(value->descr);            /* TODO: Wrap descriptor */
        retval = (PyObject *)value->descr;
    }
    else if (PyInt_Check(op)) {
        NpyArray_DescrField *field;
        int size;
        int value = PyArray_PyIntAsInt(op);

        for (size=0; NULL != self->names[size]; size++) ;
        
        if (PyErr_Occurred()) {
            return NULL;
        }
        if (value < 0) {
            value += size;
        }
        if (value < 0 || value >= size) {
            PyErr_Format(PyExc_IndexError,
                    "Field index out of range.");
            return NULL;
        }
        
        field = NpyDict_Get(self->fields, self->names[value]);
        retval = (PyObject*) field->descr;      /* TODO: Wrap descr object */
        Npy_INCREF(field->descr);
    }
    else {
        PyErr_SetString(PyExc_ValueError,
                "Field key must be an integer, string, or unicode.");
        return NULL;
    }
    return retval;
}

static PySequenceMethods descr_as_sequence = {
    descr_length,
    (binaryfunc)NULL,
    descr_repeat,
    NULL, NULL,
    NULL,                                        /* sq_ass_item */
    NULL,                                        /* ssizessizeobjargproc sq_ass_slice */
    0,                                           /* sq_contains */
    0,                                           /* sq_inplace_concat */
    0,                                           /* sq_inplace_repeat */
};

static PyMappingMethods descr_as_mapping = {
    descr_length,                                /* mp_length*/
    (binaryfunc)descr_subscript,                 /* mp_subscript*/
    (objobjargproc)NULL,                         /* mp_ass_subscript*/
};

/****************** End of Mapping Protocol ******************************/

NPY_NO_EXPORT PyTypeObject PyArrayDescr_Type = {
#if defined(NPY_PY3K)
    PyVarObject_HEAD_INIT(NULL, 0)
#else
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
#endif
    "numpy.dtype",                              /* tp_name */
    sizeof(PyArray_Descr),                      /* tp_basicsize */
    0,                                          /* tp_itemsize */
    /* methods */
    (destructor)arraydescr_dealloc,             /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
#if defined(NPY_PY3K)
    (void *)0,                                  /* tp_reserved */
#else
    0,                                          /* tp_compare */
#endif
    (reprfunc)arraydescr_repr,                  /* tp_repr */
    0,                                          /* tp_as_number */
    &descr_as_sequence,                         /* tp_as_sequence */
    &descr_as_mapping,                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    (reprfunc)arraydescr_str,                   /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                         /* tp_flags */
    0,                                          /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    (richcmpfunc)arraydescr_richcompare,        /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    arraydescr_methods,                         /* tp_methods */
    arraydescr_members,                         /* tp_members */
    arraydescr_getsets,                         /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    0,                                          /* tp_init */
    0,                                          /* tp_alloc */
    arraydescr_new,                             /* tp_new */
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
