#include "threads/fixedpoint.h"
#include <stdint.h>
#include <stdio.h>

fp int_to_fp(int n) 
{ 
    return n * f;
}

int fp_to_int_round_zero(fp x) 
{
    return x / f;
}

int fp_to_int_round_near(fp x)
{
    if(x >= 0)
        return (x + f/2) / f;
    else
        return (x - f/2) / f;
}

fp fp_add(fp x, fp y)
{
    return x + y;
}

fp fp_sub(fp x, fp y)
{
    return x - y;
}

fp fp_add_int(fp x, int n)
{
    return x + n*f;
}

fp fp_sub_int(fp x, int n)
{
    return x - n*f;
}

fp fp_mult(fp x, fp y)
{
    return ((int64_t) x) * y / f;
}

fp fp_mult_int(fp x, int n)
{
    return x * n;
}

fp fp_div(fp x, fp y)
{
    return ((int64_t) x) * f / y;
}

fp fp_div_int(fp x, int n)
{
    return x / n;
}