""" Generic support code for handling standard Numeric arrays      
"""

import base_info


array_convert_code = \
"""
// simple meta-program templates to specify python typecodes
// for each of the numeric types.
template<class T>
class py_type{public: enum {code = 100};};
class py_type<char>{public: enum {code = PyArray_CHAR};};
class py_type<unsigned char>{public: enum { code = PyArray_UBYTE};};
class py_type<short>{public:  enum { code = PyArray_SHORT};};
class py_type<int>{public: enum { code = PyArray_LONG};};// PyArray_INT has troubles;
class py_type<long>{public: enum { code = PyArray_LONG};};
class py_type<float>{public: enum { code = PyArray_FLOAT};};
class py_type<double>{public: enum { code = PyArray_DOUBLE};};
class py_type<std::complex<float> >{public: enum { code = PyArray_CFLOAT};};
class py_type<std::complex<double> >{public: enum { code = PyArray_CDOUBLE};};

class numpy_handler
{
public:
    PyArrayObject* convert_to_numpy(PyObject* py_obj, const char* name)
    {
        PyArrayObject* arr_obj = NULL;
    
        if (!py_obj || !PyArray_Check(py_obj))
            handle_conversion_error(py_obj,"array", name);
    
        // Any need to deal with INC/DEC REFs?
        Py_INCREF(py_obj);
        return (PyArrayObject*) py_obj;
    }
    
    PyArrayObject* py_to_numpy(PyObject* py_obj, const char* name)
    {
        PyArrayObject* arr_obj = NULL;
    
        if (!py_obj || !PyArray_Check(py_obj))
            handle_bad_type(py_obj,"array", name);
    
        // Any need to deal with INC/DEC REFs?
        Py_INCREF(py_obj);
        return (PyArrayObject*) py_obj;
    }
};

numpy_handler x__numpy_handler = numpy_handler();
#define convert_to_numpy x__numpy_handler.convert_to_numpy
#define py_to_numpy x__numpy_handler.py_to_numpy
"""

type_check_code = \
"""
class numpy_type_handler
{
public:
    void conversion_numpy_check_type(PyArrayObject* arr_obj, int numeric_type,
                                     const char* name)
    {
        // Make sure input has correct numeric type.
        // allow character and byte to match
        // ?? Should int and long match??
        if (arr_obj->descr->type_num != numeric_type &&
            !(numeric_type == 0 && arr_obj->descr->type_num == 2) &&
            !(numeric_type == 2 && arr_obj->descr->type_num == 0)) 
        {
            char* type_names[13] = {"char","unsigned byte","byte", "short", "int", 
                                    "long", "float", "double", "complex float",
                                    "complex double", "object","ntype","unkown"};
            char msg[500];
            sprintf(msg,"Conversion Error: received '%s' typed array instead of '%s' typed array for variable '%s'",
                    type_names[arr_obj->descr->type_num],type_names[numeric_type],name);
            throw_error(PyExc_TypeError,msg);    
        }
    }
    
    void numpy_check_type(PyArrayObject* arr_obj, int numeric_type, const char* name)
    {
        // Make sure input has correct numeric type.
        if (arr_obj->descr->type_num != numeric_type &&
            !(numeric_type == 0 && arr_obj->descr->type_num == 2) &&
            !(numeric_type == 2 && arr_obj->descr->type_num == 0)) 
        {
            char* type_names[13] = {"char","unsigned byte","byte", "short", "int", 
                                    "long", "float", "double", "complex float",
                                    "complex double", "object","ntype","unkown"};
            char msg[500];
            sprintf(msg,"received '%s' typed array instead of '%s' typed array for variable '%s'",
                    type_names[arr_obj->descr->type_num],type_names[numeric_type],name);
            throw_error(PyExc_TypeError,msg);    
        }
    }
};

numpy_type_handler x__numpy_type_handler = numpy_type_handler();
#define conversion_numpy_check_type x__numpy_type_handler.conversion_numpy_check_type
#define numpy_check_type x__numpy_type_handler.numpy_check_type

"""

size_check_code = \
"""
class numpy_size_handler
{
public:
    void conversion_numpy_check_size(PyArrayObject* arr_obj, int Ndims, 
                                     const char* name)
    {
        if (arr_obj->nd != Ndims)
        {
            char msg[500];
            sprintf(msg,"Conversion Error: received '%d' dimensional array instead of '%d' dimensional array for variable '%s'",
                    arr_obj->nd,Ndims,name);
            throw_error(PyExc_TypeError,msg);
        }    
    }
    
    void numpy_check_size(PyArrayObject* arr_obj, int Ndims, const char* name)
    {
        if (arr_obj->nd != Ndims)
        {
            char msg[500];
            sprintf(msg,"received '%d' dimensional array instead of '%d' dimensional array for variable '%s'",
                    arr_obj->nd,Ndims,name);
            throw_error(PyExc_TypeError,msg);
        }    
    }
};

numpy_size_handler x__numpy_size_handler = numpy_size_handler();
#define conversion_numpy_check_size x__numpy_size_handler.conversion_numpy_check_size
#define numpy_check_size x__numpy_size_handler.numpy_check_size

"""

numeric_init_code = \
"""
Py_Initialize();
import_array();
PyImport_ImportModule("Numeric");
"""

class array_info(base_info.base_info):
    _headers = ['"Numeric/arrayobject.h"','<complex>','<math.h>']
    _support_code = [array_convert_code,size_check_code, type_check_code]
    _module_init_code = [numeric_init_code]    