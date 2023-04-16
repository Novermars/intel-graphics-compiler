/*========================== begin_copyright_notice ============================

Copyright (C) 2017-2021 Intel Corporation

SPDX-License-Identifier: MIT

============================= end_copyright_notice ===========================*/

#include "../include/BiF_Definitions.cl"
#include "../../Headers/spirv.h"

#if defined(cl_khr_fp64)
    #include "../IMF/FP64/atan2_d_la.cl"
#endif // defined(cl_khr_fp64)

// TODO: I think we should be able to use M_PI_F here instead of FLOAT_PI,
// but this does cause very small differences in the results of atan2(),
// since M_PI_F is rounded (and therefore slightly larger than FLOAT_PI).
// We'll need to re-collect some GITS streams if we want to use M_PI_F
// instead.
#define FLOAT_PI                (as_float(0x40490FDA)) // 3.1415926535897930f

float SPIRV_OVERLOADABLE SPIRV_OCL_BUILTIN(atan2, _f32_f32, )( float y, float x )
{
    // atan2(y,x) =  atan(y/x)        if  any y, x > 0    atan_yx
    //            =  atan(y/x) + pi   if y >= 0, x < 0    atan_yx + pi
    //            =  atan(y/x) - pi   if y <  0, x < 0    atan_yx - pi
    //            =  pi/2             if y >= 0, x = 0    pi/2
    //            = -pi/2             if y <  0, x = 0    -pi/2
    //  Sign    = (y >= 0) ? 1 : -1;
    //  atan_yx = (x != 0) ? atan(y/x) : Sign*pi/2;
    //  Coeff   = (x < 0)  ? Sign : 0;
    //  dst     = atan_yx + Coeff*pi;

    float result;

    if(__FastRelaxedMath && (!__APIRS))
    {
        // Implemented as:
        //  atan(y/x) for x > 0,
        //  atan(y/x) + M_PI_F for x < 0 and y > 0, and
        //  atan(y/x) -M_PI_F for x < 0 and y < 0.
        float px = SPIRV_OCL_BUILTIN(atan, _f32, )( y / x );
        float py = px + M_PI_F;
        float ny = px - M_PI_F;

        result = ( y > 0 ) ? py : ny;
        result = ( x > 0 ) ? px : result;
        result = SPIRV_OCL_BUILTIN(fabs, _f32, )(result > M_PI_F) ? px : result;
    }
    else
    {
        // The LA atan2 implementation (IMF/FP32/atan2_s_la.cl)
        // seems to be slower on Mandelbulb algorithm..
        if( __intel_relaxed_isnan(x) |
            __intel_relaxed_isnan(y) )
        {
            result = SPIRV_OCL_BUILTIN(nan, _i32, )(0);
        }
        else
        {
            float signy = SPIRV_OCL_BUILTIN(copysign, _f32_f32, )(1.0f, y);
            if( y == 0.0f )
            {
                float signx = SPIRV_OCL_BUILTIN(copysign, _f32_f32, )(1.0f, x);
                float px = signy * 0.0f;
                float nx = signy * FLOAT_PI;
                // In this case, we need to compare signx against
                // 1.0f, not x > 0.0f, since we need to distinguish
                // between x == +0.0f and x == -0.0f.
                result = ( signx == 1.0f ) ? px : nx;
            }
            else if( x == 0.0f )
            {
                result = signy * ( FLOAT_PI * 0.5f );
            }
            else if( __intel_relaxed_isinf( y ) &
                     __intel_relaxed_isinf( x ) )
            {
                float px = signy * ( FLOAT_PI * 0.25f );
                float nx = signy * ( FLOAT_PI * 0.75f );
                result = ( x > 0.0f ) ? px : nx;
            }
            else
            {
                float px = SPIRV_OCL_BUILTIN(atan, _f32, )( y / x );
                float nx = SPIRV_OCL_BUILTIN(mad, _f32_f32_f32, )( signy, FLOAT_PI, px );
                result = ( x > 0.0f ) ? px : nx;
            }
        }
    }

    return result;
}

GENERATE_SPIRV_OCL_VECTOR_FUNCTIONS_2ARGS_VV_LOOP( atan2, float, float, float, f32, f32 )

#if defined(cl_khr_fp64)

INLINE double SPIRV_OVERLOADABLE SPIRV_OCL_BUILTIN(atan2, _f64_f64, )( double y, double x )
{
    return __ocl_svml_atan2(y, x);
}

GENERATE_SPIRV_OCL_VECTOR_FUNCTIONS_2ARGS_VV_LOOP( atan2, double, double, double, f64, f64 )

#endif // defined(cl_khr_fp64)

#if defined(cl_khr_fp16)

INLINE half SPIRV_OVERLOADABLE SPIRV_OCL_BUILTIN(atan2, _f16_f16, )( half y, half x )
{
    half result;

    if( __intel_relaxed_isnan((float)x) |
        __intel_relaxed_isnan((float)y) )
    {
        result = SPIRV_OCL_BUILTIN(nan, _i32, )(0);
    }
    else
    {
        half signy = SPIRV_OCL_BUILTIN(copysign, _f16_f16, )((half)(1.0), y);
        if( y == 0.0f )
        {
            half signx = SPIRV_OCL_BUILTIN(copysign, _f16_f16, )((half)(1.0), x);
            half px = signy * 0.0f;
            half nx = signy * FLOAT_PI;
            // In this case, we need to compare signx against
            // 1.0f, not x > 0.0f, since we need to distinguish
            // between x == +0.0f and x == -0.0f.
            result = ( signx == 1.0f ) ? px : nx;
        }
        else if( x == 0.0f )
        {
            result = signy * ( FLOAT_PI * 0.5f );
        }
        else if(__intel_relaxed_isinf((float)y) &
                __intel_relaxed_isinf((float)x))
        {
            half px = signy * ( FLOAT_PI * 0.25f );
            half nx = signy * ( FLOAT_PI * 0.75f );
            result = ( x > 0.0f ) ? px : nx;
        }
        else
        {
            half px = SPIRV_OCL_BUILTIN(atan, _f32, )( (float)y / (float)x );
            half nx = SPIRV_OCL_BUILTIN(mad, _f32_f32_f32, )( (float)signy, FLOAT_PI, (float)px );
            result = ( x > 0.0f ) ? px : nx;
        }
    }

    return result;
}

GENERATE_SPIRV_OCL_VECTOR_FUNCTIONS_2ARGS_VV_LOOP( atan2, half, half, half, f16, f16 )

#endif // defined(cl_khr_fp16)
