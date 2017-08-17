/**
 * @file
 * @brief main.c. What else?
 * @author Paul Bailey
 * @date
 */
#include "struct.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SETUP_FILENAME         ".v120_tui"
#define HOME_SETUP_FILENAME    "~/" SETUP_FILENAME
#define BUILTIN_SETUP_FILENAME RCDATADIR SETUP_FILENAME

struct global_t gbl;

/*
 * Open all crates connected to this PC, and add the V120 handles to the
 * global crate array.  The array will store V120_HANDLES for crates that
 * are connected, and NULL for crates that are not connected.
 */
static void openallcrates(void)
{
        V120_HANDLE *p;
        int i;

        for (i = 0; i < 16; ++i) {
                p = v120_open(i);
                if (p != NULL)
                        ctrlnew(p, i);

                gbl.g_v120[i] = p;
        }
}

/*
 * Add a path name to the global list of search paths.  Returns 0 if
 * added successfully, -1 if no more memory.
 */
static int pathadd2list(char *pathname)
{
        struct pathlist_t *p;

        p = malloc(sizeof(*p));
        if (p == NULL)
                return -1;

        p->p_path  = pathname;
        p->p_next  = gbl.g_path;
        gbl.g_path = p;
        return 0;
}

/* XXX: This should be in a helper library, not in main.c */
static char UNK_HOME[] = { ".\0" };

/* Initialize the default paths */
static void pathconfig(void)
{
        char *s;

        /* Get home path */
        s = getenv("HOME");
        gbl.g_home = malloc(strlen(s) + 1);
        if (gbl.g_home == NULL || s == NULL)
                gbl.g_home = UNK_HOME;
        else
                strcpy(gbl.g_home, s);

        /*
         * First path is home. If there is a .v120_tui file in the home
         * path, then other paths can be added. We malloc it again
         * so we don't free gbl.g_home during cleanup.
         */
        s = malloc(strlen(gbl.g_home) + 1);
        if (s != NULL) {
                strcpy(s, gbl.g_home);
                if (pathadd2list(s))
                        free(s);
        }

        /*
         * $(prefix)/v120_tui/something somthig
         */
        s = malloc(strlen(RCDATADIR) + 1);
        if (s != NULL) {
                strcpy(s, RCDATADIR);
                if (pathadd2list(s) != 0)
                        free(s);
        }

        /*
         * Next path is .v120_tui.d by default. Check if if exists, then add
         * it to the global path list.
         */
        s = getfullpath("~/.v120_tui.d");
        if (s != NULL) {
                if (pathadd2list(s) != 0)
                        free(s);
        }
        /* Remainder of paths can be parsed from .v120_tui file */
}

/**
 * @brief Add a dummy VME browser.
 *
 * This is for debugging v120_tui without having a V120 connected.
 */
void add_dummy(void)
{
        VME_REGION *region;
        struct buffer_t *res;

        region = malloc(sizeof(*region));
        if (region == NULL )
                return;

        region->len  = 32768;
        region->base = calloc(32768, 2);
        region->vme_addr = 0;
        if (region->base == NULL)
                goto err_base;

        res = browsernew("dummy VME", region, NULL, -1);
        if (res == NULL)
                goto err_browser;

        buffershuffle();
        return;

err_browser:
        free(region->base);
err_base:
        free(region);
}

/**
 * @brief Load and execute a v120_tui script.
 * @warning Do not call this unless the global buffer list
 * has been initialized.
 * @param fname Name of the file. The path can be relative or
 * absolute.
 * @param pbuf Buffer on which this file acts.
 * @return zero if the file loaded successfully, a negative error
 * if not.
 */
int load_file(const char *fname, struct buffer_t *pbuf)
{
        FILE *fp, *fpsave;
        char *fullpath;
        int res;

        fullpath = getfullpath(fname);
        if (fullpath == NULL)
                return -1;

        fp = fopen(fullpath, "r");
        free(fullpath);

        if (fp == NULL)
                return -1;

        /*
         * push global instream onto the stack, in case we are
         * recursively parsing files
         */
        fpsave = gbl.g_instream;
        gbl.g_instream = fp;

        /*
         * Ditto with the tokens. There is a static stack for this.
         * Here is the only place we do this.
         */
        res = tokenpush();
        if (res != 0)
                return -1;

        while (!feof(fp)) {
                token_start();
                inpcommand(pbuf);
                token_end();
        }

        /* Undo our push operations */
        tokenpop();
        gbl.g_instream = fpsave;
        fclose(fp);
        return 0;
}

