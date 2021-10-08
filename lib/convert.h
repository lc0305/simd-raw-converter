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

#ifndef __CONVERT_LIB_H__
#define __CONVERT_LIB_H__

#include <inttypes.h>
#include <stdlib.h>

#if defined(__SSE4_1__) || defined(__AVX2__)
#include <emmintrin.h>
#include <immintrin.h>
#include <smmintrin.h>
#include <tmmintrin.h>
#endif

#ifdef __aarch64__
#include <arm_neon.h>
#endif

#define CL_SUCCESS 0
#define CL_ERR_SBUF_DIV_12 -1
#define CL_ERR_DBUF_2_SMALL -2
#define CL_ERR_SBUF_A16 -3
#define CL_ERR_DBUF_A16 -4
#define CL_ERR_SBUF_A32 -5
#define CL_ERR_DBUF_A32 -6
#define CL_ERR_SBUF_DIV_8 -7

#ifdef __cplusplus
extern "C" {
#endif

const char *cl_error_message_from_return_code(int return_code);

/**
 * IMPORTANT: dst_buf must have size of at least ((src_size / 3) * 2) elements
 *                                            or ((src_size / 3) * 4) bytes
 **/
int u8_buf_12bit_encoded_to_u16_scalar(const uint8_t *src_buf, size_t src_size,
                                       uint16_t *dst_buf, size_t dst_size);

#ifdef __aarch64__
/**
 * IMPORTANT: src_buf and dst_buf must be aligned to 16 bytes
 * IMPORTANT: dst_buf must have size of at least ((src_size / 3) * 2) elements
 *                                            or ((src_size / 3) * 4) bytes
 * IMPORTANT: only supported on aarch64 CPUs
 **/
int u8_buf_12bit_encoded_to_u16_neon(const uint8_t *src_buf, size_t src_size,
                                     uint16_t *dst_buf, size_t dst_size);
#endif

#ifdef __SSE4_1__
/**
 * IMPORTANT: src_buf and dst_buf must be aligned to 16 bytes
 * IMPORTANT: dst_buf must have size of at least ((src_size / 3) * 2) elements
 *                                            or ((src_size / 3) * 4) bytes
 * IMPORTANT: only supported on x86-64 CPUs with feature >SSE4.1
 **/
int u8_buf_12bit_encoded_to_u16_sse4(const uint8_t *src_buf, size_t src_size,
                                     uint16_t *dst_buf, size_t dst_size);
#endif

#ifdef __AVX2__
/**
 * IMPORTANT: src_buf and dst_buf must be aligned to 32 bytes
 * IMPORTANT: dst_buf must have size of at least ((src_size / 3) * 2) elements
 *                                            or ((src_size / 3) * 4) bytes
 * IMPORTANT: only supported on x86-64 CPUs with feature >AVX2
 **/
int u8_buf_12bit_encoded_to_u16_avx2(const uint8_t *src_buf, size_t src_size,
                                     uint16_t *dst_buf, size_t dst_size);
#endif

/**
 * IMPORTANT: dst_buf must have size of at least ((src_size / 3) * 2) elements
 *                                            or ((src_size / 3) * 4) bytes
 **/
inline int u8_buf_12bit_encoded_to_u16(const uint8_t *src_buf, size_t src_size,
                                       uint16_t *dst_buf, size_t dst_size) {
#ifdef __AVX2__
  return u8_buf_12bit_encoded_to_u16_avx2(src_buf, src_size, dst_buf, dst_size);
#elif defined(__SSE4_1__)
  return u8_buf_12bit_encoded_to_u16_sse4(src_buf, src_size, dst_buf, dst_size);
#else
  return u8_buf_12bit_encoded_to_u16_scalar(src_buf, src_size, dst_buf,
                                            dst_size);
#endif
}

/**
 * IMPORTANT: dst_buf must have size of at least ((src_size / 2) * 3) elements
 *                                                                   (bytes)
 **/
int u16_buf_to_u8_12bit_encoded_scalar(const uint16_t *src_buf, size_t src_size,
                                       uint8_t *dst_buf, size_t dst_size);

#ifdef __aarch64__
/**
 * IMPORTANT: src_buf and dst_buf must be aligned to 16 bytes
 * IMPORTANT: dst_buf must have size of at least ((src_size / 2) * 3) elements
 *                                                                   (bytes)
 * IMPORTANT: only supported on aarch64 CPUs
 **/
int u16_buf_to_u8_12bit_encoded_neon(const uint16_t *src_buf, size_t src_size,
                                     uint8_t *dst_buf, size_t dst_size);
#endif

#ifdef __SSE4_1__
/**
 * IMPORTANT: src_buf and dst_buf must be aligned to 16 bytes
 * IMPORTANT: dst_buf must have size of at least ((src_size / 2) * 3) elements
 *                                                                   (bytes)
 * IMPORTANT: only supported on x86-64 CPUs with feature >SSE4.1
 **/
int u16_buf_to_u8_12bit_encoded_sse4(const uint16_t *src_buf, size_t src_size,
                                     uint8_t *dst_buf, size_t dst_size);
#endif

#ifdef __AVX2__
/**
 * IMPORTANT: src_buf and dst_buf must be aligned to 32 bytes
 * IMPORTANT: dst_buf must have size of at least ((src_size / 2) * 3) elements
 *                                                                   (bytes)
 * IMPORTANT: only supported on x86-64 CPUs with feature >AVX2
 **/
int u16_buf_to_u8_12bit_encoded_avx2(const uint16_t *src_buf, size_t src_size,
                                     uint8_t *dst_buf, size_t dst_size);
#endif

/**
 * IMPORTANT: dst_buf must have size of at least ((src_size / 2) * 3) elements
 *                                                                   (bytes)
 **/
inline int u16_buf_to_u8_12bit_encoded(const uint16_t *src_buf, size_t src_size,
                                       uint8_t *dst_buf, size_t dst_size) {
#ifdef __AVX2__
  return u16_buf_to_u8_12bit_encoded_avx2(src_buf, src_size, dst_buf, dst_size);
#elif defined(__SSE4_1__)
  return u16_buf_to_u8_12bit_encoded_sse4(src_buf, src_size, dst_buf, dst_size);
#else
  return u16_buf_to_u8_12bit_encoded_scalar(src_buf, src_size, dst_buf,
                                            dst_size);
#endif
}

int u8_buf_12bit_encoded_transform_inplace_scalar(
    uint8_t *src_buf, size_t src_size, void (*transform_fn)(uint16_t[8]));

int u8_buf_12bit_encoded_to_log_encoded_12bit_scalar(uint8_t *src_buf,
                                                     size_t src_size);

#ifdef __aarch64__
/**
 * IMPORTANT: src_buf must be aligned to 16 bytes
 * IMPORTANT: only supported on aarch64 CPUs
 * IMPORTANT: transform_fn_scalar is ONLY used if src_size % 48 != 0
 *            else it can be set to NULL
 **/
int u8_buf_12bit_encoded_transform_inplace_neon(
    uint8_t *src_buf, size_t src_size,
    uint16x8_t (*transform_fn_neon)(uint16x8_t),
    void (*transform_fn_scalar)(uint16_t[8]));

/**
 * IMPORTANT: src_buf must be aligned to 16 bytes
 * IMPORTANT: only supported on aarch64 CPUs
 **/
int u8_buf_12bit_encoded_to_log_encoded_12bit_neon(uint8_t *src_buf,
                                                   size_t src_size);
#endif

#ifdef __SSE4_1__
/**
 * IMPORTANT: src_buf must be aligned to 16 bytes
 * IMPORTANT: only supported on x86-64 CPUs with feature >SSE4.1
 * IMPORTANT: transform_fn_scalar is ONLY used if src_size % 48 != 0
 *            else it can be set to NULL
 **/
int u8_buf_12bit_encoded_transform_inplace_sse4(
    uint8_t *src_buf, size_t src_size, __m128i (*transform_fn_sse)(__m128i),
    void (*transform_fn_scalar)(uint16_t[8]));

/**
 * IMPORTANT: src_buf must be aligned to 16 bytes
 * IMPORTANT: only supported on x86-64 CPUs with feature >SSE4.1
 **/
int u8_buf_12bit_encoded_to_log_encoded_12bit_sse4(uint8_t *src_buf,
                                                   size_t src_size);
#endif

#ifdef __AVX2__
/**
 * IMPORTANT: src_buf must be aligned to 32 bytes
 * IMPORTANT: only supported on x86-64 CPUs with feature >AVX2
 * IMPORTANT: transform_fn_scalar is ONLY used if src_size % 96 != 0
 *            else it can be set to NULL
 **/
int u8_buf_12bit_encoded_transform_inplace_avx2(
    uint8_t *src_buf, size_t src_size, __m256i (*transform_fn_avx)(__m256i),
    void (*transform_fn_scalar)(uint16_t[8]));

/**
 * IMPORTANT: src_buf must be aligned to 32 bytes
 * IMPORTANT: only supported on x86-64 CPUs with feature >AVX2
 **/
int u8_buf_12bit_encoded_to_log_encoded_12bit_avx2(uint8_t *src_buf,
                                                   size_t src_size);
#endif

static inline int u8_buf_12bit_encoded_to_log_encoded_12bit(uint8_t *src_buf,
                                                            size_t src_size) {
#ifdef __aarch64__
  return u8_buf_12bit_encoded_to_log_encoded_12bit_neon(src_buf, src_size);
#elif defined(__AVX2__)
  return u8_buf_12bit_encoded_to_log_encoded_12bit_avx2(src_buf, src_size);
#elif defined(__SSE4_1__)
  return u8_buf_12bit_encoded_to_log_encoded_12bit_sse4(src_buf, src_size);
#else
  return u8_buf_12bit_encoded_to_log_encoded_12bit_scalar(src_buf, src_size);
#endif
}

#ifdef __cplusplus
}
#endif

#endif