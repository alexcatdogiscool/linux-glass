#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <X11/extensions/XShm.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <X11/extensions/Xdamage.h>


typedef struct WinMeta {
    Window win;
    Pixmap pix;
    XRenderPictFormat *fmt;
    int x, y;
    unsigned int w, h;
} WinMeta;

typedef struct timing_t {
    struct timeval tv1, tv2;
    Bool whichState;
} timing_t;

typedef struct windowAttrs {
    XRenderPictFormat fmt;
    XWindowAttributes attr;
} windowAttrs;


void getStats(timing_t* timing) {
    // i want to see:
    // fps (and/or seconds per frame)
    // memory usage
    // ...

    long elapsed;

    if (timing->whichState) {//tv1 is old value(start), and tv2 is new(end)
        gettimeofday(&timing->tv2, NULL);
        timing->whichState = !timing->whichState;
    } else {
        gettimeofday(&timing->tv1, NULL);
        timing->whichState = !timing->whichState;
    }

    
    
    elapsed = abs((long)(timing->tv1.tv_usec - timing->tv2.tv_usec));
    printf("fps: %ld\n", (int)1000000.0/elapsed);

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    printf("Memory usage: %ld kB\n", usage.ru_maxrss);
    fflush(stdout);

    printf("\033[2A");
    printf("\033[2K\033[1B\033[2K\033[1A");

    
}


XImage* getScreenshotPixmap1234(Display* dpy, Window rootWin, Window mainWin,
                                 Pixmap finalPixmap, Picture finalPic,
                                 XShmSegmentInfo* shminfo, XImage** shmImgPtr) {

    int defaultScreen = DefaultScreen(dpy);
    Window returnedRoot, parent;
    Window* children;
    int numChildren;
    XQueryTree(dpy, rootWin, &returnedRoot, &parent, &children, &numChildren);

    WinMeta* meta = (WinMeta*)malloc(sizeof(WinMeta) * numChildren);
    int checkIndex = 0, openIndex = 0;
    Bool isDone = False;

    while (checkIndex < numChildren && !isDone) {
        // check if this window contains mainWin
        Window rr, pp;
        Window* grandChildren;
        int numGrandchildren;
        if (XQueryTree(dpy, children[checkIndex], &rr, &pp, &grandChildren, &numGrandchildren)) {
            for (int j = 0; j < numGrandchildren; j++) {
                if (grandChildren[j] == mainWin) isDone = True;
            }
            XFree(grandChildren);
        }

        XWindowAttributes attr;
        XGetWindowAttributes(dpy, children[checkIndex], &attr);

        if (attr.map_state == IsViewable && !attr.override_redirect) {
            int rootX = 0, rootY = 0;
            Window tmp;
            XTranslateCoordinates(dpy, children[checkIndex], rootWin, 0,0, &rootX, &rootY, &tmp);

            Pixmap p = XCompositeNameWindowPixmap(dpy, children[checkIndex]);
            XRenderPictFormat* fmt = XRenderFindVisualFormat(dpy, attr.visual);

            meta[openIndex].win = tmp;
            meta[openIndex].pix = p;
            meta[openIndex].fmt = fmt;
            meta[openIndex].x = rootX;
            meta[openIndex].y = rootY;
            meta[openIndex].w = attr.width;
            meta[openIndex].h = attr.height;
            openIndex++;
        }
        checkIndex++;
    }
    openIndex--;

    // composite all windows into finalPixmap
    for (int i = 0; i < openIndex; i++) {
        Picture pic = XRenderCreatePicture(dpy, meta[i].pix, meta[i].fmt, 0, NULL);
        XRenderComposite(dpy, PictOpOver, pic, None, finalPic,
                         0,0,0,0, meta[i].x, meta[i].y, meta[i].w, meta[i].h);
        XRenderFreePicture(dpy, pic);
    }

    free(meta);
    XFree(children);

    // create shared memory XImage if not already
    if (!(*shmImgPtr)) {
        *shmImgPtr = XShmCreateImage(dpy,
                                     DefaultVisual(dpy, defaultScreen),
                                     DefaultDepth(dpy, defaultScreen),
                                     ZPixmap,
                                     NULL,
                                     shminfo,
                                     DisplayWidth(dpy, defaultScreen),
                                     DisplayHeight(dpy, defaultScreen));

        shminfo->shmid = shmget(IPC_PRIVATE,
                                (*shmImgPtr)->bytes_per_line * (*shmImgPtr)->height,
                                IPC_CREAT | 0777);
        shminfo->shmaddr = shmat(shminfo->shmid, 0, 0);
        (*shmImgPtr)->data = shminfo->shmaddr;
        shminfo->readOnly = False;

        if (!XShmAttach(dpy, shminfo)) {
            fprintf(stderr, "XShmAttach failed\n");
            exit(1);
        }
    }

    // get the image from finalPixmap into shared memory
    XShmGetImage(dpy, finalPixmap, *shmImgPtr, 0, 0, AllPlanes);

    return *shmImgPtr;
}


