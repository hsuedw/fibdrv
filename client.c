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
#define FIB_UPPER_BOUND (11000)

int main(int argc, char **argv)
{
    int fd_out, fd_in, fd_time;
    char fib_input[FIB_INPUT_LEN];
    char fib_output[FIB_OUTPUT_LEN];
    char fib_time[FIB_TIME_LEN];

    fd_in = open(FIB_INPUT, O_RDWR);
    fd_out = open(FIB_OUTPUT, O_RDONLY);
    fd_time = open(FIB_TIME, O_RDONLY);

    // The input of the Fibonacci driver is a decimal number.
    snprintf(fib_input, FIB_INPUT_LEN, "%d", 11000);
    write(fd_in, fib_input, strlen(fib_input));

    size_t out_len = 0;
    struct timespec t1, t2;

    for (int k = FIB_LOWER_BOUND; k <= FIB_UPPER_BOUND; ++k) {
        size_t rd_len;
        // The output of the Fibonacci driver is a hexdecimal number.
        clock_gettime(CLOCK_MONOTONIC, &t1);
        while (rd_len = read(fd_out, fib_output + out_len,
                             FIB_OUTPUT_LEN - out_len)) {
            out_len += rd_len;
            // printf(">>>> %lu, %lu\n", out_len, rd_len);
        }
        clock_gettime(CLOCK_MONOTONIC, &t2);
        fib_output[FIB_OUTPUT_LEN - 1] = '\0';
        // printf("<<%s>>, %lu\n", fib_output, out_len);

        // The time cost of calculate a Fibonacci number
        // is represented in hexdecimal.
        size_t time_len = read(fd_time, fib_time, FIB_TIME_LEN);
        fib_time[FIB_TIME_LEN - 1] = '\0';
        // printf("{{%s}}, %lu\n", fib_time, time_len);

        uint64_t rd_time = (uint64_t)((t2.tv_sec * 1e9 + t2.tv_nsec) -
                                      (t1.tv_sec * 1e9 + t1.tv_nsec));
        // printf("[[%lu]]\n", rd_time);
    }

    close(fd_in);
    close(fd_out);
    close(fd_time);

    return 0;
}
