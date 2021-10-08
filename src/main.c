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
#include "queue.h"
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define DELIMITER '\n'
#define OPTION_VERBOSE (1 << 0)
#define OPTION_STDIN (1 << 1)

static lf_ow_queue_t queue = {0};
static pthread_t *threads = NULL;
static int num_threads = 1;
static int return_code = 0;
static int options = 0;

static const char *shortopts = "hit:v";
static const struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"input", no_argument, NULL, 'i'},
    {"threads", required_argument, NULL, 't'},
    {"verbose", no_argument, NULL, 'v'},
    {NULL, 0, NULL, 0},
};

static const char *usage = "Usage: %s [--help (-h)] [--input (-i)] [--threads "
                           "(-t) <threads>] [--verbose (-v)]\n";

#define PRINT_SYS_ERR                                                          \
  if (errno) {                                                                 \
    fprintf(stderr, "System error: %s\n", strerror(errno));                    \
  }

static void *worker_thread(void *ctx) {
  lf_ow_queue_t *queue = (lf_ow_queue_t *)ctx;
  const char *file_path;
  while ((file_path = lf_ow_queue_pop(queue)) != NULL) {
    if (options & OPTION_VERBOSE)
      printf("* Start processing: %s\n", file_path);
    int ret;
    if ((ret = convert_file(file_path)) < 0) {
      switch (ret) {
      case C_ERR_SYS:
        fprintf(stderr, "A system error occurend while processing: %s\n",
                file_path);
        PRINT_SYS_ERR;
        break;
      case C_ERR_FILE_SIZE:
        fprintf(stderr, "Unable to process file: %s\n", file_path);
        break;
      default:
        fprintf(stderr, "An internal error occurend while processing: %s\n",
                file_path);
        const char *msg = c_error_message_from_return_code(ret);
        if (msg != NULL)
          fprintf(stderr, "Internal error: %s\n", msg);
      }
    }
    if (options & OPTION_VERBOSE) {
      printf("* Finished processing: %s\n", file_path);
      printf("* Finished: %f%%\n", lf_ow_queue_percentage(queue));
    }
  }
  return NULL;
}

#define FPATH_BUF_SIZE 256
#define RPATH_BUF_SIZE 4096

#define QUEUE_PUSH(value)                                                      \
  if (lf_ow_queue_push(&queue, (value)) != 0) {                                \
    fprintf(stderr, "Fatal error pushing into queue :(\n");                    \
    return_code = 1;                                                           \
    goto err;                                                                  \
  }

#define PATH_READY_ALLOC_AND_PUSH_QUEUE(fbuf, i_fbuf)                          \
  fbuf[i_fbuf++] = 0;                                                          \
  char *file_path = (char *)malloc(i_fbuf * sizeof(char));                     \
  if (file_path == NULL) {                                                     \
    fprintf(stderr, "Fatal calloc error :(\n");                                \
    return_code = 1;                                                           \
    goto err;                                                                  \
  }                                                                            \
  memcpy(file_path, fbuf, i_fbuf);                                             \
  QUEUE_PUSH((const char *)file_path);

int main(int argc, char *const *argv) {
  if (lf_ow_queue_init(&queue) != 0) {
    fprintf(stderr, "Fatal error initializing queue :(\n");
    return_code = 1;
    goto err;
  }

  int option_index = 0;
  int opt;
  while ((opt = getopt_long(argc, argv, shortopts, long_options,
                            &option_index)) != -1) {
    switch (opt) {
    case 'h':
      printf(usage, argv[0]);
      break;
    case 'i':
      options |= OPTION_STDIN;
      break;
    case 't':
      num_threads = atoi(optarg);
      break;
    case 'v':
      options |= OPTION_VERBOSE;
      printf("* VERBOSE option set\n");
      break;
    default:
      fprintf(stderr, usage, argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if (options & OPTION_STDIN) {
    if (options & OPTION_VERBOSE)
      printf("* Reading file paths from STDIN\n");
    char fbuf[FPATH_BUF_SIZE];
    char rbuf[RPATH_BUF_SIZE];
    ssize_t i_fbuf = 0;
    ssize_t nbytes;
    while ((nbytes = read(STDIN_FILENO, rbuf, RPATH_BUF_SIZE)) > 0) {
      for (ssize_t i_rbuf = 0; i_rbuf < nbytes && i_fbuf < FPATH_BUF_SIZE;
           ++i_rbuf) {
        if (rbuf[i_rbuf] == DELIMITER) {
          PATH_READY_ALLOC_AND_PUSH_QUEUE(fbuf, i_fbuf);
          i_fbuf = 0;
          continue;
        }
        fbuf[i_fbuf++] = rbuf[i_rbuf];
      }
      if (i_fbuf >= FPATH_BUF_SIZE) {
        fprintf(stderr, "File path is too large. Max file path size is 255 "
                        "characters (bytes).\n");
        return_code = 1;
        goto err;
      }
    }
    if (nbytes < 0) {
      return_code = 1;
      goto sys_err;
    }
    if (i_fbuf > 0) {
      PATH_READY_ALLOC_AND_PUSH_QUEUE(fbuf, i_fbuf);
    }
  } else if (optind < argc) {
    for (ssize_t i = optind; i < argc; ++i) {
      QUEUE_PUSH(argv[i]);
    }
  } else {
    fprintf(stderr, "Error no files to process.\n");
    fprintf(stderr, usage, argv[0]);
    return_code = 1;
    goto err;
  }

  if (num_threads > 1) {
    threads = malloc(sizeof(pthread_t) * num_threads);
    if (threads == NULL) {
      fprintf(stderr, "Fatal malloc error :(\n");
      return_code = 1;
      goto err;
    }
  }
  if (options & OPTION_VERBOSE)
    printf("* Starting %d worker threads\n", num_threads);
  for (int i = 1; i < num_threads; ++i) {
    pthread_t tid;
    if (pthread_create(&tid, NULL, worker_thread, &queue) != 0) {
      fprintf(stderr, "Fatal error while creating thread. Consider checking "
                      "[-t threads].\n");
      return_code = 1;
      goto err;
    }
    threads[i] = tid;
  }

  worker_thread(&queue);

  if (options & OPTION_VERBOSE)
    printf("* Joining %d worker threads\n", num_threads);
  for (int i = 1; i < num_threads; ++i) {
    if (pthread_join(threads[i], NULL) != 0) {
      fprintf(stderr, "Error while joining thread.\n");
      return_code = 2;
      goto err;
    }
  }

  if (options & OPTION_VERBOSE)
    printf("* Successfully joined %d worker threads. Shutting down. :)\n",
           num_threads);

err:
  if (threads != NULL) {
    free(threads);
    threads = NULL;
  }
  if (lf_ow_queue_is_init(&queue)) {
    if (options & OPTION_STDIN) {
      lf_ow_queue_free_elements(&queue);
    }
    lf_ow_queue_free(&queue);
  }
  return return_code;

sys_err:
  PRINT_SYS_ERR;
  goto err;
}
