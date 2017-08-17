#include "struct.h"
#include <stdlib.h>

static void pathclean(void)
{
        /*
         * FIXME: Unclear code alert: This implicitly takes care of
         * gbl.g_home
         */
        struct pathlist_t *p, *q;

        for (p = gbl.g_path; p != NULL; p = q) {
                free(p->p_path);
                q = p->p_next;
                free(p);
        }
}

static void rnmclean(void)
{
        struct rnm_t *p, *q;

        p = gbl.g_rnms;
        while (p != NULL) {
                q = p->r_next;
                rnmfree(p);
                p = q;
        }
        gbl.g_rnms = NULL;
}

static void bufferclean(void)
{
        struct buffer_t *p, *q;

        p = gbl.g_headbuf;
        while (p != NULL) {
                q = p->b_next;
                /*
                 * This kills both the buffer and any private
                 * date allocated by the buffer.
                 */
                p->b_mode->m_quit(p);
                p = q;
        }
        gbl.g_headbuf = NULL;
        gbl.g_lastbuf = NULL;
        gbl.g_buffer  = NULL;
}

static void crateclean(void)
{
        V120_HANDLE **ph;

        for (ph = &gbl.g_v120[0]; ph < &gbl.g_v120[16]; ++ph) {
                if (*ph != NULL)
                        v120_close(*ph);
        }
}

/**
 * @brief Exit as gracefully as possible from program.
 */
void quit(void)
{
        pathclean();
        rnmclean();
        bufferclean();
        crateclean();
        ttexit();

        /* just in case ttexit() gave us the slip */
        printf("\e[0m\e[?25h\e[2J");
        fflush(stdout);

        exit(0);
}
