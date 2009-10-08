multiarray_global_vars = {
    'NPY_NUMUSERTYPES':             7,
}

multiarray_global_vars_types = {
    'NPY_NUMUSERTYPES':             'int',
}

multiarray_scalar_bool_values = {
    '_PyArrayScalar_BoolValues':    9
}

multiarray_types_api = {
    'PyBigArray_Type':                  1,
    'PyArray_Type':                     2,
    'PyArrayDescr_Type':                3,
    'PyArrayFlags_Type':                4,
    'PyArrayIter_Type':                 5,
    'PyArrayMultiIter_Type':            6,
    'PyBoolArrType_Type':               8,
    'PyGenericArrType_Type':            10,
    'PyNumberArrType_Type':             11,
    'PyIntegerArrType_Type':            12,
    'PySignedIntegerArrType_Type':      13,
    'PyUnsignedIntegerArrType_Type':    14,
    'PyInexactArrType_Type':            15,
    'PyFloatingArrType_Type':           16,
    'PyComplexFloatingArrType_Type':    17,
    'PyFlexibleArrType_Type':           18,
    'PyCharacterArrType_Type':          19,
    'PyByteArrType_Type':               20,
    'PyShortArrType_Type':              21,
    'PyIntArrType_Type':                22,
    'PyLongArrType_Type':               23,
    'PyLongLongArrType_Type':           24,
    'PyUByteArrType_Type':              25,
    'PyUShortArrType_Type':             26,
    'PyUIntArrType_Type':               27,
    'PyULongArrType_Type':              28,
    'PyULongLongArrType_Type':          29,
    'PyFloatArrType_Type':              30,
    'PyDoubleArrType_Type':             31,
    'PyLongDoubleArrType_Type':         32,
    'PyCFloatArrType_Type':             33,
    'PyCDoubleArrType_Type':            34,
    'PyCLongDoubleArrType_Type':        35,
    'PyObjectArrType_Type':             36,
    'PyStringArrType_Type':             37,
    'PyUnicodeArrType_Type':            38,
    'PyVoidArrType_Type':               39,
# Those were added much later, and there is no space anymore between Void and
# first functions from multiarray API
    'PyTimeIntegerArrType_Type':        215,
    'PyDatetimeArrType_Type':           216,
    'PyTimedeltaArrType_Type':          217,
}

#define NPY_NUMUSERTYPES (*(int *)PyArray_API[7])
#define PyBoolArrType_Type (*(PyTypeObject *)PyArray_API[8])
#define _PyArrayScalar_BoolValues ((PyBoolScalarObject *)PyArray_API[9])

