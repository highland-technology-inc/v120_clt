/**
 * @file
 * @brief RNM file parser.
 * @author Paul Bailey
 * @date
 */
#include "struct.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define RNM_FILE_BUFSIZE        256
#define RNM_MAX_TOKENS          128
#define PATH_MAX                512
#define NIL_HASH                (-1LL)

static int add2hash(struct rnm_t *rnm, char *name, int index)
{
        hash_t hash = hashstring(name);

        struct rnm_hash_t *rh = &rnm->r_hash[hash % RNM_HASH_SIZE];

        if (rh->rh_hash == NIL_HASH) {
                goto put;
        } else {
                while (rh->rh_next != NULL)
                        rh = rh->rh_next;

                rh->rh_next = malloc(sizeof(*rh->rh_next));
                if (rh->rh_next == NULL)
                        return -1;

                goto put;
        }
put:
        rh->rh_name = name;
        rh->rh_hash = hash;
        rh->rh_next = NULL;
        rh->rh_idx = index;
        return 0;
}

/* helper to hashclear() */
static void hashclear1(struct rnm_hash_t *rh)
{
        rh->rh_hash = NIL_HASH;
        rh->rh_name = NULL;
        rh->rh_next = NULL;
}

/*
 * Not the same as hashfree(). This is done when a new hash table has
 * been allocated.
 *    rh is a pointer to the entire hash table
 */
static void hashclear(struct rnm_hash_t rh[])
{
        struct rnm_hash_t *r, *rend;
        rend = &rh[RNM_HASH_SIZE];
        for (r = &rh[0]; r < rend; ++r)
                hashclear1(r);
}

/*
 * Free a hash table entry's collision list, if it exists.
 */
static void hashfree1(struct rnm_hash_t *rh)
{
        struct rnm_hash_t *q, *r;
        r = rh;
        q = r->rh_next;
        while (q != NULL) {
                r = q;
                q = r->rh_next;
                free(r);
        }
}

/*
 * Exit call. This frees an rnm hash table's collision lists. The hash
 * table itself is not freed: it was malloc'd as a nested part of struct
 * rnm_t.
 */
static void hashfree(struct rnm_hash_t rh[])
{
        struct rnm_hash_t *r, *rend;
        rend = &rh[RNM_HASH_SIZE];
        for (r = &rh[0]; r < rend; ++r)
                hashfree1(r);
}

