#ifndef FIXED_POINT_H
#define FIXED_POINT_H
typedef int fp;
static const int f = 1 <<14;

fp int_to_fp(int);
int fp_to_int_round_zero(fp);
int fp_to_int_round_near(fp);

fp fp_add(fp, fp);
fp fp_sub(fp, fp);
fp fp_add_int(fp, int);
fp fp_sub_int(fp, int);
fp fp_mult(fp, fp);
fp fp_mult_int(fp, int);
fp fp_div(fp, fp);
fp fp_div_int(fp, int);

#endif /* threads/fixedpoint.h */