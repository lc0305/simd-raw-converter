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

#include "timer.h"

#define NANOSECONDS_IN_ONE_SECOND 1000000000

high_resolution_timer high_resolution_time() {
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC, &start);
  return (high_resolution_timer){
      .start_time = start,
  };
}

void restart_high_resolution_timer(high_resolution_timer *timer) {
  clock_gettime(CLOCK_MONOTONIC, &timer->start_time);
}

static struct timespec diff(struct timespec start, struct timespec end) {
  struct timespec temp;
  if ((end.tv_nsec - start.tv_nsec) < 0) {
    temp.tv_sec = end.tv_sec - start.tv_sec - 1;
    temp.tv_nsec = NANOSECONDS_IN_ONE_SECOND + end.tv_nsec - start.tv_nsec;
  } else {
    temp.tv_sec = end.tv_sec - start.tv_sec;
    temp.tv_nsec = end.tv_nsec - start.tv_nsec;
  }
  return temp;
}

static inline long timespec_to_total_nanoseconds(struct timespec timespec) {
  assert((LONG_MAX / NANOSECONDS_IN_ONE_SECOND) > timespec.tv_sec);
  return timespec.tv_sec * NANOSECONDS_IN_ONE_SECOND + timespec.tv_nsec;
}

long elapsed_high_resolution_time_nanoseconds(
    const high_resolution_timer *timer) {
  struct timespec end;
  clock_gettime(CLOCK_MONOTONIC, &end);
  return timespec_to_total_nanoseconds(diff(timer->start_time, end));
}