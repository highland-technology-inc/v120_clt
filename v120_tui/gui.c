/*
 * We were trying to build a gtk-based version, but it was too much
 * work and gtk's API kept changing on us.
 *
 * Now if the user tries to open the GUI we just tell them to go to hell.
 */

#include "struct.h"

/*
 * For now, just jump into whatever new buffer we have.  Eventually
 * we will have a much cleaner interface, and the window will be an
 * alternative to the tty.
 */
int vmebr_start_application(uint16_t *pregs, struct rnm_t *prnm)
{
        gbl.g_isgui = 0;
        return 0;
}


void notagui(void)
{
        quit();
}
