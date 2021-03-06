#include "common.h"

static void NORETURN do_exit()
{
    // FIXME: do UnhookWindowsHookEx for low level keyboardhook if necessary
    
    try_goto_desktop();
    call_atexit_hooks();
    die(0);
}

MAKE_PATCHSET(terminateatexit)
{
    // replace PAL3::Destroy
    make_jmp(0x00406230, do_exit);
    
    // terminate after WinMain returns
    make_call(0x0055514A, do_exit);
}
