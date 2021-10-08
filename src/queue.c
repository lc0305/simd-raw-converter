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

#include "queue.h"

#define BUF_INIT 8

int lf_ow_queue_init(lf_ow_queue_t *q) {
  const char **buf = (const char **)calloc(BUF_INIT, sizeof(const char **));
  if (buf == NULL)
    return -1;
  q->__capacity = BUF_INIT;
  atomic_store_explicit(&q->__length, 0, memory_order_relaxed);
  q->__buf = buf;
  return 0;
}

bool lf_ow_queue_is_init(lf_ow_queue_t *q) {
  return q->__buf != NULL;
}

int lf_ow_queue_push(lf_ow_queue_t *q, const char *str) {
  const ssize_t length =
      atomic_load_explicit(&q->__length, memory_order_relaxed);
  if (length == q->__capacity) {
    const char **buf =
        realloc(q->__buf, sizeof(const char **) * (q->__capacity <<= 1));
    if (buf == NULL) {
      free(q->__buf);
      q->__buf = NULL;
      return -1;
    }
    memset(&buf[length], 0, sizeof(const char **) * length);
    q->__buf = buf;
  }
  q->__buf[length] = str;
  const ssize_t new_length = length + 1;
  q->__initial_length = new_length;
  atomic_store_explicit(&q->__length, new_length, memory_order_relaxed);
  return 0;
}

const char *lf_ow_queue_pop(lf_ow_queue_t *q) {
  const ssize_t current_length =
      atomic_fetch_sub_explicit(&q->__length, 1, memory_order_relaxed);
  if (current_length <= 0)
    return NULL;
  return q->__buf[q->__initial_length - current_length];
}

void lf_ow_queue_free_elements(lf_ow_queue_t *q) {
  for (ssize_t i = 0; i < q->__initial_length; ++i) {
    free((void *)q->__buf[i]);
  }
}

void lf_ow_queue_free(lf_ow_queue_t *q) {
  free(q->__buf);
  q->__buf = NULL;
  atomic_store_explicit(&q->__length, 0, memory_order_relaxed);
  q->__capacity = 0;
}

float lf_ow_queue_percentage(lf_ow_queue_t *q) {
  const ssize_t current_length =
      atomic_load_explicit(&q->__length, memory_order_relaxed);
  if (current_length <= 0)
    return 100.0f;
  const ssize_t elements_popped = q->__initial_length - current_length;
  return (((float)elements_popped) / ((float)q->__initial_length)) * 100.0f;
}
