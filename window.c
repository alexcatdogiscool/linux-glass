#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>



int main() {

    Display* mainDisplay = XOpenDisplay(0);
    Window rootWindow = XDefaultRootWindow(mainDisplay);
    int defaultScreen = DefaultScreen(mainDisplay);
    
    unsigned int windowX = 0;
    unsigned int windowY = 0;
    unsigned int windowWidth = 500;
    unsigned int windowHeight = 500;
    unsigned int borderWidth = 0;
    unsigned int windowDepth = CopyFromParent;
    int windowClass = CopyFromParent;
    Visual* windowVisual = CopyFromParent;

    int attributeValueMask = CWBackPixel;
    XSetWindowAttributes windowAttributes = {};
    windowAttributes.background_pixel = 0xffff0000;

    Window mainWindow = XCreateWindow(mainDisplay, rootWindow,
                                      windowX, windowY, windowWidth, windowHeight,
                                      borderWidth, windowDepth, windowClass, windowVisual,
                                      attributeValueMask, &windowAttributes);

    XSelectInput(mainDisplay, mainWindow, ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(mainDisplay, mainWindow);

    GC gc = XCreateGC(mainDisplay, mainWindow, 0, NULL);

    int screenWidth = DisplayWidth(mainDisplay, defaultScreen);
    int screenHeight = DisplayHeight(mainDisplay, defaultScreen);
    

    XShmSegmentInfo shmInfo;
    XImage* img = XShmCreateImage(mainDisplay, DefaultVisual(mainDisplay, defaultScreen), DefaultDepth(mainDisplay, defaultScreen), ZPixmap, NULL, &shmInfo, screenWidth, screenHeight);
    
    shmInfo.shmid = shmget(IPC_PRIVATE, img->bytes_per_line * img->height, IPC_CREAT | 0777);
    shmInfo.shmaddr = (char*)shmat(shmInfo.shmid, 0, 0);

    img->data = shmInfo.shmaddr;

    shmInfo.readOnly = False;

    XShmAttach(mainDisplay, &shmInfo);

    

    for (;;) {

        XEvent generalEvent = {};
        XNextEvent(mainDisplay, &generalEvent);

        XGetGeometry(mainDisplay, mainWindow, &rootWindow, &windowX, &windowY, &windowWidth, &windowHeight, &borderWidth, &windowDepth);

        XShmGetImage(mainDisplay, rootWindow, img, 0, 0, AllPlanes);
        XPutImage(mainDisplay, mainWindow, gc, img, 0, 0, 0, 0, windowWidth, windowHeight);

        XFlush(mainDisplay);

    }


    return 0;


}