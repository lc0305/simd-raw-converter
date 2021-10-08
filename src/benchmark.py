#!/bin/python3

from os import system
from time import time_ns
from argparse import ArgumentParser
from functools import reduce


def mean(iter):
    return reduce(lambda a, b: a + b, iter) / len(iter)


def main():
    arg_parser = ArgumentParser()
    arg_parser.add_argument('-f', '--file', type=str, required=True)
    arg_parser.add_argument('-n', '--number', type=int, default=1)
    arg_parser.add_argument('-i', '--iterations', type=int, default=1)
    arg_parser.add_argument('-t', '--threads', type=int, default=1)
    arg_parser.add_argument('-v', '--verbose', type=bool, default=False)

    args = arg_parser.parse_args()

    splitfile = args.file.split('.')

    file_ext = splitfile.pop()
    file_name = ''.join(splitfile)

    all_files = []

    for i in range(0, args.number):
        cp_file = f'{file_name}{i}.{file_ext}'
        system(f'cp {args.file} {cp_file}')
        all_files.append(cp_file)

    exec_str = './raw_converter ' + \
        f' -t {args.threads}' + \
        (' -v' if args.verbose else '') + ' ' + ' '.join(all_files)

    start = []
    stop = []

    for i in range(0, args.iterations):
        start.append(time_ns())
        system(exec_str)
        stop.append(time_ns())

    system(f'rm {" ".join(all_files)}')

    print(f'Time: {mean(stop) - mean(start)}ns')


if __name__ == '__main__':
    main()
