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

static const char *const error_messages[] = {
    "Success.",                                 // 0
    "Source buffer must be divisible by 12.",   // (-)1
    "Destination buffer is too small.",         // (-)2
    "\"src_buf\" must be aligned to 16 bytes.", // (-)3
    "\"dst_buf\" must be aligned to 16 bytes.", // (-)4
    "\"src_buf\" must be aligned to 32 bytes.", // (-)5
    "\"dst_buf\" must be aligned to 32 bytes.", // (-)6
    "Source buffer must be divisible by 8.",    // (-)7
};

#define ABS(val) (((val) >= 0) ? (val) : (-(val)))

const char *cl_error_message_from_return_code(int return_code) {
  const unsigned int index = ABS(return_code);
  if (index >= (sizeof(error_messages) / sizeof(char *)))
    return NULL;
  return error_messages[index];
}

#define ENCODED_TO_DECODED_SIZE(size) (((size) / 3) << 1)

int u8_buf_12bit_encoded_to_u16_scalar(const uint8_t *src_buf,
                                       const size_t src_size, uint16_t *dst_buf,
                                       const size_t dst_size) {
  if (!src_size)
    return CL_SUCCESS;
  if (src_size % 12)
    return CL_ERR_SBUF_DIV_12;
  if (dst_size < ENCODED_TO_DECODED_SIZE(src_size))
    return CL_ERR_DBUF_2_SMALL;
  for (size_t i_src = 0, i_dst = 0; i_src < (src_size - 11);
       i_src += 12, i_dst += 8) {
    dst_buf[i_dst] = ((src_buf[i_src + 2] << 8) & 0x0F00) |
                     (src_buf[i_src + 1] & 0x00FF); // G2G1G0
    dst_buf[i_dst + 1] = ((src_buf[i_src + 3] << 4) & 0x0FF0) |
                         ((src_buf[i_src + 2] >> 4) & 0x000F); // R2R1R0
    dst_buf[i_dst + 2] = ((src_buf[i_src + 7] << 8) & 0x0F00) |
                         (src_buf[i_src + 6] & 0x00FF); // G5G4G3
    dst_buf[i_dst + 3] = ((src_buf[i_src] << 4) & 0x0FF0) |
                         ((src_buf[i_src + 7] >> 4) & 0x000F); // R5R4R3
    dst_buf[i_dst + 4] = ((src_buf[i_src + 4] << 8) & 0x0F00) |
                         (src_buf[i_src + 11] & 0x00FF); // G8G7G6
    dst_buf[i_dst + 5] = ((src_buf[i_src + 5] << 4) & 0x0FF0) |
                         ((src_buf[i_src + 4] >> 4) & 0x000F); // R8R7R6
    dst_buf[i_dst + 6] = ((src_buf[i_src + 9] << 8) & 0x0F00) |
                         (src_buf[i_src + 8] & 0x00FF); // G11G10G9
    dst_buf[i_dst + 7] = ((src_buf[i_src + 10] << 4) & 0x0FF0) |
                         ((src_buf[i_src + 9] >> 4) & 0x000F); // R11R10R9
  }
  return CL_SUCCESS;
}

#ifdef __aarch64__
// aligned vector load and store operations
#define vld1q_u8_ex(ptr)                                                       \
  vld1q_u8(__builtin_assume_aligned((ptr), sizeof(uint8x16_t)))
#define vld1q_u16_ex(ptr)                                                      \
  vld1q_u16(__builtin_assume_aligned((ptr), sizeof(uint16x8_t)))
#define vld1q_s16_ex(ptr)                                                      \
  vld1q_s16(__builtin_assume_aligned((ptr), sizeof(int16x8_t)))
#define vst1q_u8_ex(ptr, val)                                                  \
  vst1q_u8(__builtin_assume_aligned((ptr), sizeof(uint8x16_t)), (val))
#define vst1q_u16_ex(ptr, val)                                                 \
  vst1q_u16(__builtin_assume_aligned((ptr), sizeof(uint16x8_t)), (val))
#endif

#ifdef __aarch64__

