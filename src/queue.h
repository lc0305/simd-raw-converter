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

#ifndef __LOCK_FREE_ONE_WAY_QUEUE_H__
#define __LOCK_FREE_ONE_WAY_QUEUE_H__

#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

typedef struct lf_ow_queue {
  ssize_t __initial_length;
  _Atomic ssize_t __length;
  ssize_t __capacity;
  const char **__buf;
} lf_ow_queue_t;

int lf_ow_queue_init(lf_ow_queue_t *q);
bool lf_ow_queue_is_init(lf_ow_queue_t *q);
int lf_ow_queue_push(lf_ow_queue_t *q, const char *str);
const char *lf_ow_queue_pop(lf_ow_queue_t *q);
void lf_ow_queue_free_elements(lf_ow_queue_t *q);
void lf_ow_queue_free(lf_ow_queue_t *q);
float lf_ow_queue_percentage(lf_ow_queue_t *q);

#endif
