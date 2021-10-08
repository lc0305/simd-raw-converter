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

#ifndef __CONVERT_FILE_H__
#define __CONVERT_FILE_H__

#include "../lib/convert.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define C_SUCCESS 0
#define C_ERR_SYS -101
#define C_ERR_FILE_SIZE -102

const char *c_error_message_from_return_code(int return_code);

/**
 * IMPORTANT: on C_ERR_SYS check errno
 **/
int convert_file(const char *file_path);

#endif
