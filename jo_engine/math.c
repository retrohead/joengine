/*
** Jo Sega Saturn Engine
** Copyright (c) 2012-2020, Johannes Fetz (johannesfetz@gmail.com)
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of the Johannes Fetz nor the
**       names of its contributors may be used to endorse or promote products
**       derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL Johannes Fetz BE LIABLE FOR ANY
** DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
** LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
** ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
** INCLUDES
*/
#include <stdbool.h>
#include "jo/sgl_prototypes.h"
#include "jo/conf.h"
#include "jo/types.h"
#include "jo/sega_saturn.h"
#include "jo/smpc.h"
#include "jo/time.h"
#include "jo/core.h"
#include "jo/tools.h"
#include "jo/math.h"

/*
** FIXED Q16.16 Number
*/

jo_fixed                jo_fixed_mult(jo_fixed x, jo_fixed y)
{
    int                 a;
    int                 c;
    int                 ac;
    int                 adcb;
    int                 mulH;
    unsigned int        b;
    unsigned int        d;
    unsigned int        bd;
    unsigned int        tmp;
    unsigned int        mulL;

    a = JO_DIV_BY_65536(x);
    c = JO_DIV_BY_65536(y);
    b = (x & 0xFFFF);
    d = (y & 0xFFFF);
    ac = a * c;
    adcb = a * d + c * b;
    bd = b * d;
    mulH = ac + JO_DIV_BY_65536(adcb);
    tmp = JO_MULT_BY_65536(adcb);
    mulL = bd + tmp;
    if (mulL < bd)
        ++mulH;
    if (JO_DIV_BY_2147483648(mulH) == JO_DIV_BY_32768(mulH))
        return (JO_MULT_BY_65536(mulH) | JO_DIV_BY_65536(mulL));
    return (JO_FIXED_OVERFLOW);
}

/* Taylor series approximation */
jo_fixed                jo_fixed_sin(jo_fixed rad)
{
    jo_fixed            result;
    jo_fixed            x2;

    rad = jo_fixed_wrap_to_pi(rad);
    x2 = jo_fixed_mult(rad ,rad);

    result = rad;
    rad = jo_fixed_mult(rad, x2);
    result -= (rad / 6);
    rad = jo_fixed_mult(rad, x2);
    result += (rad / 120);
    rad = jo_fixed_mult(rad, x2);
    result -= (rad / 5040);
    rad = jo_fixed_mult(rad, x2);
    result += (rad / 362880);
    rad = jo_fixed_mult(rad, x2);
    result -= (rad / 39916800);

    return (result);
}

/*
** OTHER
*/

unsigned int        jo_sqrt(unsigned int value)
{
    unsigned int    start;
    unsigned int    end;
    unsigned int    res;
    unsigned int    mid;

    if (value == 0 || value == 1)
        return (value);
    JO_ZERO(res);
    start = 1;
    end = value;
    while (start <= end)
    {
        mid = JO_DIV_BY_2(start + end);
        if (mid * mid == value)
            return (mid);
        if (mid * mid < value)
        {
            start = mid + 1;
            res = mid;
        }
        else
            end = mid - 1;
    }
    return (res);
}

int     jo_gcd(int a, int b)
{
#ifdef JO_DEBUG
    if (a <= 0)
    {
        jo_core_error("a <= 0 in jo_gcd()");
        return (-1);
    }
    if (b <= 0)
    {
        jo_core_error("b <= 0 in jo_gcd()");
        return (-1);
    }
#endif
    while (a != b)
    {
        if (a > b)
            a -= b;
        else
            b -= a;
    }
    return (a);
}

float       jo_atan2f_rad(const float y, const float x)
{
    float   atan;
    float   z;

    if (JO_IS_FLOAT_NULL(x))
    {
        if (y > 0.0f)
            return (JO_PI_2);
        if (JO_IS_FLOAT_NULL(y))
            return 0.0f;
        return (-JO_PI_2);
    }
    z = y / x;
    if (JO_FABS(z) < 1.0f)
    {
        atan = z / (1.0f + 0.28f * z * z);
        if (x < 0.0f)
        {
            if (y < 0.0f)
                return (atan - JO_PI);
            return (atan + JO_PI);
        }
    }
    else
    {
        atan = JO_PI_2 - z / (z * z + 0.28f);
        if (y < 0.0f)
            return (atan - JO_PI);
    }
    return (atan);
}

void                        jo_planar_rotate(const jo_pos2D * const point, const jo_pos2D * const origin, const int angle, jo_pos2D * const result)
{
    register int            dx;
    register int            dy;
    register jo_fixed       cos_theta;
    register jo_fixed       sin_theta;

    dx = point->x - origin->x;
    dy = point->y - origin->y;
    cos_theta = jo_cos(angle);
    sin_theta = jo_sin(angle);
    result->x = (jo_fixed2int(dx * cos_theta) - jo_fixed2int(dy * sin_theta)) + origin->x;
    result->y = (jo_fixed2int(dy * cos_theta) + jo_fixed2int(dx * sin_theta)) + origin->y;
}

/* single phase linear congruential random integer generator */

#define JO_RANDOM_M             (2147483647)
#define JO_RANDOM_A             (16807)
#define JO_RANDOM_Q             (JO_RANDOM_M / JO_RANDOM_A)
#define JO_RANDOM_R             (JO_RANDOM_M % JO_RANDOM_A)

int                             jo_random(int max)
{
    static int                  __jo_seed = 1;

    __jo_seed = JO_RANDOM_A * (__jo_seed % JO_RANDOM_Q) - JO_RANDOM_R * (__jo_seed / JO_RANDOM_Q);
    if (__jo_seed <= 0)
        __jo_seed += JO_RANDOM_M;
    return __jo_seed % max + 1;
}

/*
** END OF FILE
*/