multiarray_funcs_api = {
    'PyArray_GetNDArrayCVersion':           0,
    'PyArray_SetNumericOps':                40,
    'PyArray_GetNumericOps':                41,
    'PyArray_INCREF':                       42,
    'PyArray_XDECREF':                      43,
    'PyArray_SetStringFunction':            44,
    'PyArray_DescrFromType':                45,
    'PyArray_TypeObjectFromType':           46,
    'PyArray_Zero':                         47,
    'PyArray_One':                          48,
    'PyArray_CastToType':                   49,
    'PyArray_CastTo':                       50,
    'PyArray_CastAnyTo':                    51,
    'PyArray_CanCastSafely':                52,
    'PyArray_CanCastTo':                    53,
    'PyArray_ObjectType':                   54,
    'PyArray_DescrFromObject':              55,
    'PyArray_ConvertToCommonType':          56,
    'PyArray_DescrFromScalar':              57,
    'PyArray_DescrFromTypeObject':          58,
    'PyArray_Size':                         59,
    'PyArray_Scalar':                       60,
    'PyArray_FromScalar':                   61,
    'PyArray_ScalarAsCtype':                62,
    'PyArray_CastScalarToCtype':            63,
    'PyArray_CastScalarDirect':             64,
    'PyArray_ScalarFromObject':             65,
    'PyArray_GetCastFunc':                  66,
    'PyArray_FromDims':                     67,
    'PyArray_FromDimsAndDataAndDescr':      68,
    'PyArray_FromAny':                      69,
    'PyArray_EnsureArray':                  70,
    'PyArray_EnsureAnyArray':               71,
    'PyArray_FromFile':                     72,
    'PyArray_FromString':                   73,
    'PyArray_FromBuffer':                   74,
    'PyArray_FromIter':                     75,
    'PyArray_Return':                       76,
    'PyArray_GetField':                     77,
    'PyArray_SetField':                     78,
    'PyArray_Byteswap':                     79,
    'PyArray_Resize':                       80,
    'PyArray_MoveInto':                     81,
    'PyArray_CopyInto':                     82,
    'PyArray_CopyAnyInto':                  83,
    'PyArray_CopyObject':                   84,
    'PyArray_NewCopy':                      85,
    'PyArray_ToList':                       86,
    'PyArray_ToString':                     87,
    'PyArray_ToFile':                       88,
    'PyArray_Dump':                         89,
    'PyArray_Dumps':                        90,
    'PyArray_ValidType':                    91,
    'PyArray_UpdateFlags':                  92,
    'PyArray_New':                          93,
    'PyArray_NewFromDescr':                 94,
    'PyArray_DescrNew':                     95,
    'PyArray_DescrNewFromType':             96,
    'PyArray_GetPriority':                  97,
    'PyArray_IterNew':                      98,
    'PyArray_MultiIterNew':                 99,
    'PyArray_PyIntAsInt':                   100,
    'PyArray_PyIntAsIntp':                  101,
    'PyArray_Broadcast':                    102,
    'PyArray_FillObjectArray':              103,
    'PyArray_FillWithScalar':               104,
    'PyArray_CheckStrides':                 105,
    'PyArray_DescrNewByteorder':            106,
    'PyArray_IterAllButAxis':               107,
    'PyArray_CheckFromAny':                 108,
    'PyArray_FromArray':                    109,
    'PyArray_FromInterface':                110,
    'PyArray_FromStructInterface':          111,
    'PyArray_FromArrayAttr':                112,
    'PyArray_ScalarKind':                   113,
    'PyArray_CanCoerceScalar':              114,
    'PyArray_NewFlagsObject':               115,
    'PyArray_CanCastScalar':                116,
    'PyArray_CompareUCS4':                  117,
    'PyArray_RemoveSmallest':               118,
    'PyArray_ElementStrides':               119,
    'PyArray_Item_INCREF':                  120,
    'PyArray_Item_XDECREF':                 121,
    'PyArray_FieldNames':                   122,
    'PyArray_Transpose':                    123,
    'PyArray_TakeFrom':                     124,
    'PyArray_PutTo':                        125,
    'PyArray_PutMask':                      126,
    'PyArray_Repeat':                       127,
    'PyArray_Choose':                       128,
    'PyArray_Sort':                         129,
    'PyArray_ArgSort':                      130,
    'PyArray_SearchSorted':                 131,
    'PyArray_ArgMax':                       132,
    'PyArray_ArgMin':                       133,
    'PyArray_Reshape':                      134,
    'PyArray_Newshape':                     135,
    'PyArray_Squeeze':                      136,
    'PyArray_View':                         137,
    'PyArray_SwapAxes':                     138,
    'PyArray_Max':                          139,
    'PyArray_Min':                          140,
    'PyArray_Ptp':                          141,
    'PyArray_Mean':                         142,
    'PyArray_Trace':                        143,
    'PyArray_Diagonal':                     144,
    'PyArray_Clip':                         145,
    'PyArray_Conjugate':                    146,
    'PyArray_Nonzero':                      147,
    'PyArray_Std':                          148,
    'PyArray_Sum':                          149,
    'PyArray_CumSum':                       150,
    'PyArray_Prod':                         151,
    'PyArray_CumProd':                      152,
    'PyArray_All':                          153,
    'PyArray_Any':                          154,
    'PyArray_Compress':                     155,
    'PyArray_Flatten':                      156,
    'PyArray_Ravel':                        157,
    'PyArray_MultiplyList':                 158,
    'PyArray_MultiplyIntList':              159,
    'PyArray_GetPtr':                       160,
    'PyArray_CompareLists':                 161,
    'PyArray_AsCArray':                     162,
    'PyArray_As1D':                         163,
    'PyArray_As2D':                         164,
    'PyArray_Free':                         165,
    'PyArray_Converter':                    166,
    'PyArray_IntpFromSequence':             167,
    'PyArray_Concatenate':                  168,
    'PyArray_InnerProduct':                 169,
    'PyArray_MatrixProduct':                170,
    'PyArray_CopyAndTranspose':             171,
    'PyArray_Correlate':                    172,
    'PyArray_TypestrConvert':               173,
    'PyArray_DescrConverter':               174,
    'PyArray_DescrConverter2':              175,
    'PyArray_IntpConverter':                176,
    'PyArray_BufferConverter':              177,
    'PyArray_AxisConverter':                178,
    'PyArray_BoolConverter':                179,
    'PyArray_ByteorderConverter':           180,
    'PyArray_OrderConverter':               181,
    'PyArray_EquivTypes':                   182,
    'PyArray_Zeros':                        183,
    'PyArray_Empty':                        184,
    'PyArray_Where':                        185,
    'PyArray_Arange':                       186,
    'PyArray_ArangeObj':                    187,
    'PyArray_SortkindConverter':            188,
    'PyArray_LexSort':                      189,
    'PyArray_Round':                        190,
    'PyArray_EquivTypenums':                191,
    'PyArray_RegisterDataType':             192,
    'PyArray_RegisterCastFunc':             193,
    'PyArray_RegisterCanCast':              194,
    'PyArray_InitArrFuncs':                 195,
    'PyArray_IntTupleFromIntp':             196,
    'PyArray_TypeNumFromName':              197,
    'PyArray_ClipmodeConverter':            198,
    'PyArray_OutputConverter':              199,
    'PyArray_BroadcastToShape':             200,
    '_PyArray_SigintHandler':               201,
    '_PyArray_GetSigintBuf':                202,
    'PyArray_DescrAlignConverter':          203,
    'PyArray_DescrAlignConverter2':         204,
    'PyArray_SearchsideConverter':          205,
    'PyArray_CheckAxis':                    206,
    'PyArray_OverflowMultiplyList':         207,
    'PyArray_CompareString':                208,
    'PyArray_MultiIterFromObjects':         209,
    'PyArray_GetEndianness':                210,
    'PyArray_GetNDArrayCFeatureVersion':    211,
    'PyArray_Correlate2':                   212,
    'PyArray_NeighborhoodIterNew':          213,
    'PyArray_SetDatetimeParseFunction':     214,
    'PyArray_DatetimeToDatetimeStruct':     218,
    'PyArray_TimedeltaToTimedeltaStruct':   219,
    'PyArray_DatetimeStructToDatetime':     220,
    'PyArray_TimedeltaStructToTimedelta':   221,
}