static int linedata(struct rnm_t *rnm, int ntok, char **tokens)
{
        struct vme_metadata_t *rdata;
        int toffs;
        int c;

        if (ntok == 0)
                return 0;

        c = toupper(tokens[0][0]);
        switch (c) {
        case 'W':
                if (ntok < 2) {
                        mbwarn("Expected: value after global data width 'W'");
                        return -1;
                }
                rnm->r_dwidth = strtol(tokens[1], NULL, 0);
                if (rnm->r_dwidth != 2 && rnm->r_dwidth != 4) {
                        mbwarn("Unsupported global data width: %s", tokens[1]);
                        return -1;
                }
                break;
        case 'E':
                if (ntok < 2) {
                        mbwarn("Expected: value after endianness 'E'");
                        return -1;
                }
                c = toupper(tokens[1][0]);

                switch (c) {
                case 'A':
                        rnm->r_endi = V120_EAUTO;
                        break;
                case 'L':
                        rnm->r_endi = V120_ELONG;
                        break;
                case 'B':
                        rnm->r_endi = V120_EBYTE;
                        break;
                case 'S':
                        rnm->r_endi = V120_ESHORT;
                        break;
                default:
                        mbwarn("Unsupported endiannes: %s", tokens[1]);
                        return -1;
                }
                break;
        case 'A':
                if (ntok < 2) {
                        mbwarn("Expected: value after global address width 'A'");
                        return -1;
                }
                rnm->r_awidth = strtol(tokens[1], NULL, 0);
                break;
        default:
                if (!isdigit(c)) {
                        mbwarn("Unexpected token at start of line: %s", tokens[0]);
                        return -1;
                }
                toffs = strtoul(tokens[0], NULL, 0);
                rdata = &rnm->r_mdata[toffs];

                /* Establish defaults in case user did not provide a
                 * value */
                rdata->sign = 1;
                memset(rdata->name, 0, sizeof(rdata->name));
                rdata->dwidth = 2;
                /* Get name */
                if (ntok < 2) {
                        /*
                         * No reg info. This is not an error; a user may
                         * just wish to mark the maximum address space or
                         * something silly like that.
                         */
                        break;
                }
                tokens[1] = linestrip(tokens[1]);
                strncpy(rdata->name, tokens[1], sizeof(rdata->name));
                rdata->name[sizeof(rdata->name) - 1] = '\0';

                /* Add name to hash table */
                add2hash(rnm, rdata->name, toffs);

                /* Get sign */
                if (ntok < 3)
                        break;

                c = toupper(tokens[2][0]);
                if (c == 'S') {
                        rdata->sign = -1;
                } else if (c != 'U') {
                        mbwarn("Expected: 'U' or 'S' for offs %d", toffs);
                        return -1;
                }
                /* Get data width */
                if (ntok < 4)
                        break;

                switch (tokens[3][0]) {
                case '2':
                        break; /* already set above */
                case '4':
                        rdata->dwidth = 4;
                        break;
                default:
                        mbwarn("Expected: '2' or '4' datawidth for offs %d",
                               toffs);
                        return -1;
                }
                break;
        }
        return 0;
}

static int filldata(struct rnm_t *rnm, FILE *fp, int maxOffs)
{
        char line[ RNM_FILE_BUFSIZE ];
        char *tokens[ RNM_MAX_TOKENS ];
        int ntok;

        /* Some defaults, in case these values are not provided */
        rnm->r_dwidth = 2;
        rnm->r_endi = V120_EAUTO;
        rnm->r_awidth = 16;

        while (!feof(fp)) {
                char *s;
                char *context = NULL;
                int ret;

                s = fgets(line, RNM_FILE_BUFSIZE, fp);
                if (s == NULL)
                        break;

                s = linestrip(s);

                /* Tokenize line */
                for (s = strtok_r(s, ":", &context), ntok = 0;
                     s != NULL && ntok < RNM_MAX_TOKENS;
                     s = strtok_r(NULL, ":", &context), ++ntok) {
                        tokens[ ntok ] = s;
                }

                if (ntok == 0)
                        continue;

                ret = linedata(rnm, ntok, tokens);
                if (ret != 0)
                        return ret;
        }
        return 0;
}


char *slide(char *s)
{
        while (isblank(*s))
                ++s;
        return s;
}

/* Remove leading and trailing whitespace, and comments */
char *linestrip(char *s)
{
        char *ssave;
        s = slide(s);

        ssave = s;
        while (!vmebriseol(*s))
                ++s;

        do {
                *s = '\0';
                --s;
        } while (isblank(*s) && s >= ssave);

        return ssave;
}

static int maxoffs(FILE *fp)
{
        char line[RNM_FILE_BUFSIZE];
        int offs = 0;

        while (!feof(fp)) {
                char *s;
                int toffs;

                s = fgets(line, RNM_FILE_BUFSIZE, fp);
                if (s == NULL)
                        break;

                s = linestrip(s);
                if (*s < '0' || *s > '9')
                        continue;

                toffs = strtoul(s, NULL, 0);
                if (toffs > offs)
                        offs = toffs;
        }
        rewind(fp);
        return offs;
}

static void rnmadd2path(struct rnm_t *rnm)
{
        rnm->r_next = gbl.g_rnms;
        gbl.g_rnms = rnm->r_next;
}

/*
 * Returns RNM struct if it is already cached, or NULL if it is not.
 */
