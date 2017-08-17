#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>

static void
usage(FILE *fp, const char *progname)
{
        static const char *susage =
                "try poll() on a V120 IRQ device and print its return value\n"
                "\n"
                "Options:\n"
                "     -m CRATE    Use CRATE instead of default 0\n"
                "     -w MS       Expire after MS milliseconds instead of default 12000\n"
                "     -?          Show this help\n"
                "\n"
                "The idea is to run this, and in another terminal force or fake an\n"
                "interrupt before this process times out, and see if this process\n"
                "returns as a result of the interrupt.\n"
                "\n"
                "This is a lower-level test than the v120irqd tests\n";

        fprintf(fp, "%s - %s\n", progname, susage);
}

static void
inval(const char *what, const char *arg)
{
        fprintf(stderr, "invalid %s '%s'\n", what, arg);
        exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
        int res;
        struct pollfd fds;
        int fd;
        char filename[64];
        int mswait;
        int opt;
        int crate;

        crate = 0;
        mswait = 12000;
        while ((opt = getopt(argc, argv, "m:w:?")) != -1) {
                char *endptr;
                switch (opt) {
                case 'm':
                        crate = strtoul(optarg, &endptr, 0);
                        if (errno || endptr == optarg || crate > 15)
                                inval("crate", optarg);
                        break;
                case 'w':
                        mswait = strtoul(optarg, &endptr, 0);
                        if (errno || endptr == optarg)
                                inval("mswait", optarg);
                        else if (mswait < 100)
                                mswait = 100;
                        break;
                case '?':
                        usage(stdout, argv[0]);
                        exit(EXIT_SUCCESS);
                        break;
                default:
                        usage(stderr, argv[0]);
                        exit(EXIT_FAILURE);
                        break;
                }
        }
        sprintf(filename, "/dev/v120_q%d", crate);

        fd = open(filename, O_RDONLY);
        if (fd < 0) {
                fprintf(stderr, "Cannot open %s\n", filename);
                exit(EXIT_FAILURE);
        }
        fds.fd = fd;
        fds.events = POLLIN;
        fds.revents = 0;

        res = poll(&fds, 1, mswait);
        printf("poll(%s)=%d\n", filename, res);
        close(fd);
        return 0;
}
