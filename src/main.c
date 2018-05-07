#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>
#include <dynamic_libs/os_functions.h>
#include <dynamic_libs/fs_functions.h>
#include <dynamic_libs/gx2_functions.h>
#include <dynamic_libs/sys_functions.h>
#include <dynamic_libs/vpad_functions.h>
#include <dynamic_libs/padscore_functions.h>
#include <dynamic_libs/socket_functions.h>
#include <dynamic_libs/ax_functions.h>
#include <libutils/fs/sd_fat_devoptab.h>
#include <libutils/system/memory.h>
#include <libutils/utils/utils.h>
#include <libutils/kernel/syscalls.h>

int sy = 0;

unsigned char *screenBuffer = (unsigned char *)0xF4000000;

void fillScreen(uint32_t rgba){
    OSScreenClearBufferEx(0, rgba);
    OSScreenClearBufferEx(1, rgba);
}   

void flipBuffers() {

    int buf0_size = OSScreenGetBufferSizeEx(0);
    int buf1_size = OSScreenGetBufferSizeEx(1);
    //Flush the cache
    DCFlushRange((void *)screenBuffer + buf0_size, buf1_size);
    DCFlushRange((void *)screenBuffer, buf0_size);
    //Flip the buffer
    OSScreenFlipBuffersEx(0);
    OSScreenFlipBuffersEx(1);
}

void _print(char *buf) {

    OSScreenPutFontEx(0, 0, sy, buf);
    OSScreenPutFontEx(1, 0, sy, buf);

    sy++;
    
}

void print_aff(int argc, void *argv) {

    char buf[255];
    int aff = OSGetThreadAffinity((OSThread*)argv)-1;

    if(aff==3) {
        aff = 2;
    }

    __os_snprintf(buf, 255, "On CPU%d | OSThread: %p | Name: %s", aff, (void*)argv, OSGetThreadName((OSThread*)argv));

    if(aff == 0x00) {
        OSScreenPutFontEx(1, 0, 0, buf);
    } else if (aff == 0x01) {
        OSScreenPutFontEx(1, 0, 1, buf);
    }
    else if (aff == 0x02) {
        OSScreenPutFontEx(1, 0, 2, buf);
    }
    
    ((void (*)())0x01041D6C)(); // OSExitThread()
}

/* Entry point */
int Menu_Main(void)
{
    //!*******************************************************************
    //!                   Initialize function pointers                   *
    //!*******************************************************************
    //! do OS (for acquire) and sockets first so we got logging
    InitOSFunctionPointers();
    InitSocketFunctionPointers();

    InitFSFunctionPointers();
    InitVPadFunctionPointers();

    //!*******************************************************************
    //!                    Initialize heap memory                        *
    //!*******************************************************************
    //! We don't need bucket and MEM1 memory so no need to initialize
    memoryInitialize();

    //!*******************************************************************
    //!                        Initialize FS                             *
    //!*******************************************************************
    mount_sd_fat("sd");

    VPADInit();

    OSScreenInit();                                                        
    int buf0_size = OSScreenGetBufferSizeEx(0); // TV Buffer Size                    

    OSScreenSetBufferEx(0, screenBuffer);   // 0
    OSScreenSetBufferEx(1, (screenBuffer + buf0_size)); // 0 + TV Buffer Size

    OSScreenEnableEx(0, 1); // Enable TV
    OSScreenEnableEx(1, 1); // Enable GamePad

    fillScreen(0x0000FFFF);  // Fill Screen Blue

    /*
        eAttributeAffCore0          = 0x01,
        eAttributeAffCore1          = 0x02,
        eAttributeAffCore2          = 0x04,
    */

    OSThread *thread_cpu0 = OSAllocFromSystem(sizeof(OSThread), 8); // Somewhat needed or it crashes
    OSThread *thread_cpu1 = OSAllocFromSystem(sizeof(OSThread), 8); // Somewhat needed or it crashes
    OSThread *thread_cpu2 = OSAllocFromSystem(sizeof(OSThread), 8); // Somewhat needed or it crashes

    uint32_t *stack0 = OSAllocFromSystem(0x300, 0x20);  //
    uint32_t *stack1 = OSAllocFromSystem(0x300, 0x20);  // Allocate stacks for each thread
    uint32_t *stack2 = OSAllocFromSystem(0x300, 0x20);  //

    bool ret0 = OSCreateThread(thread_cpu0, (void*)print_aff, 1, thread_cpu0, (u32)stack0+0x300, 0x300, 0, 0x01);   // Create a Core on CPU0 (1st bit set)
    bool ret1 = OSCreateThread(thread_cpu1, (void*)print_aff, 1, thread_cpu1, (u32)stack1+0x300, 0x300, 0, 0x02);   // Create a Core on CPU1 (2nd bit set)
    bool ret2 = OSCreateThread(thread_cpu2, (void*)print_aff, 1, thread_cpu2, (u32)stack2+0x300, 0x300, 0, 0x04);   // Create a Core on CPU2 (3rd bit set)

    if(!ret0 & !ret1 & !ret2) {
        OSFatal("Couldn't create Threads");
    }

    OSSetThreadName(thread_cpu0, "Thread CPU0");    //
    OSSetThreadName(thread_cpu1, "Thread CPU1");    //  Attribute thread names because it's cool lol  
    OSSetThreadName(thread_cpu2, "Thread CPU2");    //

    OSResumeThread(thread_cpu0);    //  Why usleep?
    os_usleep(100000);              //
    OSResumeThread(thread_cpu1);    //  Need to sleep because otherwise the text overlaps
    os_usleep(100000);              //  as there's no thread sync and some cores are faster than the others
    OSResumeThread(thread_cpu2);    //
    os_usleep(100000);              //  That's why.

    flipBuffers();

    s32 vpadError = -1;
    VPADData vpad;

    while(1)
    {
        VPADRead(0, &vpad, 1, &vpadError);

        if(vpadError == 0 && ((vpad.btns_d | vpad.btns_h) & VPAD_BUTTON_HOME))
            break;
    }

    unmount_sd_fat("sd");
    memoryRelease();

    return EXIT_SUCCESS;
}