ufunc_api = {
    'PyUFunc_FromFuncAndData':                  0,
    'PyUFunc_RegisterLoopForType':              1,
    'PyUFunc_GenericFunction':                  2,
    'PyUFunc_f_f_As_d_d':                       3,
    'PyUFunc_d_d':                              4,
    'PyUFunc_f_f':                              5,
    'PyUFunc_g_g':                              6,
    'PyUFunc_F_F_As_D_D':                       7,
    'PyUFunc_F_F':                              8,
    'PyUFunc_D_D':                              9,
    'PyUFunc_G_G':                              10,
    'PyUFunc_O_O':                              11,
    'PyUFunc_ff_f_As_dd_d':                     12,
    'PyUFunc_ff_f':                             13,
    'PyUFunc_dd_d':                             14,
    'PyUFunc_gg_g':                             15,
    'PyUFunc_FF_F_As_DD_D':                     16,
    'PyUFunc_DD_D':                             17,
    'PyUFunc_FF_F':                             18,
    'PyUFunc_GG_G':                             19,
    'PyUFunc_OO_O':                             20,
    'PyUFunc_O_O_method':                       21,
    'PyUFunc_OO_O_method':                      22,
    'PyUFunc_On_Om':                            23,
    'PyUFunc_GetPyValues':                      24,
    'PyUFunc_checkfperr':                       25,
    'PyUFunc_clearfperr':                       26,
    'PyUFunc_getfperr':                         27,
    'PyUFunc_handlefperr':                      28,
    'PyUFunc_ReplaceLoopBySignature':           29,
    'PyUFunc_FromFuncAndDataAndSignature':      30,
    'PyUFunc_SetUsesArraysAsData':              31,
}