_Alignas(uint8x16_t) static const uint8_t shuffle_mask_hb_u8[16] = {
    2, 3, 7, 0, 4, 5, 9, 10, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
_Alignas(uint8x16_t) static const uint8_t shuffle_mask_lb_u8[16] = {
    1, 2, 6, 7, 11, 4, 8, 9, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
_Alignas(uint16x8_t) static const uint16_t and_mask_hb_u8[8] = {
    0x0F00, 0x0FF0, 0x0F00, 0x0FF0, 0x0F00, 0x0FF0, 0x0F00, 0x0FF0};
_Alignas(int16x8_t) static const int16_t shift_mask_8_4[8] = {
    8, 4, 8, 4, 8, 4, 8, 4,
};
_Alignas(int16x8_t) static const int16_t shift_mask_0_4[8] = {0, -4, 0, -4,
                                                              0, -4, 0, -4};

#define _12bit_encoded_uint8x16_to_uint16x8(                                   \
    __p, __shuffle_mask_hb, __shuffle_mask_lb, __shift_mask_8_4,               \
    __shift_mask_0_4, __and_mask_hb)                                           \
  vbslq_u16(                                                                   \
      (__and_mask_hb),                                                         \
      vshlq_u16(vmovl_u8(vget_low_u8(vqtbl1q_u8((__p), (__shuffle_mask_hb)))), \
                (__shift_mask_8_4)),                                           \
      vshlq_u16(vmovl_u8(vget_low_u8(vqtbl1q_u8((__p), (__shuffle_mask_lb)))), \
                (__shift_mask_0_4)))

int u8_buf_12bit_encoded_to_u16_neon(const uint8_t *src_buf, size_t src_size,
                                     uint16_t *dst_buf, size_t dst_size) {
  if (!src_size)
    return CL_SUCCESS;
  if ((size_t)src_buf & (sizeof(uint8x16_t) - 1))
    return CL_ERR_SBUF_A16;
  if ((size_t)dst_buf & (sizeof(uint16x8_t) - 1))
    return CL_ERR_DBUF_A16;
  if (dst_size < ENCODED_TO_DECODED_SIZE(src_size))
    return CL_ERR_DBUF_2_SMALL;

  size_t dst_size_cut =
      0; // is NOT necessary but GCC won't shut up otherwise :/
  const size_t src_size_cut = src_size % (sizeof(uint8x16_t) * 3);
  if (src_size_cut) {
    dst_size -= (dst_size_cut = ENCODED_TO_DECODED_SIZE(src_size_cut));
    if (!(src_size -= src_size_cut))
      goto done;
  }
  const uint8x16_t __zero_mask = vdupq_n_u8(0);

  const uint8x16_t __shuffle_mask_hb_u8 = vld1q_u8_ex(shuffle_mask_hb_u8);
  const uint8x16_t __shuffle_mask_lb_u8 = vld1q_u8_ex(shuffle_mask_lb_u8);

  const uint16x8_t __and_mask_hb_u8 = vld1q_u16_ex(and_mask_hb_u8);

  const int16x8_t __shift_mask_8_4 = vld1q_s16_ex(shift_mask_8_4);
  const int16x8_t __shift_mask_0_4 = vld1q_s16_ex(shift_mask_0_4);

  for (size_t i_src = 0, i_dst = 0; i_src < (src_size - (12 * 4 - 1));
       i_src += 12 * 4, i_dst += 8 * 4) {
    const uint8x16_t __v0 = vld1q_u8_ex(&src_buf[i_src]);
    vst1q_u16_ex(&dst_buf[i_dst],
                 _12bit_encoded_uint8x16_to_uint16x8(
                     __v0, __shuffle_mask_hb_u8, __shuffle_mask_lb_u8,
                     __shift_mask_8_4, __shift_mask_0_4, __and_mask_hb_u8));

    const uint8x16_t __v1 = vld1q_u8_ex(&src_buf[i_src + 16]);
    vst1q_u16_ex(&dst_buf[i_dst + 8],
                 _12bit_encoded_uint8x16_to_uint16x8(
                     vextq_u8(__v0, __v1, 12), __shuffle_mask_hb_u8,
                     __shuffle_mask_lb_u8, __shift_mask_8_4, __shift_mask_0_4,
                     __and_mask_hb_u8));

    const uint8x16_t __v2 = vld1q_u8_ex(&src_buf[i_src + 32]);
    vst1q_u16_ex(&dst_buf[i_dst + 16],
                 _12bit_encoded_uint8x16_to_uint16x8(
                     vextq_u8(__v1, __v2, 8), __shuffle_mask_hb_u8,
                     __shuffle_mask_lb_u8, __shift_mask_8_4, __shift_mask_0_4,
                     __and_mask_hb_u8));

    vst1q_u16_ex(&dst_buf[i_dst + 24],
                 _12bit_encoded_uint8x16_to_uint16x8(
                     vextq_u8(__v2, __zero_mask, 4), __shuffle_mask_hb_u8,
                     __shuffle_mask_lb_u8, __shift_mask_8_4, __shift_mask_0_4,
                     __and_mask_hb_u8));
  }

  if (src_size_cut) {
  done:
    return u8_buf_12bit_encoded_to_u16_scalar(&src_buf[src_size], src_size_cut,
                                              &dst_buf[dst_size], dst_size_cut);
  }

  return CL_SUCCESS;
}

#endif

#ifdef __SSE4_1__

static inline __m128i _mm_12bit_encoded_epu8_to_epu16(
    const __m128i __p, const __m128i __shuffle_mask_hb,
    const __m128i __shuffle_mask_lb, const __m128i __and_mask_hb,
    const __m128i __and_mask_lb) {
  __m128i __phb = _mm_cvtepu8_epi16(_mm_shuffle_epi8(__p, __shuffle_mask_hb));
  __phb = _mm_and_si128(_mm_blend_epi16(_mm_slli_epi16(__phb, 8),
                                        _mm_slli_epi16(__phb, 4), 0b10101010),
                        __and_mask_hb);
  __m128i __plb = _mm_cvtepu8_epi16(_mm_shuffle_epi8(__p, __shuffle_mask_lb));
  __plb = _mm_and_si128(
      _mm_blend_epi16(__plb, _mm_srli_epi16(__plb, 4), 0b10101010),
      __and_mask_lb);
  return _mm_or_si128(__phb, __plb);
}

int u8_buf_12bit_encoded_to_u16_sse4(const uint8_t *src_buf, size_t src_size,
                                     uint16_t *dst_buf, size_t dst_size) {
  if (!src_size)
    return CL_SUCCESS;
  if ((size_t)src_buf & (sizeof(__m128i) - 1))
    return CL_ERR_SBUF_A16;
  if ((size_t)dst_buf & (sizeof(__m128i) - 1))
    return CL_ERR_DBUF_A16;
  if (dst_size < ENCODED_TO_DECODED_SIZE(src_size))
    return CL_ERR_DBUF_2_SMALL;

  size_t dst_size_cut =
      0; // is NOT necessary but GCC won't shut up otherwise :/
  const size_t src_size_cut = src_size % (sizeof(__m128i) * 3);
  if (src_size_cut) {
    dst_size -= (dst_size_cut = ENCODED_TO_DECODED_SIZE(src_size_cut));
    if (!(src_size -= src_size_cut))
      goto done;
  }

  const __m128i __shuffle_mask_hb =
      _mm_setr_epi8(2, 3, 7, 0, 4, 5, 9, 10, -1, -1, -1, -1, -1, -1, -1, -1);
  const __m128i __shuffle_mask_lb =
      _mm_setr_epi8(1, 2, 6, 7, 11, 4, 8, 9, -1, -1, -1, -1, -1, -1, -1, -1);

  const __m128i __and_mask_hb = _mm_setr_epi16(0x0F00, 0x0FF0, 0x0F00, 0x0FF0,
                                               0x0F00, 0x0FF0, 0x0F00, 0x0FF0);
  const __m128i __and_mask_lb = _mm_setr_epi16(0x00FF, 0x000F, 0x00FF, 0x000F,
                                               0x00FF, 0x000F, 0x00FF, 0x000F);

  for (size_t i_src = 0, i_dst = 0; i_src < (src_size - (12 * 4 - 1));
       i_src += 12 * 4, i_dst += 8 * 4) {
    const __m128i __v0 = _mm_load_si128((const __m128i *)&src_buf[i_src]);
    _mm_store_si128((__m128i *)&dst_buf[i_dst],
                    _mm_12bit_encoded_epu8_to_epu16(
                        __v0, __shuffle_mask_hb, __shuffle_mask_lb,
                        __and_mask_hb, __and_mask_lb));

    const __m128i __v1 = _mm_load_si128((const __m128i *)&src_buf[i_src + 16]);
    _mm_store_si128((__m128i *)&dst_buf[i_dst + 8],
                    _mm_12bit_encoded_epu8_to_epu16(
                        _mm_alignr_epi8(__v1, __v0, 12), __shuffle_mask_hb,
                        __shuffle_mask_lb, __and_mask_hb, __and_mask_lb));

    const __m128i __v2 = _mm_load_si128((const __m128i *)&src_buf[i_src + 32]);
    _mm_store_si128((__m128i *)&dst_buf[i_dst + 16],
                    _mm_12bit_encoded_epu8_to_epu16(
                        _mm_alignr_epi8(__v2, __v1, 8), __shuffle_mask_hb,
                        __shuffle_mask_lb, __and_mask_hb, __and_mask_lb));

    _mm_store_si128((__m128i *)&dst_buf[i_dst + 24],
                    _mm_12bit_encoded_epu8_to_epu16(
                        _mm_srli_si128(__v2, 4), __shuffle_mask_hb,
                        __shuffle_mask_lb, __and_mask_hb, __and_mask_lb));
  }

  if (src_size_cut) {
  done:
    return u8_buf_12bit_encoded_to_u16_scalar(&src_buf[src_size], src_size_cut,
                                              &dst_buf[dst_size], dst_size_cut);
  }

  return CL_SUCCESS;
}

#endif

#ifdef __AVX2__

static inline __m256i _mm256_12bit_encoded_epu8_to_epu16(
    const __m256i __p, const __m256i __shuffle_mask_hb,
    const __m256i __shuffle_mask_lb, const __m256i __and_mask_hb,
    const __m256i __and_mask_lb) {
  __m256i __phb = _mm256_shuffle_epi8(__p, __shuffle_mask_hb);
  __phb = _mm256_cvtepu8_epi16(_mm_or_si128(
      _mm256_castsi256_si128(__phb), _mm256_extracti128_si256(__phb, 1)));
  __phb = _mm256_and_si256(_mm256_blend_epi16(_mm256_slli_epi16(__phb, 8),
                                              _mm256_slli_epi16(__phb, 4),
                                              0b10101010),
                           __and_mask_hb);
  __m256i __plb = _mm256_shuffle_epi8(__p, __shuffle_mask_lb);
  __plb = _mm256_cvtepu8_epi16(_mm_or_si128(
      _mm256_castsi256_si128(__plb), _mm256_extracti128_si256(__plb, 1)));
  __plb = _mm256_and_si256(
      _mm256_blend_epi16(__plb, _mm256_srli_epi16(__plb, 4), 0b10101010),
      __and_mask_lb);
  return _mm256_or_si256(__phb, __plb);
}

int u8_buf_12bit_encoded_to_u16_avx2(const uint8_t *src_buf, size_t src_size,
                                     uint16_t *dst_buf, size_t dst_size) {
  if (!src_size)
    return CL_SUCCESS;
  if ((size_t)src_buf & (sizeof(__m256i) - 1))
    return CL_ERR_SBUF_A32;
  if ((size_t)dst_buf & (sizeof(__m256i) - 1))
    return CL_ERR_DBUF_A32;
  if (dst_size < ENCODED_TO_DECODED_SIZE(src_size))
    return CL_ERR_DBUF_2_SMALL;

  size_t dst_size_cut =
      0; // is NOT necessary but GCC won't shut up otherwise :/
  const size_t src_size_cut = src_size % (sizeof(__m256i) * 3);
  if (src_size_cut) {
    dst_size -= (dst_size_cut = ENCODED_TO_DECODED_SIZE(src_size_cut));
    if (!(src_size -= src_size_cut))
      goto done;
  }

  const __m256i __shuffle_mask_hb =
      _mm256_setr_epi8(2, 3, 7, 0, 4, 5, 9, 10, -1, -1, -1, -1, -1, -1, -1, -1,
                       -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 7, 0, 4, 5, 9, 10);
  const __m256i __shuffle_mask_lb =
      _mm256_setr_epi8(1, 2, 6, 7, 11, 4, 8, 9, -1, -1, -1, -1, -1, -1, -1, -1,
                       -1, -1, -1, -1, -1, -1, -1, -1, 1, 2, 6, 7, 11, 4, 8, 9);

  const __m256i __and_mask_hb = _mm256_setr_epi16(
      0x0F00, 0x0FF0, 0x0F00, 0x0FF0, 0x0F00, 0x0FF0, 0x0F00, 0x0FF0, 0x0F00,
      0x0FF0, 0x0F00, 0x0FF0, 0x0F00, 0x0FF0, 0x0F00, 0x0FF0);
  const __m256i __and_mask_lb = _mm256_setr_epi16(
      0x00FF, 0x000F, 0x00FF, 0x000F, 0x00FF, 0x000F, 0x00FF, 0x000F, 0x00FF,
      0x000F, 0x00FF, 0x000F, 0x00FF, 0x000F, 0x00FF, 0x000F);

  for (size_t i_src = 0, i_dst = 0; i_src < (src_size - (12 * 8 - 1));
       i_src += 12 * 8, i_dst += 8 * 8) {

    const __m256i __v0 = _mm256_load_si256((const __m256i *)&src_buf[i_src]);
    const __m128i __v00 = _mm256_castsi256_si128(__v0);
    const __m128i __v01 = _mm256_extracti128_si256(__v0, 1);

    _mm256_store_si256(
        (__m256i *)&dst_buf[i_dst],
        _mm256_12bit_encoded_epu8_to_epu16(
            _mm256_insertf128_si256(_mm256_castsi128_si256(__v00),
                                    _mm_alignr_epi8(__v01, __v00, 12), 1),
            __shuffle_mask_hb, __shuffle_mask_lb, __and_mask_hb,
            __and_mask_lb));

    const __m256i __v1 =
        _mm256_load_si256((const __m256i *)&src_buf[i_src + 32]);
    const __m128i __v10 = _mm256_castsi256_si128(__v1);
    const __m128i __v11 = _mm256_extracti128_si256(__v1, 1);

    _mm256_store_si256(
        (__m256i *)&dst_buf[i_dst + 16],
        _mm256_12bit_encoded_epu8_to_epu16(
            _mm256_insertf128_si256(
                _mm256_castsi128_si256(_mm_alignr_epi8(__v10, __v01, 8)),
                _mm_srli_si128(__v10, 4), 1),
            __shuffle_mask_hb, __shuffle_mask_lb, __and_mask_hb,
            __and_mask_lb));

    const __m256i __v2 =
        _mm256_load_si256((const __m256i *)&src_buf[i_src + 64]);
    const __m128i __v20 = _mm256_castsi256_si128(__v2);
    const __m128i __v21 = _mm256_extracti128_si256(__v2, 1);

    _mm256_store_si256(
        (__m256i *)&dst_buf[i_dst + 32],
        _mm256_12bit_encoded_epu8_to_epu16(
            _mm256_insertf128_si256(_mm256_castsi128_si256(__v11),
                                    _mm_alignr_epi8(__v20, __v11, 12), 1),
            __shuffle_mask_hb, __shuffle_mask_lb, __and_mask_hb,
            __and_mask_lb));

    _mm256_store_si256(
        (__m256i *)&dst_buf[i_dst + 48],
        _mm256_12bit_encoded_epu8_to_epu16(
            _mm256_insertf128_si256(
                _mm256_castsi128_si256(_mm_alignr_epi8(__v21, __v20, 8)),
                _mm_srli_si128(__v21, 4), 1),
            __shuffle_mask_hb, __shuffle_mask_lb, __and_mask_hb,
            __and_mask_lb));
  }

  if (src_size_cut) {
  done:
    return u8_buf_12bit_encoded_to_u16_scalar(&src_buf[src_size], src_size_cut,
                                              &dst_buf[dst_size], dst_size_cut);
  }

  return CL_SUCCESS;
}

#endif

#define DECODED_TO_ENCODED_SIZE(size) (((size) >> 1) * 3)

int u16_buf_to_u8_12bit_encoded_scalar(const uint16_t *src_buf,
                                       const size_t src_size, uint8_t *dst_buf,
                                       const size_t dst_size) {
  if (!src_size)
    return CL_SUCCESS;
  if (src_size & 7)
    return CL_ERR_SBUF_DIV_8;
  if (dst_size < DECODED_TO_ENCODED_SIZE(src_size))
    return CL_ERR_DBUF_2_SMALL;
  for (size_t i_src = 0, i_dst = 0; i_src < (src_size - 7);
       i_src += 8, i_dst += 12) {
    dst_buf[i_dst] = (src_buf[i_src + 3] >> 4) & 0xFF; // R5R4
    dst_buf[i_dst + 1] = src_buf[i_src] & 0xFF;        // G1G0
    dst_buf[i_dst + 2] = ((src_buf[i_src + 1] << 4) & 0xF0) |
                         ((src_buf[i_src] >> 8) & 0x0F);     // R0G2
    dst_buf[i_dst + 3] = ((src_buf[i_src + 1] >> 4) & 0xFF); // R2R1
    dst_buf[i_dst + 4] = ((src_buf[i_src + 5] << 4) & 0xF0) |
                         ((src_buf[i_src + 4] >> 8) & 0x0F); // R6G8
    dst_buf[i_dst + 5] = (src_buf[i_src + 5] >> 4) & 0xFF;   // R8R7
    dst_buf[i_dst + 6] = src_buf[i_src + 2] & 0xFF;          // G4G3
    dst_buf[i_dst + 7] = ((src_buf[i_src + 3] << 4) & 0xF0) |
                         ((src_buf[i_src + 2] >> 8) & 0x0F); // R3G5
    dst_buf[i_dst + 8] = src_buf[i_src + 6] & 0xFF;          // G10G9
    dst_buf[i_dst + 9] = ((src_buf[i_src + 7] << 4) & 0xF0) |
                         ((src_buf[i_src + 6] >> 8) & 0x0F); // R9G11
    dst_buf[i_dst + 10] = (src_buf[i_src + 7] >> 4) & 0xFF;  // R11R10
    dst_buf[i_dst + 11] = src_buf[i_src + 4] & 0xFF;         // G7G6
  }
  return CL_SUCCESS;
}

#ifdef __aarch64__

_Alignas(uint8x16_t) static const uint8_t shuffle_mask_hb_u16[16] = {
    0xFF, 0, 2, 0xFF, 10, 0xFF, 4, 6, 12, 14, 0xFF, 8, 0xFF, 0xFF, 0xFF, 0xFF};
_Alignas(uint8x16_t) static const uint8_t shuffle_mask_lb_u16[16] = {
    6, 0xFF, 0, 2, 8, 10, 0xFF, 4, 0xFF, 12, 14, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
_Alignas(uint8x16_t) static const uint8_t and_mask_hb_u16[16] = {
    0x00, 0xFF, 0xF0, 0x00, 0xF0, 0x00, 0xFF, 0xF0,
    0xFF, 0xF0, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00};

#define _uint16x8_to_12bit_encoded_uint8x16(                                   \
    __v, __and_mask_hb, __shift_mask_8_4, __shift_mask_0_4,                    \
    __dst_shuffle_mask_hb, __dst_shuffle_mask_lb)                              \
  vbslq_u8(                                                                    \
      (__and_mask_hb),                                                         \
      vqtbl1q_u8(vreinterpretq_u8_u16(vshlq_u16((__v), (__shift_mask_0_4))),   \
                 (__dst_shuffle_mask_hb)),                                     \
      vqtbl1q_u8(vreinterpretq_u8_u16(vshlq_u16((__v), (__shift_mask_8_4))),   \
                 (__dst_shuffle_mask_lb)))

int u16_buf_to_u8_12bit_encoded_neon(const uint16_t *src_buf, size_t src_size,
                                     uint8_t *dst_buf, size_t dst_size) {
  if (!src_size)
    return CL_SUCCESS;
  if ((size_t)src_buf & (sizeof(uint16x8_t) - 1))
    return CL_ERR_SBUF_A16;
  if ((size_t)dst_buf & (sizeof(uint8x16_t) - 1))
    return CL_ERR_DBUF_A16;
  if (dst_size < DECODED_TO_ENCODED_SIZE(src_size))
    return CL_ERR_DBUF_2_SMALL;

  size_t dst_size_cut =
      0; // is NOT necessary but GCC won't shut up otherwise :/
  const size_t src_size_cut = src_size & (sizeof(uint16x8_t) * 2 - 1);
  if (src_size_cut) {
    dst_size -= (dst_size_cut = DECODED_TO_ENCODED_SIZE(src_size_cut));
    if (!(src_size -= src_size_cut))
      goto done;
  }
  const uint8x16_t __zero_mask = vdupq_n_u8(0);

  const uint8x16_t __shuffle_mask_hb_u16 = vld1q_u8_ex(shuffle_mask_hb_u16);
  const uint8x16_t __shuffle_mask_lb_u16 = vld1q_u8_ex(shuffle_mask_lb_u16);

  const uint8x16_t __and_mask_hb_u16 = vld1q_u8_ex(and_mask_hb_u16);

  const int16x8_t __shiftr_mask_8_4 = vnegq_s16(vld1q_s16_ex(shift_mask_8_4));
  const int16x8_t __shiftl_mask_0_4 = vnegq_s16(vld1q_s16_ex(shift_mask_0_4));

  for (size_t i_src = 0, i_dst = 0; i_src < (src_size - (8 * 4 - 1));
       i_src += 8 * 4, i_dst += 12 * 4) {

    const uint8x16_t __v0 = _uint16x8_to_12bit_encoded_uint8x16(
        vld1q_u16_ex(&src_buf[i_src]), __and_mask_hb_u16, __shiftr_mask_8_4,
        __shiftl_mask_0_4, __shuffle_mask_hb_u16, __shuffle_mask_lb_u16);

    const uint8x16_t __v1 = _uint16x8_to_12bit_encoded_uint8x16(
        vld1q_u16_ex(&src_buf[i_src + 8]), __and_mask_hb_u16, __shiftr_mask_8_4,
        __shiftl_mask_0_4, __shuffle_mask_hb_u16, __shuffle_mask_lb_u16);

    vst1q_u8_ex(&dst_buf[i_dst],
                vorrq_u8(__v0, vextq_u8(__zero_mask, __v1, 4)));

    const uint8x16_t __v2 = _uint16x8_to_12bit_encoded_uint8x16(
        vld1q_u16_ex(&src_buf[i_src + 16]), __and_mask_hb_u16,
        __shiftr_mask_8_4, __shiftl_mask_0_4, __shuffle_mask_hb_u16,
        __shuffle_mask_lb_u16);

    vst1q_u8_ex(&dst_buf[i_dst + 16], vorrq_u8(vextq_u8(__v1, __zero_mask, 4),
                                               vextq_u8(__zero_mask, __v2, 8)));

    const uint8x16_t __v3 = _uint16x8_to_12bit_encoded_uint8x16(
        vld1q_u16_ex(&src_buf[i_src + 24]), __and_mask_hb_u16,
        __shiftr_mask_8_4, __shiftl_mask_0_4, __shuffle_mask_hb_u16,
        __shuffle_mask_lb_u16);

    vst1q_u8_ex(&dst_buf[i_dst + 32],
                vorrq_u8(vextq_u8(__v2, __zero_mask, 8),
                         vextq_u8(__zero_mask, __v3, 12)));
  }

  if (src_size_cut) {
  done:
    return u16_buf_to_u8_12bit_encoded_scalar(&src_buf[src_size], src_size_cut,
                                              &dst_buf[dst_size], dst_size_cut);
  }

  return CL_SUCCESS;
}

#endif

#ifdef __SSE4_1__

static inline __m128i _mm_epu16_to_12bit_encoded_epu8(
    const __m128i __v, const __m128i __and_mask_hb, const __m128i __and_mask_lb,
    const __m128i __dst_shuffle_mask_hb, const __m128i __dst_shuffle_mask_lb) {
  return _mm_or_si128(
      _mm_shuffle_epi8(
          _mm_and_si128(
              _mm_blend_epi16(__v, _mm_slli_epi16(__v, 4), 0b10101010),
              __and_mask_hb),
          __dst_shuffle_mask_hb),
      _mm_shuffle_epi8(
          _mm_and_si128(_mm_blend_epi16(_mm_srli_epi16(__v, 8),
                                        _mm_srli_epi16(__v, 4), 0b10101010),
                        __and_mask_lb),
          __dst_shuffle_mask_lb));
}

int u16_buf_to_u8_12bit_encoded_sse4(const uint16_t *src_buf, size_t src_size,
                                     uint8_t *dst_buf, size_t dst_size) {
  if (!src_size)
    return CL_SUCCESS;
  if ((size_t)src_buf & (sizeof(__m128i) - 1))
    return CL_ERR_SBUF_A16;
  if ((size_t)dst_buf & (sizeof(__m128i) - 1))
    return CL_ERR_DBUF_A16;
  if (dst_size < DECODED_TO_ENCODED_SIZE(src_size))
    return CL_ERR_DBUF_2_SMALL;

  size_t dst_size_cut =
      0; // is NOT necessary but GCC won't shut up otherwise :/
  const size_t src_size_cut = src_size & (sizeof(__m128i) * 2 - 1);
  if (src_size_cut) {
    dst_size -= (dst_size_cut = DECODED_TO_ENCODED_SIZE(src_size_cut));
    if (!(src_size -= src_size_cut))
      goto done;
  }

  const __m128i __and_mask_hb = _mm_setr_epi16(0x00FF, 0x00F0, 0x00FF, 0x00F0,
                                               0x00FF, 0x00F0, 0x00FF, 0x00F0);
  const __m128i __dst_shuffle_mask_hb =
      _mm_setr_epi8(-1, 0, 2, -1, 10, -1, 4, 6, 12, 14, -1, 8, -1, -1, -1, -1);

  const __m128i __and_mask_lb = _mm_setr_epi16(0x000F, 0x00FF, 0x000F, 0x00FF,
                                               0x000F, 0x00FF, 0x000F, 0x00FF);
  const __m128i __dst_shuffle_mask_lb =
      _mm_setr_epi8(6, -1, 0, 2, 8, 10, -1, 4, -1, 12, 14, -1, -1, -1, -1, -1);

  for (size_t i_src = 0, i_dst = 0; i_src < (src_size - (8 * 4 - 1));
       i_src += 8 * 4, i_dst += 12 * 4) {

    const __m128i __v0 = _mm_epu16_to_12bit_encoded_epu8(
        _mm_load_si128((const __m128i *)&src_buf[i_src]), __and_mask_hb,
        __and_mask_lb, __dst_shuffle_mask_hb, __dst_shuffle_mask_lb);

    const __m128i __v1 = _mm_epu16_to_12bit_encoded_epu8(
        _mm_load_si128((const __m128i *)&src_buf[i_src + 8]), __and_mask_hb,
        __and_mask_lb, __dst_shuffle_mask_hb, __dst_shuffle_mask_lb);

    _mm_store_si128((__m128i *)&dst_buf[i_dst],
                    _mm_or_si128(__v0, _mm_slli_si128(__v1, 12)));

    const __m128i __v2 = _mm_epu16_to_12bit_encoded_epu8(
        _mm_load_si128((const __m128i *)&src_buf[i_src + 16]), __and_mask_hb,
        __and_mask_lb, __dst_shuffle_mask_hb, __dst_shuffle_mask_lb);

    _mm_store_si128(
        (__m128i *)&dst_buf[i_dst + 16],
        _mm_or_si128(_mm_srli_si128(__v1, 4), _mm_slli_si128(__v2, 8)));

    const __m128i __v3 = _mm_epu16_to_12bit_encoded_epu8(
        _mm_load_si128((const __m128i *)&src_buf[i_src + 24]), __and_mask_hb,
        __and_mask_lb, __dst_shuffle_mask_hb, __dst_shuffle_mask_lb);

    _mm_store_si128(
        (__m128i *)&dst_buf[i_dst + 32],
        _mm_or_si128(_mm_srli_si128(__v2, 8), _mm_slli_si128(__v3, 4)));
  }

  if (src_size_cut) {
  done:
    return u16_buf_to_u8_12bit_encoded_scalar(&src_buf[src_size], src_size_cut,
                                              &dst_buf[dst_size], dst_size_cut);
  }

  return CL_SUCCESS;
}

#endif

#ifdef __AVX2__

static inline __m256i _mm256_epu16_to_12bit_encoded_epu8(
    const __m256i __v, const __m256i __and_mask_hb, const __m256i __and_mask_lb,
    const __m256i __dst_shuffle_mask_hb, const __m256i __dst_shuffle_mask_lb) {
  const __m256i __res = _mm256_or_si256(
      _mm256_shuffle_epi8(
          _mm256_and_si256(
              _mm256_blend_epi16(__v, _mm256_slli_epi16(__v, 4), 0b10101010),
              __and_mask_hb),
          __dst_shuffle_mask_hb),
      _mm256_shuffle_epi8(
          _mm256_and_si256(_mm256_blend_epi16(_mm256_srli_epi16(__v, 8),
                                              _mm256_srli_epi16(__v, 4),
                                              0b10101010),
                           __and_mask_lb),
          __dst_shuffle_mask_lb));
  const __m128i __res_h = _mm256_extracti128_si256(__res, 1);
  return _mm256_inserti128_si256(
      _mm256_castsi128_si256(_mm_or_si128(_mm256_castsi256_si128(__res),
                                          _mm_slli_si128(__res_h, 12))),
      _mm_srli_si128(__res_h, 4), 1);
}

int u16_buf_to_u8_12bit_encoded_avx2(const uint16_t *src_buf, size_t src_size,
                                     uint8_t *dst_buf, size_t dst_size) {
  if (!src_size)
    return CL_SUCCESS;
  if ((size_t)src_buf & (sizeof(__m256i) - 1))
    return CL_ERR_SBUF_A32;
  if ((size_t)dst_buf & (sizeof(__m256i) - 1))
    return CL_ERR_DBUF_A32;
  if (dst_size < DECODED_TO_ENCODED_SIZE(src_size))
    return CL_ERR_DBUF_2_SMALL;

  size_t dst_size_cut =
      0; // is NOT necessary but GCC won't shut up otherwise :/
  const size_t src_size_cut = src_size & (sizeof(__m256i) * 2 - 1);
  if (src_size_cut) {
    dst_size -= (dst_size_cut = DECODED_TO_ENCODED_SIZE(src_size_cut));
    if (!(src_size -= src_size_cut))
      goto done;
  }

  const __m256i __and_mask_hb = _mm256_setr_epi16(
      0x00FF, 0x00F0, 0x00FF, 0x00F0, 0x00FF, 0x00F0, 0x00FF, 0x00F0, 0x00FF,
      0x00F0, 0x00FF, 0x00F0, 0x00FF, 0x00F0, 0x00FF, 0x00F0);
  const __m256i __dst_shuffle_mask_hb = _mm256_setr_epi8(
      -1, 0, 2, -1, 10, -1, 4, 6, 12, 14, -1, 8, -1, -1, -1, -1, -1, 0, 2, -1,
      10, -1, 4, 6, 12, 14, -1, 8, -1, -1, -1, -1);

  const __m256i __and_mask_lb = _mm256_setr_epi16(
      0x000F, 0x00FF, 0x000F, 0x00FF, 0x000F, 0x00FF, 0x000F, 0x00FF, 0x000F,
      0x00FF, 0x000F, 0x00FF, 0x000F, 0x00FF, 0x000F, 0x00FF);
  const __m256i __dst_shuffle_mask_lb = _mm256_setr_epi8(
      6, -1, 0, 2, 8, 10, -1, 4, -1, 12, 14, -1, -1, -1, -1, -1, 6, -1, 0, 2, 8,
      10, -1, 4, -1, 12, 14, -1, -1, -1, -1, -1);

  for (size_t i_src = 0, i_dst = 0; i_src < (src_size - (8 * 8 - 1));
       i_src += 8 * 8, i_dst += 12 * 8) {

    const __m256i __v0 = _mm256_epu16_to_12bit_encoded_epu8(
        _mm256_load_si256((const __m256i *)&src_buf[i_src]), __and_mask_hb,
        __and_mask_lb, __dst_shuffle_mask_hb, __dst_shuffle_mask_lb);
    const __m256i __v1 = _mm256_epu16_to_12bit_encoded_epu8(
        _mm256_load_si256((const __m256i *)&src_buf[i_src + 16]), __and_mask_hb,
        __and_mask_lb, __dst_shuffle_mask_hb, __dst_shuffle_mask_lb);
    _mm256_store_si256(
        (__m256i *)&dst_buf[i_dst],
        _mm256_or_si256(
            __v0, _mm256_inserti128_si256(
                      _mm256_setzero_si256(),
                      _mm_slli_si128(_mm256_castsi256_si128(__v1), 8), 1)));

    const __m256i __v2 = _mm256_epu16_to_12bit_encoded_epu8(
        _mm256_load_si256((const __m256i *)&src_buf[i_src + 32]), __and_mask_hb,
        __and_mask_lb, __dst_shuffle_mask_hb, __dst_shuffle_mask_lb);

    _mm256_store_si256(
        (__m256i *)&dst_buf[i_dst + 32],
        _mm256_inserti128_si256(_mm256_castsi128_si256(_mm_alignr_epi8(
                                    _mm256_extracti128_si256(__v1, 1),
                                    _mm256_castsi256_si128(__v1), 8)),
                                _mm256_castsi256_si128(__v2), 1));

    const __m256i __v3 = _mm256_epu16_to_12bit_encoded_epu8(
        _mm256_load_si256((const __m256i *)&src_buf[i_src + 48]), __and_mask_hb,
        __and_mask_lb, __dst_shuffle_mask_hb, __dst_shuffle_mask_lb);

    _mm256_store_si256(
        (__m256i *)&dst_buf[i_dst + 64],
        _mm256_inserti128_si256(
            _mm256_castsi128_si256(
                _mm_or_si128(_mm256_extracti128_si256(__v2, 1),
                             _mm_slli_si128(_mm256_castsi256_si128(__v3), 8))),
            _mm_or_si128(_mm_srli_si128(_mm256_castsi256_si128(__v3), 8),
                         _mm_slli_si128(_mm256_extracti128_si256(__v3, 1), 8)),
            1));
  }

  if (src_size_cut) {
  done:
    return u16_buf_to_u8_12bit_encoded_scalar(&src_buf[src_size], src_size_cut,
                                              &dst_buf[dst_size], dst_size_cut);
  }

  return CL_SUCCESS;
}

#endif

static inline int u8_buf_12bit_encoded_transform_inplace_scalar_inline(
    uint8_t *src_buf, const size_t src_size,
    void (*transform_fn)(uint16_t[8])) {
  if (!src_size)
    return CL_SUCCESS;
  if (src_size % 12)
    return CL_ERR_SBUF_DIV_12;

  uint16_t dst_temp[8];
  for (size_t i_src = 0; i_src < (src_size - 11); i_src += 12) {
    dst_temp[0] = ((src_buf[i_src + 2] << 8) & 0x0F00) |
                  (src_buf[i_src + 1] & 0x00FF); // G2G1G0
    dst_temp[1] = ((src_buf[i_src + 3] << 4) & 0x0FF0) |
                  ((src_buf[i_src + 2] >> 4) & 0x000F); // R2R1R0
    dst_temp[2] = ((src_buf[i_src + 7] << 8) & 0x0F00) |
                  (src_buf[i_src + 6] & 0x00FF); // G5G4G3
    dst_temp[3] = ((src_buf[i_src] << 4) & 0x0FF0) |
                  ((src_buf[i_src + 7] >> 4) & 0x000F); // R5R4R3
    dst_temp[4] = ((src_buf[i_src + 4] << 8) & 0x0F00) |
                  (src_buf[i_src + 11] & 0x00FF); // G8G7G6
    dst_temp[5] = ((src_buf[i_src + 5] << 4) & 0x0FF0) |
                  ((src_buf[i_src + 4] >> 4) & 0x000F); // R8R7R6
    dst_temp[6] = ((src_buf[i_src + 9] << 8) & 0x0F00) |
                  (src_buf[i_src + 8] & 0x00FF); // G11G10G9
    dst_temp[7] = ((src_buf[i_src + 10] << 4) & 0x0FF0) |
                  ((src_buf[i_src + 9] >> 4) & 0x000F); // R11R10R9

    transform_fn(dst_temp);

    src_buf[i_src] = (dst_temp[3] >> 4) & 0xFF; // R5R4
    src_buf[i_src + 1] = dst_temp[0] & 0xFF;    // G1G0
    src_buf[i_src + 2] =
        ((dst_temp[1] << 4) & 0xF0) | ((dst_temp[0] >> 8) & 0x0F); // R0G2
    src_buf[i_src + 3] = ((dst_temp[1] >> 4) & 0xFF);              // R2R1
    src_buf[i_src + 4] =
        ((dst_temp[5] << 4) & 0xF0) | ((dst_temp[4] >> 8) & 0x0F); // R6G8
    src_buf[i_src + 5] = (dst_temp[5] >> 4) & 0xFF;                // R8R7
    src_buf[i_src + 6] = dst_temp[2] & 0xFF;                       // G4G3
    src_buf[i_src + 7] =
        ((dst_temp[3] << 4) & 0xF0) | ((dst_temp[2] >> 8) & 0x0F); // R3G5
    src_buf[i_src + 8] = dst_temp[6] & 0xFF;                       // G10G9
    src_buf[i_src + 9] =
        ((dst_temp[7] << 4) & 0xF0) | ((dst_temp[6] >> 8) & 0x0F); // R9G11
    src_buf[i_src + 10] = (dst_temp[7] >> 4) & 0xFF;               // R11R10
    src_buf[i_src + 11] = dst_temp[4] & 0xFF;                      // G7G6
  }

  return CL_SUCCESS;
}

int u8_buf_12bit_encoded_transform_inplace_scalar(
    uint8_t *src_buf, const size_t src_size,
    void (*transform_fn)(uint16_t[8])) {
  return u8_buf_12bit_encoded_transform_inplace_scalar_inline(src_buf, src_size,
                                                              transform_fn);
}

#define LOG2U16(x)                                                             \
  ((uint16_t)((8 * sizeof(int) - 1) - __builtin_clz((unsigned int)(x))))

#ifdef __clang__
static inline uint16_t linear_16bit_to_log_encoded_12bit(uint16_t v) {
  const uint16_t q = LOG2U16(v) - 9;
  const uint16_t log_encoded = (q << 9) + (v >> q);
  // compiles to cmov on clang
  return (v < 1024) ? v : log_encoded;
}
#else
static inline uint16_t linear_16bit_to_log_encoded_12bit(uint16_t v) {
  const uint16_t q = LOG2U16(v) - 9;
  return (((q << 9) + (v >> q)) * (v >= 1024)) + (v * (v < 1024));
}
#endif

#define _12BIT_TO_16BIT(v) ((v) << 4)

static inline void to_log_encoded_12bit_inline(uint16_t p_buf[8]) {
  p_buf[0] = linear_16bit_to_log_encoded_12bit(_12BIT_TO_16BIT(p_buf[0]));
  p_buf[1] = linear_16bit_to_log_encoded_12bit(_12BIT_TO_16BIT(p_buf[1]));
  p_buf[2] = linear_16bit_to_log_encoded_12bit(_12BIT_TO_16BIT(p_buf[2]));
  p_buf[3] = linear_16bit_to_log_encoded_12bit(_12BIT_TO_16BIT(p_buf[3]));
  p_buf[4] = linear_16bit_to_log_encoded_12bit(_12BIT_TO_16BIT(p_buf[4]));
  p_buf[5] = linear_16bit_to_log_encoded_12bit(_12BIT_TO_16BIT(p_buf[5]));
  p_buf[6] = linear_16bit_to_log_encoded_12bit(_12BIT_TO_16BIT(p_buf[6]));
  p_buf[7] = linear_16bit_to_log_encoded_12bit(_12BIT_TO_16BIT(p_buf[7]));
}

int u8_buf_12bit_encoded_to_log_encoded_12bit_scalar(uint8_t *src_buf,
                                                     const size_t src_size) {
  return u8_buf_12bit_encoded_transform_inplace_scalar_inline(
      src_buf, src_size, to_log_encoded_12bit_inline);
}

#ifdef __aarch64__

static inline int u8_buf_12bit_encoded_transform_inplace_neon_inline(
    uint8_t *src_buf, size_t src_size,
    uint16x8_t (*transform_fn_neon)(uint16x8_t),
    void (*transform_fn_scalar)(uint16_t[8])) {
  if (!src_size)
    return CL_SUCCESS;
  if ((size_t)src_buf & (sizeof(uint8x16_t) - 1))
    return CL_ERR_SBUF_A16;

  const size_t src_size_cut = src_size % (sizeof(uint8x16_t) * 3);
  if (src_size_cut && !(src_size -= src_size_cut))
    goto done;

  const uint8x16_t __zero_mask = vdupq_n_u8(0);

  const uint8x16_t __shuffle_mask_hb_u8 = vld1q_u8_ex(shuffle_mask_hb_u8);
  const uint8x16_t __shuffle_mask_lb_u8 = vld1q_u8_ex(shuffle_mask_lb_u8);

  const uint16x8_t __and_mask_hb_u8 = vld1q_u16_ex(and_mask_hb_u8);

  const int16x8_t __shift_mask_8_4 = vld1q_s16_ex(shift_mask_8_4);
  const int16x8_t __shift_mask_0_4 = vld1q_s16_ex(shift_mask_0_4);

  const uint8x16_t __shuffle_mask_hb_u16 = vld1q_u8_ex(shuffle_mask_hb_u16);
  const uint8x16_t __shuffle_mask_lb_u16 = vld1q_u8_ex(shuffle_mask_lb_u16);

  const uint8x16_t __and_mask_hb_u16 = vld1q_u8_ex(and_mask_hb_u16);

  const int16x8_t __shiftr_mask_8_4 = vnegq_s16(__shift_mask_8_4);
  const int16x8_t __shiftl_mask_0_4 = vnegq_s16(__shift_mask_0_4);

  for (size_t i_src = 0; i_src < (src_size - (12 * 4 - 1)); i_src += 12 * 4) {

    const uint8x16_t __v0 = vld1q_u8_ex(&src_buf[i_src]);

    const uint8x16_t __v0_res = _uint16x8_to_12bit_encoded_uint8x16(
        transform_fn_neon(_12bit_encoded_uint8x16_to_uint16x8(
            __v0, __shuffle_mask_hb_u8, __shuffle_mask_lb_u8, __shift_mask_8_4,
            __shift_mask_0_4, __and_mask_hb_u8)),
        __and_mask_hb_u16, __shiftr_mask_8_4, __shiftl_mask_0_4,
        __shuffle_mask_hb_u16, __shuffle_mask_lb_u16);

    const uint8x16_t __v1 = vld1q_u8_ex(&src_buf[i_src + 16]);

    const uint8x16_t __v1_res = _uint16x8_to_12bit_encoded_uint8x16(
        transform_fn_neon(_12bit_encoded_uint8x16_to_uint16x8(
            vextq_u8(__v0, __v1, 12), __shuffle_mask_hb_u8,
            __shuffle_mask_lb_u8, __shift_mask_8_4, __shift_mask_0_4,
            __and_mask_hb_u8)),
        __and_mask_hb_u16, __shiftr_mask_8_4, __shiftl_mask_0_4,
        __shuffle_mask_hb_u16, __shuffle_mask_lb_u16);

    vst1q_u8_ex(&src_buf[i_src],
                vorrq_u8(__v0_res, vextq_u8(__zero_mask, __v1_res, 4)));

    const uint8x16_t __v2 = vld1q_u8_ex(&src_buf[i_src + 32]);

    const uint8x16_t __v2_res = _uint16x8_to_12bit_encoded_uint8x16(
        transform_fn_neon(_12bit_encoded_uint8x16_to_uint16x8(
            vextq_u8(__v1, __v2, 8), __shuffle_mask_hb_u8, __shuffle_mask_lb_u8,
            __shift_mask_8_4, __shift_mask_0_4, __and_mask_hb_u8)),
        __and_mask_hb_u16, __shiftr_mask_8_4, __shiftl_mask_0_4,
        __shuffle_mask_hb_u16, __shuffle_mask_lb_u16);

    vst1q_u8_ex(&src_buf[i_src + 16],
                vorrq_u8(vextq_u8(__v1_res, __zero_mask, 4),
                         vextq_u8(__zero_mask, __v2_res, 8)));

    const uint8x16_t __v3_res = _uint16x8_to_12bit_encoded_uint8x16(
        transform_fn_neon(_12bit_encoded_uint8x16_to_uint16x8(
            vextq_u8(__v2, __zero_mask, 4), __shuffle_mask_hb_u8,
            __shuffle_mask_lb_u8, __shift_mask_8_4, __shift_mask_0_4,
            __and_mask_hb_u8)),
        __and_mask_hb_u16, __shiftr_mask_8_4, __shiftl_mask_0_4,
        __shuffle_mask_hb_u16, __shuffle_mask_lb_u16);

    vst1q_u8_ex(&src_buf[i_src + 32],
                vorrq_u8(vextq_u8(__v2_res, __zero_mask, 8),
                         vextq_u8(__zero_mask, __v3_res, 12)));
  }

  if (src_size_cut) {
  done:
    return u8_buf_12bit_encoded_transform_inplace_scalar(
        &src_buf[src_size], src_size_cut, transform_fn_scalar);
  }

  return CL_SUCCESS;
}

int u8_buf_12bit_encoded_transform_inplace_neon(
    uint8_t *src_buf, const size_t src_size,
    uint16x8_t (*transform_fn_neon)(uint16x8_t),
    void (*transform_fn_scalar)(uint16_t[8])) {
  return u8_buf_12bit_encoded_transform_inplace_neon_inline(
      src_buf, src_size, transform_fn_neon, transform_fn_scalar);
}

#define vbsrq_u16(a)                                                           \
  vsubq_u16(vdupq_n_u16(8 * sizeof(uint16_t) - 1), vclzq_u16((a)))

static inline uint16x8_t to_log_encoded_12bit_neon_inline(uint16x8_t __p) {
  __p = vshlq_n_u16(__p, 4);
  const uint16x8_t __q = vsubq_u16(vbsrq_u16(__p), vdupq_n_u16(9));
  return vbslq_u16(
      vcgtq_u16(__p, vdupq_n_u16(1023)),
      vaddq_u16(vshlq_n_u16(__q, 9),
                vshlq_u16(__p, vnegq_s16(vreinterpretq_s16_u16(__q)))),
      __p);
}

int u8_buf_12bit_encoded_to_log_encoded_12bit_neon(uint8_t *src_buf,
                                                   const size_t src_size) {
  return u8_buf_12bit_encoded_transform_inplace_neon_inline(
      src_buf, src_size, to_log_encoded_12bit_neon_inline,
      to_log_encoded_12bit_inline);
}

#endif

#ifdef __SSE4_1__

static inline int u8_buf_12bit_encoded_transform_inplace_sse4_inline(
    uint8_t *src_buf, size_t src_size, __m128i (*transform_fn_sse)(__m128i),
    void (*transform_fn_scalar)(uint16_t[8])) {
  if (!src_size)
    return CL_SUCCESS;
  if ((size_t)src_buf & (sizeof(__m128i) - 1))
    return CL_ERR_SBUF_A16;

  const size_t src_size_cut = src_size % (sizeof(__m128i) * 3);
  if (src_size_cut && !(src_size -= src_size_cut))
    goto done;

  const __m128i __shuffle_mask_hb =
      _mm_setr_epi8(2, 3, 7, 0, 4, 5, 9, 10, -1, -1, -1, -1, -1, -1, -1, -1);
  const __m128i __shuffle_mask_lb =
      _mm_setr_epi8(1, 2, 6, 7, 11, 4, 8, 9, -1, -1, -1, -1, -1, -1, -1, -1);

  const __m128i __and_mask_hb = _mm_setr_epi16(0x0F00, 0x0FF0, 0x0F00, 0x0FF0,
                                               0x0F00, 0x0FF0, 0x0F00, 0x0FF0);
  const __m128i __and_mask_lb = _mm_setr_epi16(0x00FF, 0x000F, 0x00FF, 0x000F,
                                               0x00FF, 0x000F, 0x00FF, 0x000F);

  const __m128i __dst_and_mask_hb = _mm_setr_epi16(
      0x00FF, 0x00F0, 0x00FF, 0x00F0, 0x00FF, 0x00F0, 0x00FF, 0x00F0);
  const __m128i __dst_shuffle_mask_hb =
      _mm_setr_epi8(-1, 0, 2, -1, 10, -1, 4, 6, 12, 14, -1, 8, -1, -1, -1, -1);

  const __m128i __dst_and_mask_lb = _mm_setr_epi16(
      0x000F, 0x00FF, 0x000F, 0x00FF, 0x000F, 0x00FF, 0x000F, 0x00FF);
  const __m128i __dst_shuffle_mask_lb =
      _mm_setr_epi8(6, -1, 0, 2, 8, 10, -1, 4, -1, 12, 14, -1, -1, -1, -1, -1);

  for (size_t i_src = 0; i_src < (src_size - (12 * 4 - 1)); i_src += 12 * 4) {
    const __m128i __v0 = _mm_load_si128((const __m128i *)&src_buf[i_src]);

    const __m128i __v0_res = _mm_epu16_to_12bit_encoded_epu8(
        transform_fn_sse(_mm_12bit_encoded_epu8_to_epu16(
            __v0, __shuffle_mask_hb, __shuffle_mask_lb, __and_mask_hb,
            __and_mask_lb)),
        __dst_and_mask_hb, __dst_and_mask_lb, __dst_shuffle_mask_hb,
        __dst_shuffle_mask_lb);

    const __m128i __v1 = _mm_load_si128((const __m128i *)&src_buf[i_src + 16]);

    const __m128i __v1_res = _mm_epu16_to_12bit_encoded_epu8(
        transform_fn_sse(_mm_12bit_encoded_epu8_to_epu16(
            _mm_alignr_epi8(__v1, __v0, 12), __shuffle_mask_hb,
            __shuffle_mask_lb, __and_mask_hb, __and_mask_lb)),
        __dst_and_mask_hb, __dst_and_mask_lb, __dst_shuffle_mask_hb,
        __dst_shuffle_mask_lb);

    _mm_store_si128((__m128i *)&src_buf[i_src],
                    _mm_or_si128(__v0_res, _mm_slli_si128(__v1_res, 12)));

    const __m128i __v2 = _mm_load_si128((const __m128i *)&src_buf[i_src + 32]);

    const __m128i __v2_res = _mm_epu16_to_12bit_encoded_epu8(
        transform_fn_sse(_mm_12bit_encoded_epu8_to_epu16(
            _mm_alignr_epi8(__v2, __v1, 8), __shuffle_mask_hb,
            __shuffle_mask_lb, __and_mask_hb, __and_mask_lb)),
        __dst_and_mask_hb, __dst_and_mask_lb, __dst_shuffle_mask_hb,
        __dst_shuffle_mask_lb);

    _mm_store_si128(
        (__m128i *)&src_buf[i_src + 16],
        _mm_or_si128(_mm_srli_si128(__v1_res, 4), _mm_slli_si128(__v2_res, 8)));

    const __m128i __v3_res = _mm_epu16_to_12bit_encoded_epu8(
        transform_fn_sse(_mm_12bit_encoded_epu8_to_epu16(
            _mm_srli_si128(__v2, 4), __shuffle_mask_hb, __shuffle_mask_lb,
            __and_mask_hb, __and_mask_lb)),
        __dst_and_mask_hb, __dst_and_mask_lb, __dst_shuffle_mask_hb,
        __dst_shuffle_mask_lb);

    _mm_store_si128(
        (__m128i *)&src_buf[i_src + 32],
        _mm_or_si128(_mm_srli_si128(__v2_res, 8), _mm_slli_si128(__v3_res, 4)));
  }

  if (src_size_cut) {
  done:
    return u8_buf_12bit_encoded_transform_inplace_scalar(
        &src_buf[src_size], src_size_cut, transform_fn_scalar);
  }

  return CL_SUCCESS;
}

int u8_buf_12bit_encoded_transform_inplace_sse4(
    uint8_t *src_buf, const size_t src_size,
    __m128i (*transform_fn_sse)(__m128i),
    void (*transform_fn_scalar)(uint16_t[8])) {
  return u8_buf_12bit_encoded_transform_inplace_sse4_inline(
      src_buf, src_size, transform_fn_sse, transform_fn_scalar);
}

/**
 * credits:
 * https://old.reddit.com/r/simd/comments/b3k1oa/looking_for_sseavx_bitscan_discussions/ej3i3aq/
 **/
static inline __m128i _mm_bsr_epi16(__m128i __h) {
  const __m128i __lut_lo =
      _mm_set_epi8(11, 11, 11, 11, 11, 11, 11, 11, 10, 10, 10, 10, 9, 9, 8, 0);
  const __m128i __lut_hi = _mm_set_epi8(15, 15, 15, 15, 15, 15, 15, 15, 14, 14,
                                        14, 14, 13, 13, 12, 0);
  const __m128i __nibble_mask = _mm_set1_epi8(0x0F);
  const __m128i __adj = _mm_set1_epi16(0x1F08);

  __h = _mm_max_epu8(
      _mm_shuffle_epi8(__lut_hi,
                       _mm_and_si128(_mm_srli_epi16(__h, 4), __nibble_mask)),
      _mm_shuffle_epi8(__lut_lo, _mm_and_si128(__nibble_mask, __h)));

  return _mm_max_epi8(_mm_sub_epi8(__h, __adj), _mm_srli_epi16(__h, 8));
}

#if defined(__AVX2__)

static inline __m128i _mm_srlv_epi16x(__m128i __a, __m128i __count) {
  return _mm_packus_epi32(
      _mm_srlv_epi32(_mm_cvtepu16_epi32(__a), _mm_cvtepu16_epi32(__count)),
      _mm_srlv_epi32(_mm_cvtepu16_epi32(_mm_srli_si128(__a, 8)),
                     _mm_cvtepu16_epi32(_mm_srli_si128(__count, 8))));
}

#else

// sse4.1 does NOT support _mm_srlv_epi32
// in my tests the fastest way to do a vector shift
// is going back to scalars :/

typedef union aligned_buf_128 {
  __m128i __values;
  uint16_t values[(sizeof(__m128i) / sizeof(uint16_t))];
} aligned_buf_128_t;

static inline __m128i _mm_srlv_epi16x(__m128i __a, __m128i __count) {
  aligned_buf_128_t a;
  aligned_buf_128_t count;
  _mm_store_si128(&a.__values, __a);
  _mm_store_si128(&count.__values, __count);

  a.values[0] >>= count.values[0];
  a.values[1] >>= count.values[1];
  a.values[2] >>= count.values[2];
  a.values[3] >>= count.values[3];
  a.values[4] >>= count.values[4];
  a.values[5] >>= count.values[5];
  a.values[6] >>= count.values[6];
  a.values[7] >>= count.values[7];

  return _mm_load_si128(&a.__values);
}

#endif

#define _mm_cmplt_epu16(a, b)                                                  \
  _mm_cmplt_epi16(_mm_xor_si128(a, _mm_set1_epi16(0x8000)),                    \
                  _mm_xor_si128(b, _mm_set1_epi16(0x8000)))

static inline __m128i to_log_encoded_12bit_sse4_inline(__m128i __p) {
  __p = _mm_slli_epi16(__p, 4);
  const __m128i __q = _mm_sub_epi16(_mm_bsr_epi16(__p), _mm_set1_epi16(9));
  return _mm_blendv_epi8(
      _mm_add_epi16(_mm_slli_epi16(__q, 9), _mm_srlv_epi16x(__p, __q)), __p,
      _mm_cmplt_epu16(__p, _mm_set1_epi16(1024)));
}

int u8_buf_12bit_encoded_to_log_encoded_12bit_sse4(uint8_t *src_buf,
                                                   const size_t src_size) {
  return u8_buf_12bit_encoded_transform_inplace_sse4_inline(
      src_buf, src_size, to_log_encoded_12bit_sse4_inline,
      to_log_encoded_12bit_inline);
}

#endif

#ifdef __AVX2__

static inline int u8_buf_12bit_encoded_transform_inplace_avx2_inline(
    uint8_t *src_buf, size_t src_size, __m256i (*transform_fn_avx)(__m256i),
    void (*transform_fn_scalar)(uint16_t[8])) {
  if (!src_size)
    return CL_SUCCESS;
  if ((size_t)src_buf & (sizeof(__m256i) - 1))
    return CL_ERR_SBUF_A32;

  const size_t src_size_cut = src_size % (sizeof(__m256i) * 3);
  if (src_size_cut && !(src_size -= src_size_cut))
    goto done;

  const __m256i __shuffle_mask_hb =
      _mm256_setr_epi8(2, 3, 7, 0, 4, 5, 9, 10, -1, -1, -1, -1, -1, -1, -1, -1,
                       -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 7, 0, 4, 5, 9, 10);
  const __m256i __shuffle_mask_lb =
      _mm256_setr_epi8(1, 2, 6, 7, 11, 4, 8, 9, -1, -1, -1, -1, -1, -1, -1, -1,
                       -1, -1, -1, -1, -1, -1, -1, -1, 1, 2, 6, 7, 11, 4, 8, 9);

  const __m256i __and_mask_hb = _mm256_setr_epi16(
      0x0F00, 0x0FF0, 0x0F00, 0x0FF0, 0x0F00, 0x0FF0, 0x0F00, 0x0FF0, 0x0F00,
      0x0FF0, 0x0F00, 0x0FF0, 0x0F00, 0x0FF0, 0x0F00, 0x0FF0);
  const __m256i __and_mask_lb = _mm256_setr_epi16(
      0x00FF, 0x000F, 0x00FF, 0x000F, 0x00FF, 0x000F, 0x00FF, 0x000F, 0x00FF,
      0x000F, 0x00FF, 0x000F, 0x00FF, 0x000F, 0x00FF, 0x000F);

  const __m256i __dst_and_mask_hb = _mm256_setr_epi16(
      0x00FF, 0x00F0, 0x00FF, 0x00F0, 0x00FF, 0x00F0, 0x00FF, 0x00F0, 0x00FF,
      0x00F0, 0x00FF, 0x00F0, 0x00FF, 0x00F0, 0x00FF, 0x00F0);
  const __m256i __dst_shuffle_mask_hb = _mm256_setr_epi8(
      -1, 0, 2, -1, 10, -1, 4, 6, 12, 14, -1, 8, -1, -1, -1, -1, -1, 0, 2, -1,
      10, -1, 4, 6, 12, 14, -1, 8, -1, -1, -1, -1);

  const __m256i __dst_and_mask_lb = _mm256_setr_epi16(
      0x000F, 0x00FF, 0x000F, 0x00FF, 0x000F, 0x00FF, 0x000F, 0x00FF, 0x000F,
      0x00FF, 0x000F, 0x00FF, 0x000F, 0x00FF, 0x000F, 0x00FF);
  const __m256i __dst_shuffle_mask_lb = _mm256_setr_epi8(
      6, -1, 0, 2, 8, 10, -1, 4, -1, 12, 14, -1, -1, -1, -1, -1, 6, -1, 0, 2, 8,
      10, -1, 4, -1, 12, 14, -1, -1, -1, -1, -1);

  for (size_t i_src = 0; i_src < (src_size - (12 * 8 - 1)); i_src += 12 * 8) {

    const __m256i __v0 = _mm256_load_si256((const __m256i *)&src_buf[i_src]);
    const __m128i __v00 = _mm256_castsi256_si128(__v0);
    const __m128i __v01 = _mm256_extracti128_si256(__v0, 1);

    const __m256i __v0_res = _mm256_epu16_to_12bit_encoded_epu8(
        transform_fn_avx(_mm256_12bit_encoded_epu8_to_epu16(
            _mm256_insertf128_si256(_mm256_castsi128_si256(__v00),
                                    _mm_alignr_epi8(__v01, __v00, 12), 1),
            __shuffle_mask_hb, __shuffle_mask_lb, __and_mask_hb,
            __and_mask_lb)),
        __dst_and_mask_hb, __dst_and_mask_lb, __dst_shuffle_mask_hb,
        __dst_shuffle_mask_lb);

    const __m256i __v1 =
        _mm256_load_si256((const __m256i *)&src_buf[i_src + 32]);
    const __m128i __v10 = _mm256_castsi256_si128(__v1);
    const __m128i __v11 = _mm256_extracti128_si256(__v1, 1);

    const __m256i __v1_res = _mm256_epu16_to_12bit_encoded_epu8(
        transform_fn_avx(_mm256_12bit_encoded_epu8_to_epu16(
            _mm256_insertf128_si256(
                _mm256_castsi128_si256(_mm_alignr_epi8(__v10, __v01, 8)),
                _mm_srli_si128(__v10, 4), 1),
            __shuffle_mask_hb, __shuffle_mask_lb, __and_mask_hb,
            __and_mask_lb)),
        __dst_and_mask_hb, __dst_and_mask_lb, __dst_shuffle_mask_hb,
        __dst_shuffle_mask_lb);

    _mm256_store_si256(
        (__m256i *)&src_buf[i_src],
        _mm256_or_si256(__v0_res,
                        _mm256_inserti128_si256(
                            _mm256_setzero_si256(),
                            _mm_slli_si128(_mm256_castsi256_si128(__v1_res), 8),
                            1)));

    const __m256i __v2 =
        _mm256_load_si256((const __m256i *)&src_buf[i_src + 64]);
    const __m128i __v20 = _mm256_castsi256_si128(__v2);
    const __m128i __v21 = _mm256_extracti128_si256(__v2, 1);

    const __m256i __v2_res = _mm256_epu16_to_12bit_encoded_epu8(
        transform_fn_avx(_mm256_12bit_encoded_epu8_to_epu16(
            _mm256_insertf128_si256(_mm256_castsi128_si256(__v11),
                                    _mm_alignr_epi8(__v20, __v11, 12), 1),
            __shuffle_mask_hb, __shuffle_mask_lb, __and_mask_hb,
            __and_mask_lb)),
        __dst_and_mask_hb, __dst_and_mask_lb, __dst_shuffle_mask_hb,
        __dst_shuffle_mask_lb);

    _mm256_store_si256(
        (__m256i *)&src_buf[i_src + 32],
        _mm256_inserti128_si256(_mm256_castsi128_si256(_mm_alignr_epi8(
                                    _mm256_extracti128_si256(__v1_res, 1),
                                    _mm256_castsi256_si128(__v1_res), 8)),
                                _mm256_castsi256_si128(__v2_res), 1));

    const __m256i __v3_res = _mm256_epu16_to_12bit_encoded_epu8(
        transform_fn_avx(_mm256_12bit_encoded_epu8_to_epu16(
            _mm256_insertf128_si256(
                _mm256_castsi128_si256(_mm_alignr_epi8(__v21, __v20, 8)),
                _mm_srli_si128(__v21, 4), 1),
            __shuffle_mask_hb, __shuffle_mask_lb, __and_mask_hb,
            __and_mask_lb)),
        __dst_and_mask_hb, __dst_and_mask_lb, __dst_shuffle_mask_hb,
        __dst_shuffle_mask_lb);

    _mm256_store_si256(
        (__m256i *)&src_buf[i_src + 64],
        _mm256_inserti128_si256(
            _mm256_castsi128_si256(_mm_or_si128(
                _mm256_extracti128_si256(__v2_res, 1),
                _mm_slli_si128(_mm256_castsi256_si128(__v3_res), 8))),
            _mm_or_si128(
                _mm_srli_si128(_mm256_castsi256_si128(__v3_res), 8),
                _mm_slli_si128(_mm256_extracti128_si256(__v3_res, 1), 8)),
            1));
  }

  if (src_size_cut) {
  done:
    return u8_buf_12bit_encoded_transform_inplace_scalar(
        &src_buf[src_size], src_size_cut, transform_fn_scalar);
  }

  return CL_SUCCESS;
}

int u8_buf_12bit_encoded_transform_inplace_avx2(
    uint8_t *src_buf, size_t src_size, __m256i (*transform_fn_avx)(__m256i),
    void (*transform_fn_scalar)(uint16_t[8])) {
  return u8_buf_12bit_encoded_transform_inplace_avx2_inline(
      src_buf, src_size, transform_fn_avx, transform_fn_scalar);
}

/**
 * credits:
 * https://old.reddit.com/r/simd/comments/b3k1oa/looking_for_sseavx_bitscan_discussions/ej3i3aq/
 **/
static inline __m256i _mm256_bsr_epi16(__m256i __h) {
  const __m256i __lut_lo = _mm256_set_epi8(
      11, 11, 11, 11, 11, 11, 11, 11, 10, 10, 10, 10, 9, 9, 8, 0, 11, 11, 11,
      11, 11, 11, 11, 11, 10, 10, 10, 10, 9, 9, 8, 0);
  const __m256i __lut_hi = _mm256_set_epi8(
      15, 15, 15, 15, 15, 15, 15, 15, 14, 14, 14, 14, 13, 13, 12, 0, 15, 15, 15,
      15, 15, 15, 15, 15, 14, 14, 14, 14, 13, 13, 12, 0);
  const __m256i __nibble_mask = _mm256_set1_epi8(0x0F);
  const __m256i __adj = _mm256_set1_epi16(0x1F08);

  __h = _mm256_max_epu8(
      _mm256_shuffle_epi8(
          __lut_hi, _mm256_and_si256(_mm256_srli_epi16(__h, 4), __nibble_mask)),
      _mm256_shuffle_epi8(__lut_lo, _mm256_and_si256(__nibble_mask, __h)));

  return _mm256_max_epi8(_mm256_sub_epi8(__h, __adj),
                         _mm256_srli_epi16(__h, 8));
}

static inline __m256i _mm256_srlv_epi16x(__m256i __a, __m256i __count) {
  const __m256i __al =
      _mm256_srlv_epi32(_mm256_cvtepu16_epi32(_mm256_castsi256_si128(__a)),
                        _mm256_cvtepu16_epi32(_mm256_castsi256_si128(__count)));
  const __m256i __ah = _mm256_srlv_epi32(
      _mm256_cvtepu16_epi32(_mm256_extracti128_si256(__a, 1)),
      _mm256_cvtepu16_epi32(_mm256_extracti128_si256(__count, 1)));
  return _mm256_inserti128_si256(
      _mm256_castsi128_si256(_mm_packus_epi32(
          _mm256_castsi256_si128(__al), _mm256_extracti128_si256(__al, 1))),
      _mm_packus_epi32(_mm256_castsi256_si128(__ah),
                       _mm256_extracti128_si256(__ah, 1)),
      1);
}

#define _mm256_cmpgt_epu16(a, b)                                               \
  _mm256_cmpgt_epi16(_mm256_xor_si256(a, _mm256_set1_epi16(0x8000)),           \
                     _mm256_xor_si256(b, _mm256_set1_epi16(0x8000)))

static inline __m256i to_log_encoded_12bit_avx2_inline(__m256i __p) {
  __p = _mm256_slli_epi16(__p, 4);
  const __m256i __q =
      _mm256_sub_epi16(_mm256_bsr_epi16(__p), _mm256_set1_epi16(9));
  return _mm256_blendv_epi8(
      __p,
      _mm256_add_epi16(_mm256_slli_epi16(__q, 9), _mm256_srlv_epi16x(__p, __q)),
      _mm256_cmpgt_epu16(__p, _mm256_set1_epi16(1023)));
}

int u8_buf_12bit_encoded_to_log_encoded_12bit_avx2(uint8_t *src_buf,
                                                   const size_t src_size) {
  return u8_buf_12bit_encoded_transform_inplace_avx2_inline(
      src_buf, src_size, to_log_encoded_12bit_avx2_inline,
      to_log_encoded_12bit_inline);
}

#endif
