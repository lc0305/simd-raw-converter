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

#ifndef __TIMER_H__
#define __TIMER_H__

#include <assert.h>
#include <limits.h>
#include <time.h>

typedef struct high_resolution_timer {
  struct timespec start_time;
} high_resolution_timer;

high_resolution_timer high_resolution_time();
void restart_high_resolution_timer(high_resolution_timer *timer);
long elapsed_high_resolution_time_nanoseconds(
    const high_resolution_timer *timer);

#endif