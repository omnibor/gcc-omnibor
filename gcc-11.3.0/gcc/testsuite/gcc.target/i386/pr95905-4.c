/* PR target/95905 */
/* { dg-do compile } */
/* { dg-options "-O2 -mavx512bw" } */
/* { dg-final { scan-assembler-times "\tvpmovzxbw\t" 4 } } */
/* { dg-final { scan-assembler-times "\tvpmovzxwd\t" 4 } } */
/* { dg-final { scan-assembler-times "\tvpmovzxdq\t" 4 } } */

typedef unsigned char V1 __attribute__((vector_size (64)));
typedef unsigned short V2 __attribute__((vector_size (64)));
typedef unsigned int V3 __attribute__((vector_size (64)));

V1
f1 (V1 x)
{
  return __builtin_shuffle (x, (V1) {}, (V1) { 0, 64, 1, 65, 2, 66, 3, 67, 4, 68, 5, 69, 6, 70, 7, 71, 8, 72, 9, 73, 10, 74, 11, 75, 12, 76, 13, 77, 14, 78, 15, 79, 16, 80, 17, 81, 18, 82, 19, 83, 20, 84, 21, 85, 22, 86, 23, 87, 24, 88, 25, 89, 26, 90, 27, 91, 28, 92, 29, 93, 30, 94, 31, 95 });
}

V2
f2 (V2 x)
{
  return __builtin_shuffle (x, (V2) {}, (V2) { 0, 32, 1, 33, 2, 34, 3, 35, 4, 36, 5, 37, 6, 38, 7, 39, 8, 40, 9, 41, 10, 42, 11, 43, 12, 44, 13, 45, 14, 46, 15, 47 });
}

V3
f3 (V3 x)
{
  return __builtin_shuffle (x, (V3) {}, (V3) { 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23 });
}

V1
f4 (V1 *x)
{
  return __builtin_shuffle (*x, (V1) {}, (V1) { 0, 64, 1, 65, 2, 66, 3, 67, 4, 68, 5, 69, 6, 70, 7, 71, 8, 72, 9, 73, 10, 74, 11, 75, 12, 76, 13, 77, 14, 78, 15, 79, 16, 80, 17, 81, 18, 82, 19, 83, 20, 84, 21, 85, 22, 86, 23, 87, 24, 88, 25, 89, 26, 90, 27, 91, 28, 92, 29, 93, 30, 94, 31, 95 });
}

V2
f5 (V2 *x)
{
  return __builtin_shuffle (*x, (V2) {}, (V2) { 0, 32, 1, 33, 2, 34, 3, 35, 4, 36, 5, 37, 6, 38, 7, 39, 8, 40, 9, 41, 10, 42, 11, 43, 12, 44, 13, 45, 14, 46, 15, 47 });
}

V3
f6 (V3 *x)
{
  return __builtin_shuffle (*x, (V3) {}, (V3) { 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23 });
}

V1
f7 (V1 x)
{
  return __builtin_shuffle ((V1) {}, x, (V1) { 64, 0, 65, 1, 66, 2, 67, 3, 68, 4, 69, 5, 70, 6, 71, 7, 72, 8, 73, 9, 74, 10, 75, 11, 76, 12, 77, 13, 78, 14, 79, 15, 80, 16, 81, 17, 82, 18, 83, 19, 84, 20, 85, 21, 86, 22, 87, 23, 88, 24, 89, 25, 90, 26, 91, 27, 92, 28, 93, 29, 94, 30, 95, 31 });
}

V2
f8 (V2 x)
{
  return __builtin_shuffle ((V2) {}, x, (V2) { 32, 0, 33, 1, 34, 2, 35, 3, 36, 4, 37, 5, 38, 6, 39, 7, 40, 8, 41, 9, 42, 10, 43, 11, 44, 12, 45, 13, 46, 14, 47, 15 });
}

V3
f9 (V3 x)
{
  return __builtin_shuffle ((V3) {}, x, (V3) { 16, 0, 17, 1, 18, 2, 19, 3, 20, 4, 21, 5, 22, 6, 23, 7 });
}

V1
f10 (V1 *x)
{
  return __builtin_shuffle ((V1) {}, *x, (V1) { 64, 0, 65, 1, 66, 2, 67, 3, 68, 4, 69, 5, 70, 6, 71, 7, 72, 8, 73, 9, 74, 10, 75, 11, 76, 12, 77, 13, 78, 14, 79, 15, 80, 16, 81, 17, 82, 18, 83, 19, 84, 20, 85, 21, 86, 22, 87, 23, 88, 24, 89, 25, 90, 26, 91, 27, 92, 28, 93, 29, 94, 30, 95, 31 });
}

V2
f11 (V2 *x)
{
  return __builtin_shuffle ((V2) {}, *x, (V2) { 32, 0, 33, 1, 34, 2, 35, 3, 36, 4, 37, 5, 38, 6, 39, 7, 40, 8, 41, 9, 42, 10, 43, 11, 44, 12, 45, 13, 46, 14, 47, 15 });
}

V3
f12 (V3 *x)
{
  return __builtin_shuffle ((V3) {}, *x, (V3) { 16, 0, 17, 1, 18, 2, 19, 3, 20, 4, 21, 5, 22, 6, 23, 7 });
}