/**
 * add_vme - Get access to a new VME card and add it to global VME
 *           browser list.
 * @crateid:    ID of crate, 0-16
 * @vmeaddr:    Address in VME
 * @vmelen:     Length of VME space to add, in bytes.
 * @rnmfile:    RNM file for this.  If rnmfile is NULL, default card name
 *              and settings will be used.
 */
void add_vme(int crateid, int vmeaddr, int vmelen, const char *rnmfile)
{
        VME_REGION *region;
        VME_REGION *pregion;
        struct rnm_t *rnm;
        int v;
        struct buffer_t *b;
        V120_HANDLE *h;

        if ((h = gbl.g_v120[crateid]) == NULL) {
                mbwarn("Crate %d not found", crateid);
                return;
        }

        if ((region = malloc(sizeof(*region))) == NULL)
                return;

        region->vme_addr   = vmeaddr;
        region->start_page = 0;
        region->len        = vmelen;
        region->config = V120_SFAST | V120_A16 | V120_EAUTO | V120_D16;
        region->tag = "";

        if ((pregion = v120_add_vme_region(h, region)) == NULL)
                return;

        if ((v = v120_allocate_vme(h, region->start_page)) < 0 )
                return;

        if (rnmfile != NULL)
                rnm = rnmparse(rnmfile);
        else
                rnm = NULL;

        b = browsernew("loaded VME", region, rnm, 1);
        if (b != NULL)
                buffershuffle();

        /*
         * FIXME: Should have a free(region) here is case of errors
         * above, but that currently may not be called until
         * v120_delete_vme_list(), which may not happen unless I remove
         * this V120 from the buffer list.
         */
}

static int gui_main(void)
{
        vmebr_start_application(NULL, NULL);
        return EXIT_FAILURE;
}

static int tty_main(void)
{
        while (1) {
                ttsize();

                /*
                 * FIXME: These should not need to be here, but some bug
                 * is messing things up if we do not do this every pass
                 * of the loop
                 */
                tteeop();
                ttplain();
                titleprint(titlename());

                splash();

                /* FIXME: ditto */
                /* Move this to whatever function switches mode */
                modeline();

                ttflush();

                /*
                 * Bulk of the work here. Determine if we're switching
                 * buffers, moving cursors, executing a command, running
                 * a script, and more.
                 */
                processinput(ttgetc());
        }

        return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
        const char *termtype;

        /* TODO: parse command line options */
        /* First, make sure gbl has initial values */
        gbl.g_buffer  = NULL;
        gbl.g_lastbuf = NULL;
        gbl.g_headbuf = NULL;
        gbl.g_path    = NULL;
        gbl.g_rnms    = NULL;
        gbl.g_instream = stdin;
        gbl.g_isgui   = 0;
        memset(gbl.g_v120, 0, sizeof(gbl.g_v120));

        /* Make sure we are in fact a terminal */
        termtype = (char *)getenv("TERM");
        if (termtype == NULL || strlen(termtype) == 0 || !isatty(0)) {
                gbl.g_isgui = 1;
                notagui();
        }

        pathconfig();
        /* Parse args, figure out environment */

        /* Initialize screen */
        ttstart();
        ttsize();

        /*
         * Open all available crates and add them to the buffer list.
         * This may become a dynamic option in the future.
         * Load V120 RNM file, V120 functions, etc. These should be built
         * in, since this is specifically for a V120 interface.
         */
        openallcrates();

        /* If graphical (yeah, right), initialize that */

        /* If not graphical, then... */
        titleprint("Initialization");

        bufferlist_start();
        if (gbl.g_buffer == NULL) {
                ttexit();
                /* TODO: Other cleanup */
                fprintf(stderr, "%s: Cannot allocate basic buffers\n",
                        argv[0]);
                exit(1);
        } else {
                buffergoto(gbl.g_buffer);
        }
#if 0
        /* Load PREFIX "/share/v120_tui" data file, installed .rnm's, etc. */
        if (load_file(BUILTIN_SETUP_FILENAME, gbl.g_headbuf))
                mbwarn("Failed to load %s", BUILTIN_SETUP_FILENAME);
#endif
        /*
         * Check if input .v120_tui or .v120_tui, initialize that. If there
         * are any VME cards in the file or in files it instructs us to
         * load, then create VME regions for each of them.
         */
        if (load_file(HOME_SETUP_FILENAME, gbl.g_headbuf))
                mbwarn("Failed to load %s", HOME_SETUP_FILENAME);

        return gbl.g_isgui ? gui_main() : tty_main();
}