int main() {

    Display* mainDisplay = XOpenDisplay(0);
    Window rootWindow = XDefaultRootWindow(mainDisplay);
    int defaultScreen = DefaultScreen(mainDisplay);
    
    unsigned int windowX = 0;
    unsigned int windowY = 0;
    unsigned int windowWidth = 500;
    unsigned int windowHeight = 500;

    Window mainWindow = XCreateSimpleWindow(mainDisplay, rootWindow, 0, 0, windowWidth, windowHeight, 1, BlackPixel(mainDisplay, defaultScreen), WhitePixel(mainDisplay, defaultScreen));
    XWindowAttributes wa;

    XMapWindow(mainDisplay, mainWindow);
    XFlush(mainDisplay);

    int eventBase, errorBase;
    XCompositeQueryExtension(mainDisplay, &eventBase, &errorBase);

    int damage_event, damage_error;
    XDamageQueryExtension(mainDisplay, &damage_event, &damage_error);
    int* dmgWindows = NULL;

    

    XGetWindowAttributes(mainDisplay, mainWindow, &wa);
    XImage* img;
    
    GC gc = XCreateGC(mainDisplay, mainWindow, 0, NULL);

    timing_t timing;

    Pixmap finalPixmap = XCreatePixmap(mainDisplay, rootWindow, DisplayWidth(mainDisplay, defaultScreen), DisplayHeight(mainDisplay, defaultScreen), DefaultDepth(mainDisplay, defaultScreen));

    XRenderPictureAttributes pa;
    Picture finalPic = XRenderCreatePicture(mainDisplay, finalPixmap, XRenderFindVisualFormat(mainDisplay, DefaultVisual(mainDisplay, defaultScreen)), 0, &pa);

    // for passive frame update even when nothing is happening.
    struct timeval passive1, passive2;
    gettimeofday(&passive1, NULL);
    gettimeofday(&passive2, NULL);
    

    
    XShmSegmentInfo shminfo;
    XImage* shmImg = NULL;

    img = getScreenshotPixmap1234(mainDisplay, rootWindow, mainWindow,
                                finalPixmap, finalPic,
                                &shminfo, &shmImg);

    for (;;) {//main loooooooop!!!
        //img = getScreenshotPixmap1234(mainDisplay, rootWindow, mainWindow,
          //                          finalPixmap, finalPic,
            //                        &shminfo, &shmImg);

        XGetWindowAttributes(mainDisplay, mainWindow, &wa);

        int winX, winY;
        Window child;
        XTranslateCoordinates(mainDisplay, mainWindow, rootWindow, 0, 0, &winX, &winY, &child);

        XShmPutImage(mainDisplay, mainWindow, gc, shmImg,
                    winX, winY, 0, 0, wa.width, wa.height, False);
        XSync(mainDisplay, True);

        getStats(&timing);

        gettimeofday(&passive1, NULL);// passive frame update
        if (abs((int)(passive1.tv_usec - passive2.tv_usec)) >= 100000) {//update every 100 ms
            gettimeofday(&passive1, NULL);
            gettimeofday(&passive2, NULL);
            img = getScreenshotPixmap1234(mainDisplay, rootWindow, mainWindow,
                                      finalPixmap, finalPic,
                                      &shminfo, &shmImg);
        }
    }


    return 0;


}