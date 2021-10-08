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

#include "convert_file.h"

static const char *const error_messages[] = {
    "Success.", // 0
    ("System Error occured while converting the file. Check errno for further "
     "information."),                    // (-)1
    "Filesize does not fit the format.", // (-)2
};

#define ABS(val) (((val) >= 0) ? (val) : (-(val)))

const char *c_error_message_from_return_code(int return_code) {
  if (return_code > -100)
    return cl_error_message_from_return_code(return_code);
  const unsigned int index = ABS(return_code) - 100;
  if (index >= (sizeof(error_messages) / sizeof(char *)))
    return NULL;
  return error_messages[index];
}

#define FILE_HEADER_SIZE 512

int convert_file(const char *file_path) {
  int return_code = C_SUCCESS;

  int fd = open(file_path, O_RDWR);
  if (fd < 0) {
    return_code = C_ERR_SYS;
    goto err;
  }

  ssize_t file_size = 0;
  {
    struct stat file_stat;
    if (fstat(fd, &file_stat) < 0) {
      return_code = C_ERR_SYS;
      goto err_fd;
    }
    file_size = file_stat.st_size;
  }
  if (file_size <= FILE_HEADER_SIZE) {
    return_code = C_ERR_FILE_SIZE;
    goto err_fd;
  }

#ifdef __linux__
  const int mmap_flags = MAP_SHARED | MAP_POPULATE;
#else
  const int mmap_flags = MAP_SHARED;
#endif
  uint8_t *file_map = (uint8_t *)mmap(NULL, file_size, PROT_READ | PROT_WRITE,
                                      mmap_flags, fd, 0);
  if (file_map == MAP_FAILED) {
    return_code = C_ERR_SYS;
    goto err_fd;
  }

  if (madvise(file_map, file_size, MADV_SEQUENTIAL | MADV_WILLNEED) < 0) {
    return_code = C_ERR_SYS;
    goto err_map;
  }

  if ((return_code = u8_buf_12bit_encoded_to_log_encoded_12bit(
           &file_map[FILE_HEADER_SIZE], file_size - FILE_HEADER_SIZE)) < 0) {
    goto err_map;
  }

  if (msync(file_map, file_size, MS_SYNC) < 0) {
    return_code = C_ERR_SYS;
  }

err_map:
  if (munmap(file_map, file_size) < 0) {
    return_code = C_ERR_SYS;
  }

err_fd:
  if (close(fd) < 0) {
    return_code = C_ERR_SYS;
  }

err:
  return return_code;
}
