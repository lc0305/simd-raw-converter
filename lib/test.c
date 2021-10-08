/**
 * Copyright (c) 2021 Lucas Crämer (GitHub: lc0305)
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "convert.h"
#include "timer.h"
#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int run_test(const size_t buf_size, size_t src_alloc_size,
             size_t dst_alloc_size) {
  int ret = 0;

  high_resolution_timer timer = high_resolution_time();

  const size_t src_buf_size = buf_size;
  src_alloc_size = (src_alloc_size != 0) ? src_alloc_size : src_buf_size;

  uint8_t *src_buf = (uint8_t *)aligned_alloc(32, src_alloc_size);
  assert(src_buf != NULL);
  uint8_t *src_buf_test = (uint8_t *)malloc(src_alloc_size);
  assert(src_buf_test != NULL);
#ifdef __SSE4_1__
  uint8_t *src_buf_test_sse = (uint8_t *)aligned_alloc(16, src_alloc_size);
  assert(src_buf_test_sse != NULL);
#endif
#ifdef __aarch64__
  uint8_t *src_buf_test_neon = (uint8_t *)aligned_alloc(16, src_alloc_size);
  assert(src_buf_test_neon != NULL);
#endif
#ifdef __AVX2__
  uint8_t *src_buf_test_avx = (uint8_t *)aligned_alloc(32, src_alloc_size);
  assert(src_buf_test_avx != NULL);
#endif
  for (size_t i = 0; i < src_buf_size; ++i) {
    int r = rand();
    src_buf[i] = r;
    src_buf_test[i] = 255 - ((uint8_t)r);
#ifdef __aarch64__
    src_buf_test_neon[i] = 255 - ((uint8_t)r);
#endif
#ifdef __SSE4_1__
    src_buf_test_sse[i] = 255 - ((uint8_t)r);
#endif
#ifdef __AVX2__
    src_buf_test_avx[i] = 255 - ((uint8_t)r);
#endif
  }

  const size_t dst_buf_size = (src_buf_size / 3) * 2;
  dst_alloc_size = (dst_alloc_size != 0) ? dst_alloc_size : dst_buf_size;

  uint16_t *dst_buf = (uint16_t *)malloc(sizeof(uint16_t) * dst_alloc_size);
  assert(dst_buf != NULL);
#ifdef __aarch64__
  uint16_t *dst_buf_neon =
      (uint16_t *)aligned_alloc(16, sizeof(uint16_t) * dst_alloc_size);
  assert(dst_buf_neon != NULL);
#endif
#ifdef __SSE4_1__
  uint16_t *dst_buf_sse =
      (uint16_t *)aligned_alloc(16, sizeof(uint16_t) * dst_alloc_size);
  assert(dst_buf_sse != NULL);
#endif
#ifdef __AVX2__
  uint16_t *dst_buf_avx =
      (uint16_t *)aligned_alloc(32, sizeof(uint16_t) * dst_alloc_size);
  assert(dst_buf_avx != NULL);
#endif
  for (size_t i = 0; i < (dst_buf_size >> 1); ++i) {
    dst_buf[i] = i;
#ifdef __aarch64__
    dst_buf_neon[i] = i;
#endif
#ifdef __SSE4_1__
    dst_buf_sse[i] = i;
#endif
#ifdef __AVX2__
    dst_buf_avx[i] = i;
#endif
  }

  printf("u8_buf_12bit_encoded_to_u16\n");

  restart_high_resolution_timer(&timer);
  if ((ret = u8_buf_12bit_encoded_to_u16_scalar(src_buf, src_buf_size, dst_buf,
                                                dst_buf_size)) < 0) {
    goto error;
  }
  long elapsed_normal = elapsed_high_resolution_time_nanoseconds(&timer);
  printf("Elapsed time normal: %ldns\n", elapsed_normal);

#ifdef __aarch64__
  restart_high_resolution_timer(&timer);
  if ((ret = u8_buf_12bit_encoded_to_u16_neon(
           src_buf, src_buf_size, dst_buf_neon, dst_buf_size)) < 0) {
    goto error;
  }
  long elapsed_neon = elapsed_high_resolution_time_nanoseconds(&timer);
  printf("Elapsed time NEON: %ldns\n", elapsed_neon);

  printf("Delta NEON: %ldns\n\n", elapsed_neon - elapsed_normal);
#endif

#ifdef __SSE4_1__
  restart_high_resolution_timer(&timer);
  if ((ret = u8_buf_12bit_encoded_to_u16_sse4(src_buf, src_buf_size,
                                              dst_buf_sse, dst_buf_size)) < 0) {
    goto error;
  }
  long elapsed_sse4 = elapsed_high_resolution_time_nanoseconds(&timer);
  printf("Elapsed time SSE4: %ldns\n", elapsed_sse4);

  printf("Delta SSE4: %ldns\n", elapsed_sse4 - elapsed_normal);
#endif

#ifdef __AVX2__
  restart_high_resolution_timer(&timer);
  if ((ret = u8_buf_12bit_encoded_to_u16_avx2(src_buf, src_buf_size,
                                              dst_buf_avx, dst_buf_size)) < 0) {
    goto error;
  }
  long elapsed_avx2 = elapsed_high_resolution_time_nanoseconds(&timer);
  printf("Elapsed time AVX2: %ldns\n", elapsed_avx2);

  printf("Delta AVX2: %ldns\n\n", elapsed_avx2 - elapsed_normal);
#endif

  size_t error_counter = 0;

#ifdef __aarch64__
  for (size_t i = 0; i < (dst_buf_size >> 1); ++i) {
    if (dst_buf_neon[i] != dst_buf[i]) {
      printf("Index: %lu, Value normal: %u, Value NEON: %u\n", i, dst_buf[i],
             dst_buf_neon[i]);
      if (++error_counter > 32)
        goto error;
    }
  }
#endif

#ifdef __SSE4_1__
  for (size_t i = 0; i < (dst_buf_size >> 1); ++i) {
    if (dst_buf_sse[i] != dst_buf[i]
#ifdef __AVX2__
        || dst_buf_avx[i] != dst_buf[i]
#endif
    ) {
      printf("Index: %lu, Value normal: %u, Value SSE4: %u", i, dst_buf[i],
             dst_buf_sse[i]);
#ifdef __AVX2__
      printf(", Value AVX2: %u", dst_buf_avx[i]);
#endif
      printf("\n");
      if (++error_counter > 32)
        goto error;
    }
  }
#endif

  printf("u16_buf_to_u8_12bit_encoded\n");

  restart_high_resolution_timer(&timer);
  if ((ret = u16_buf_to_u8_12bit_encoded_scalar(
           dst_buf, dst_buf_size, src_buf_test, src_buf_size)) < 0) {
    goto error;
  }
  elapsed_normal = elapsed_high_resolution_time_nanoseconds(&timer);
  printf("Elapsed time normal: %ldns\n", elapsed_normal);

#ifdef __aarch64__
  restart_high_resolution_timer(&timer);
  if ((ret = u16_buf_to_u8_12bit_encoded_neon(
           dst_buf_neon, dst_buf_size, src_buf_test_neon, src_buf_size)) < 0) {
    goto error;
  }
  elapsed_neon = elapsed_high_resolution_time_nanoseconds(&timer);
  printf("Elapsed time NEON: %ldns\n", elapsed_neon);

  printf("Delta NEON: %ldns\n", elapsed_neon - elapsed_normal);
#endif

#ifdef __SSE4_1__
  restart_high_resolution_timer(&timer);
  if ((ret = u16_buf_to_u8_12bit_encoded_sse4(
           dst_buf_sse, dst_buf_size, src_buf_test_sse, src_buf_size)) < 0) {
    goto error;
  }
  elapsed_sse4 = elapsed_high_resolution_time_nanoseconds(&timer);
  printf("Elapsed time SSE4: %ldns\n", elapsed_sse4);

  printf("Delta SSE4: %ldns\n", elapsed_sse4 - elapsed_normal);
#endif

#ifdef __AVX2__
  restart_high_resolution_timer(&timer);
  if ((ret = u16_buf_to_u8_12bit_encoded_avx2(
           dst_buf_avx, dst_buf_size, src_buf_test_avx, src_buf_size)) < 0) {
    goto error;
  }
  elapsed_avx2 = elapsed_high_resolution_time_nanoseconds(&timer);
  printf("Elapsed time AVX2: %ldns\n", elapsed_avx2);

  printf("Delta AVX2: %ldns\n", elapsed_avx2 - elapsed_normal);
#endif

#ifdef __aarch64__
  for (size_t i = 0; i < src_buf_size; ++i) {
    if (src_buf[i] != src_buf_test[i] || src_buf[i] != src_buf_test_neon[i]) {
      printf("Index: %lu, Value normal: %u, Value NEON: %u\n", i,
             src_buf_test[i], src_buf_test_neon[i]);
      if (error_counter++ > 20)
        goto error;
    }
  }
#endif

#ifdef __SSE4_1__
  for (size_t i = 0; i < src_buf_size; ++i) {
    if (src_buf[i] != src_buf_test[i] || src_buf[i] != src_buf_test_sse[i]
#ifdef __AVX2__
        || src_buf[i] != src_buf_test_avx[i]
#endif
    ) {
      printf("Index: %lu, Value normal: %u, Value SSE4: %u", i, src_buf_test[i],
             src_buf_test_sse[i]);
#ifdef __AVX2__
      printf(", Value AVX2: %u", src_buf_test_avx[i]);
#endif
      printf("\n");
      if (error_counter++ > 20)
        goto error;
    }
  }
#endif
  printf("\nu8_buf_12bit_encoded_to_log_encoded_12bit\n");

  restart_high_resolution_timer(&timer);
  if ((ret = u8_buf_12bit_encoded_to_log_encoded_12bit_scalar(
           src_buf_test, src_buf_size)) < 0) {
    goto error;
  }
  elapsed_normal = elapsed_high_resolution_time_nanoseconds(&timer);
  printf("Elapsed time normal: %ldns\n", elapsed_normal);

#ifdef __aarch64__
  restart_high_resolution_timer(&timer);
  if ((ret = u8_buf_12bit_encoded_to_log_encoded_12bit_neon(
           src_buf_test_neon, src_buf_size)) < 0) {
    goto error;
  }
  elapsed_neon = elapsed_high_resolution_time_nanoseconds(&timer);
  printf("Elapsed time NEON: %ldns\n", elapsed_neon);

  printf("Delta NEON: %ldns\n", elapsed_neon - elapsed_normal);
#endif

#ifdef __SSE4_1__
  restart_high_resolution_timer(&timer);
  if ((ret = u8_buf_12bit_encoded_to_log_encoded_12bit_sse4(
           src_buf_test_sse, src_buf_size)) < 0) {
    goto error;
  }
  elapsed_sse4 = elapsed_high_resolution_time_nanoseconds(&timer);
  printf("Elapsed time SSE4: %ldns\n", elapsed_sse4);

  printf("Delta SSE4: %ldns\n", elapsed_sse4 - elapsed_normal);
#endif

#ifdef __AVX2__
  restart_high_resolution_timer(&timer);
  if ((ret = u8_buf_12bit_encoded_to_log_encoded_12bit_avx2(
           src_buf_test_avx, src_buf_size)) < 0) {
    goto error;
  }
  elapsed_avx2 = elapsed_high_resolution_time_nanoseconds(&timer);
  printf("Elapsed time AVX2: %ldns\n", elapsed_avx2);

  printf("Delta AVX2: %ldns\n", elapsed_avx2 - elapsed_normal);
#endif

#ifdef __aarch64__
  for (size_t i = 0; i < src_buf_size; ++i) {
    if (src_buf_test[i] != src_buf_test_neon[i]) {
      printf("Index: %lu, Value normal: %u, Value NEON: %u\n", i,
             src_buf_test[i], src_buf_test_neon[i]);
      if (error_counter++ > 20)
        goto error;
    }
  }
#endif

#ifdef __SSE4_1__
  for (size_t i = 0; i < src_buf_size; ++i) {
    if (src_buf_test[i] != src_buf_test_sse[i]
#ifdef __AVX2__
        || src_buf_test[i] != src_buf_test_avx[i]
#endif
    ) {
      printf("Index: %lu, Value normal: %u, Value SSE4: %u", i, src_buf_test[i],
             src_buf_test_sse[i]);
#ifdef __AVX2__
      printf(", Value AVX2: %u", src_buf_test_avx[i]);
#endif
      printf("\n");
      if (error_counter++ > 20)
        goto error;
    }
  }
#endif

  printf("\n\n");
  free(src_buf);
  free(src_buf_test);
  free(dst_buf);
#ifdef __aarch64__
  free(src_buf_test_neon);
  free(dst_buf_neon);
#endif
#ifdef __SSE4_1__
  free(src_buf_test_sse);
  free(dst_buf_sse);
#endif
#ifdef __AVX2__
  free(src_buf_test_avx);
  free(dst_buf_avx);
#endif
  return 0;
error:
  free(src_buf);
  free(src_buf_test);
  free(dst_buf);
#ifdef __aarch64__
  free(src_buf_test_neon);
  free(dst_buf_neon);
#endif
#ifdef __SSE4_1__
  free(src_buf_test_sse);
  free(dst_buf_sse);
#endif
#ifdef __AVX2__
  free(src_buf_test_avx);
  free(dst_buf_avx);
#endif
  fprintf(stderr, "Something went wrong :(\n");
  if (ret)
    fprintf(stderr, "%s\n", cl_error_message_from_return_code(ret));
  return -1;
}

int main() {
  srand(time(NULL));
  for (size_t buf_size = 0; buf_size < 143; buf_size += 12) {
    if (run_test(buf_size, 256, 128) < 0) {
      exit(1);
      return 1;
    }
  }
  printf("FINAL TEST:\n");
  if (run_test(1620 * 2880 * 128, 0, 0) < 0) {
    exit(1);
    return 1;
  }
  printf("\nSuccess: All Tests passed. :)\n");
  return 0;
}