static struct rnm_t *findrnm(const char *path)
{
        struct rnm_t *ret = gbl.g_rnms;
        while (ret != NULL) {
                /*
                 * We check full paths, because someone may want different
                 * releases of RNM for different slots containing diefferent
                 * releases of ... same ... card. This way user can have
                 * two RNM files in different paths with same name.
                 */
                if (!strcmp(ret->r_path, path))
                        break;

                ret = ret->r_next;
        }
        return ret;
}

/**
 * @brief Parse an RNM file and allocate RNM metadata.
 *
 * @param filename Name of the RNM file to parse.
 *
 * @return Pointer to RNM metadata parsed from the file, or NULL
 * if the parsing failed.
 *
 * @par Side effect:
 *
 * mbwarn() is used to specify nature of failure; the most likely
 * failure is a syntax error in the RNM file, so a simple use of
 * errno is insufficient.
 */
struct rnm_t *rnmparse(const char *filename)
{
        FILE *fp;
        struct rnm_t *ret = NULL;
        char *fnm;  /* Full path of filename */
        struct vme_metadata_t *rdata;
        unsigned long offs;


        if (filename == NULL)
                return NULL;

        if ((fnm = getfullpath(filename)) == NULL) {
                mbwarn("Cannot determine absolute path");
                goto err_fnm;
        }

        if ((ret = findrnm(fnm)) != NULL) {
               /* We already have it. Return early. */
               free(fnm);
               return ret;
        }

        /* Still here? Roll up sleeves, do some parsing */
        if ((fp = fopen(fnm, "r")) == NULL) {
                mbwarn("Cannot open file: %s\n", fnm);
                goto err_fopen;
        }

        if ((offs = maxoffs(fp)) == 0) {
                mbwarn("Expected: At least one register offset\n");
                goto err_offs;
        }

        rdata = calloc(offs + 1, sizeof(*rdata));
        if (rdata == NULL) {
                mbwarn("Not enough memory\n");
                goto err_rdata;
        }

        if ((ret = calloc(1, sizeof(*ret))) == NULL) {
                mbwarn("Not enough memory\n");
                goto err_ret;
        }

        ret->r_len   = offs;
        ret->r_mdata = rdata;
        strncpy(ret->r_name, dirstrip(fnm), sizeof(ret->r_name) - 1);
        ret->r_name[sizeof(ret->r_name) - 1] = '\0';
        ret->r_path = fnm;

        hashclear(ret->r_hash);

        if (filldata(ret, fp, offs) != 0)
                goto err_filldata;


        rnmadd2path(ret);
        fclose(fp);
        return ret;

err_filldata:
        rnmfree(ret);
err_ret:
        free(rdata);
err_rdata:
err_offs:
        fclose(fp);
err_fopen:
        free(fnm);
err_fnm:
        return NULL;
}

/**
 * @brief Free any allocated memory associated with an RNM file's metadata.
 *
 * @param metadata Pointer to an RNM file's metadata. This pointer may no
 * longer be dereferenced after a call to this function.
 */
void rnmfree(struct rnm_t *metadata)
{
        hashfree(metadata->r_hash);
        free(metadata->r_mdata);
        if (metadata->r_path != NULL)
                free(metadata->r_path);

        free(metadata);
}

/**
 * @brief Get the register offset of a register, given its name
 *
 * @param rnm Pointer to the loaded RNM metadata
 * @param name Register name
 * @return A positive byte offset, or -1 if there was no match.
 */
int rnmsearch(struct rnm_t *rnm, const char *name)
{
        hash_t hash;
        struct rnm_hash_t *rh;

        hash = hashstring(name);
        rh = &rnm->r_hash[hash % RNM_HASH_SIZE];

        while (rh != NULL) {
                if (hash == rh->rh_hash) {
                        if (!strcmp(name, rh->rh_name))
                                return rh->rh_idx;
                }
                rh = rh->rh_next;
        }
        return -1;
}
