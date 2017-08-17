#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


void fslide(FILE *infile)
{
        int c;
        while (isspace(c = getc(infile)))
                ;
        if (!feof(infile))
                ungetc(c, infile);
}

void get_function_name(FILE *infile, char *buf, int n)
{
        int c;
        int count;
        char *s;

tryagain:

        s = buf;
        count = n - 1;
        while (!isspace(c = getc(infile)) && c != '(') {
                *s++ = c;
                --count;
                if (count == 0)
                        break;
        }
        *s = '\0';
        if (c == '(')
                return;

        fslide(infile);
        c = getc(infile);
        if (c != '(') {
                ungetc(c, infile);
                goto tryagain;
        }
}

/* TODO: This is dumb. Improve it, */
void copy_proto(FILE *infile, FILE *outfile)
{
        int c;

        printf("FUNCTION: ");
        while ((c = getc(infile)) != '(') {
                if (c == EOF)
                        goto err;
                putc(c, outfile);
        }
        putc(c, outfile);
        while ((c = getc(infile)) != ')') {
                if (c == EOF)
                        goto err;
                else if (c == '\t' || c == '\n' || c == '\r')
                        c = ' ';

                putc(c, outfile);
        }
        putc(c, outfile);
        putc(';', outfile);
        putc('\n', outfile);
        return;
err:
        fprintf(stderr, "Not a function prototype\n");
        exit(1);
}

void end_of_comment(FILE *infile)
{
        int c;
        do {
                while ((c = getc(infile)) != '*') {
                        if (c == EOF)
                                return;
                }
                if ((c = getc(infile)) == EOF)
                        return;
        } while (c != '/');
}

/* Copy comment from current position in file, not counting the
 * first whitespace. */
void copycomment(FILE *infile, FILE *outfile)
{
        int c;

        while (isblank(c = fgetc(infile)))
                ;
loop:
        while (c != '*') {
                putc(c, outfile);
                if (c == EOF)
                        return;

                c = fgetc(infile);
        }

        c = fgetc(infile);
        if (c != '/') {
                ungetc(c, infile);
                c = '*';
                goto loop;
        }
}

/* TODO: make outfile a scratchpad file for storing things like this */
void qcfunctiondoc(FILE *infile, FILE *outfile)
{
        int c;
        long pos;
        int line = 0;
        char buf[512];

        c = '\n';

        while (!feof(infile)) {
                c = getc(infile);
                if (c == '\n' || c == '\r') {
                        ++line;
                        continue;
                }
                if (c == '/') {
                        c = getc(infile);
                        if (c != '*')
                                continue;

                        c = getc(infile);
                        while (isblank(c))
                                c = getc(infile);

                        if (c != 'd')
                                continue;
                        if ((c = getc(infile)) != 'o')
                                continue;
                        if ((c = getc(infile)) != 'c')
                                continue;
                        if ((c = getc(infile)) != ':')
                                continue;
                        c = getc(infile);
                        while (isblank(c))
                                c = getc(infile);

                        ungetc(c, infile);
                        if (line != 0) {
                                pos = ftell(infile);
                                end_of_comment(infile);
                                fslide(infile);
                                fprintf(outfile, "\n***\n");
#if 0
                                copy_proto(infile, outfile);
#else
                                get_function_name(infile, buf, sizeof(buf));
                                fprintf(outfile, "%s\n", buf);
#endif
                                fseek(infile, pos, SEEK_SET);
                        }
                        copycomment(infile, outfile);
                        putc('\n', outfile);
                }
        }
}

int main(int argc, char **argv)
{
        FILE *infile;
        char *fname;

        if (argc != 2) {
                fprintf(stderr, "Usage: %s FILENAME\n", argv[0]);
                return 1;
        }

        fname = argv[1];
        infile = fopen(fname, "r");
        if (infile == NULL) {
                fprintf(stderr, "Cannot open %s\n", fname);
                return -1;
        }

        qcfunctiondoc(infile, stdout);

        return 0;
}


