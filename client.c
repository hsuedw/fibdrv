#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_INPUT "/sys/kernel/fibonacci/input"
#define FIB_OUTPUT "/sys/kernel/fibonacci/output"
#define FIB_TIME "/sys/kernel/fibonacci/time"
#define FIB_INPUT_LEN (32)
#define FIB_OUTPUT_LEN (256 * sizeof(uint32_t) * 2 + 1)
#define FIB_TIME_LEN (sizeof(uint64_t) * 2 + 1)
#define FIB_LOWER_BOUND (0)
#define FIB_UPPER_BOUND (23000)
#define TMP_BUF_LEN (40960)

int main(int argc, char **argv)
{
    if (argc != 4) {
        printf("usage: client data_path fib_time_filename rd_time_filename\n");
        return -1;
    }

    size_t fib_time_fn_len = strlen(argv[1]) + strlen(argv[2]) + 2;
    size_t rd_time_fn_len = strlen(argv[1]) + strlen(argv[3]) + 2;
    char *fib_time_file = (char *) malloc(fib_time_fn_len);
    char *rd_time_file = (char *) malloc(rd_time_fn_len);
    snprintf(fib_time_file, fib_time_fn_len, "%s/%s", argv[1], argv[2]);
    snprintf(rd_time_file, rd_time_fn_len, "%s/%s", argv[1], argv[3]);

    char *tmp_buf = (char *) malloc(TMP_BUF_LEN);
    for (int k = FIB_LOWER_BOUND; k <= FIB_UPPER_BOUND; ++k) {
        // The input of the Fibonacci driver is a decimal number.
        char fib_input[FIB_INPUT_LEN];
        int fd_in = open(FIB_INPUT, O_RDWR);
        snprintf(fib_input, FIB_INPUT_LEN, "%d", k);
        write(fd_in, fib_input, strlen(fib_input));
        close(fd_in);

        struct timespec t1, t2;
        size_t rd_len, out_len = 0;
        // Read a Fibonacci number from linux kernel.
        // The output of the Fibonacci driver is a hexdecimal number.
        char fib_output[FIB_OUTPUT_LEN];
        int fd_out = open(FIB_OUTPUT, O_RDONLY);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        while (rd_len = read(fd_out, fib_output + out_len,
                             FIB_OUTPUT_LEN - out_len)) {
            out_len += rd_len;
        }
        clock_gettime(CLOCK_MONOTONIC, &t2);
        fib_output[FIB_OUTPUT_LEN - 1] = '\0';
        close(fd_out);
        // printf("<<%s>>, %lu\n", fib_output, out_len);  // debug

        // Read the time cost for calculating a Fibonacci number
        // from Linux kernel.
        // The time cost of calculating a Fibonacci number
        // is represented in decimal.
        char fib_time[FIB_TIME_LEN];
        int fd_time = open(FIB_TIME, O_RDONLY);
        size_t time_len = read(fd_time, fib_time, FIB_TIME_LEN);
        fib_time[time_len] = '\0';
        close(fd_time);

        // Record the time cost of calculating a Fibonacci number.
        snprintf(tmp_buf, TMP_BUF_LEN, "%d %s", k, fib_time);
        int fd_fib_time = open(fib_time_file, O_CREAT | O_APPEND | O_WRONLY,
                               S_IRUSR | S_IRGRP | S_IROTH);
        write(fd_fib_time, tmp_buf, strlen(tmp_buf));
        close(fd_fib_time);
        // printf("{{%s}}, %lu\n", fib_time, time_len);  // debug

        // Record the time for reading a Fibonacci number.
        uint64_t rd_time = (uint64_t)((t2.tv_sec * 1e9 + t2.tv_nsec) -
                                      (t1.tv_sec * 1e9 + t1.tv_nsec));
        snprintf(tmp_buf, TMP_BUF_LEN, "%d %lu\n", k, rd_time);
        int fd_rd_time = open(rd_time_file, O_CREAT | O_APPEND | O_WRONLY,
                              S_IRUSR | S_IRGRP | S_IROTH);
        write(fd_rd_time, tmp_buf, strlen(tmp_buf));
        close(fd_rd_time);
        // printf("[[%s]]\n", tmp_buf);  // debug
    }

    free(fib_time_file);
    free(rd_time_file);
    free(tmp_buf);

    return 0;
}
