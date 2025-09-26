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


XImage* getScreenshotPixmap1234(Display* mainDspy, Window rootWin, Window mainWin, Pixmap finalPixmap, Picture finalPic) {

    int defaultScreen = DefaultScreen(mainDspy);
    XWindowAttributes wa;
    XGetWindowAttributes(mainDspy, mainWin, &wa);

    Window returnedRoot, parent;
    Window* children;
    int numChildren;
    XQueryTree(mainDspy, rootWin, &returnedRoot, &parent, &children, &numChildren);

    Pixmap* openedWindows = (Pixmap*)malloc(sizeof(Pixmap) * numChildren);
    XRenderPictFormat** formats = (XRenderPictFormat**)malloc(sizeof(XRenderPictFormat*) * numChildren);
    
    WinMeta* meta = (WinMeta*)malloc(sizeof(WinMeta) * numChildren);
    
    int checkIndex = 0;
    int openIndex = 0;
    Bool isDone = False;
    while (checkIndex < numChildren && !isDone) {// look over all children

        //check if this is me(my window)
        Window rr, pp;
        Window* grandChildren;
        int numGrandchildren;
        if (XQueryTree(mainDspy, children[checkIndex], &rr, &pp, &grandChildren, &numGrandchildren)) {
            for (int j = 0; j < numGrandchildren; j++) {
                if (grandChildren[j] == mainWin) {
                    isDone = True;
                }
            }
        }
        XFree(grandChildren);


        // get the atricubes of the child window
        XWindowAttributes attr;
        XGetWindowAttributes(mainDspy, children[checkIndex], &attr);

        

        //turn all valid children into pixmaps
        if (attr.map_state == IsViewable && !attr.override_redirect) {

            int rootX = 0, rootY = 0;
            Window tmp;
            XTranslateCoordinates(mainDspy, children[checkIndex], rootWin, 0,0, &rootX, &rootY, &tmp);
            
            //printf("width: %d, height: %d, x: %d, y: %d\n", attr.width, attr.height, attr.x, attr.y);

            XRenderPictFormat* format = XRenderFindVisualFormat(mainDspy, attr.visual);
            Pixmap p = XCompositeNameWindowPixmap(mainDspy, children[checkIndex]);

            meta[openIndex].win = tmp;
            meta[openIndex].fmt = format;
            meta[openIndex].pix = p;
            meta[openIndex].x = rootX;
            meta[openIndex].y = rootY;
            meta[openIndex].w = attr.width;
            meta[openIndex].h = attr.height;
            openIndex++;
        }
        checkIndex++;
    }
    openIndex--;

    //Pixmap finalPixmap = XCreatePixmap(mainDspy, rootWin, DisplayWidth(mainDspy, defaultScreen), DisplayHeight(mainDspy, defaultScreen), DefaultDepth(mainDspy, defaultScreen));

    //XRenderPictureAttributes pa;
    //Picture finalPic = XRenderCreatePicture(mainDspy, finalPixmap, XRenderFindVisualFormat(mainDspy, DefaultVisual(mainDspy, defaultScreen)), 0, &pa);


    //loop over all windows and composite them into one picture
    for (int i = 0; i < openIndex; i++) {
        Picture someWindow;
        someWindow = XRenderCreatePicture(mainDspy, meta[i].pix, meta[i].fmt, 0, NULL);
        XRenderComposite(mainDspy, PictOpOver, someWindow, None, finalPic, 0,0, 0,0, meta[i].x,meta[i].y, meta[i].w, meta[i].h);
        XRenderFreePicture(mainDspy, someWindow);
    }
    

    

    XImage* img = XGetImage(mainDspy, finalPixmap, 0,0, DisplayWidth(mainDspy, defaultScreen), DisplayHeight(mainDspy, defaultScreen), AllPlanes, ZPixmap);

    // copy pixels into a new XImage with malloc'd data
    XImage* copy = XCreateImage(mainDspy, DefaultVisual(mainDspy, defaultScreen), img->depth, ZPixmap, 0,
                                (char*)malloc(img->bytes_per_line * img->height),
                                img->width, img->height, img->bitmap_pad, img->bytes_per_line);

    memcpy(copy->data, img->data, img->bytes_per_line * img->height);

    XDestroyImage(img);
    XFree(children);
    free(openedWindows);
    free(formats);

    return copy;
    
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

    //get all the windows except mine

    XGetWindowAttributes(mainDisplay, mainWindow, &wa);
    XImage* img;
    
    GC gc = XCreateGC(mainDisplay, mainWindow, 0, NULL);

    timing_t timing;

    Pixmap finalPixmap = XCreatePixmap(mainDisplay, rootWindow, DisplayWidth(mainDisplay, defaultScreen), DisplayHeight(mainDisplay, defaultScreen), DefaultDepth(mainDisplay, defaultScreen));

    XRenderPictureAttributes pa;
    Picture finalPic = XRenderCreatePicture(mainDisplay, finalPixmap, XRenderFindVisualFormat(mainDisplay, DefaultVisual(mainDisplay, defaultScreen)), 0, &pa);


    
    for (;;) {// main looooooooooooooooooop!!!!!
        img = getScreenshotPixmap1234(mainDisplay, rootWindow, mainWindow, finalPixmap, finalPic);

        XGetWindowAttributes(mainDisplay, mainWindow, &wa);
        int winX, winY;
        Window child;
        XTranslateCoordinates(mainDisplay, mainWindow, rootWindow, 0, 0, &winX, &winY, &child);
        
        XPutImage(mainDisplay, mainWindow, gc, img, winX, winY, 0,0, wa.width, wa.height);
        XDestroyImage(img);

        getStats(&timing);

    }


    return 0;


}