#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


XImage* getScreenshotPixmap1234(Display* mainDspy, Window rootWin, Window mainWin) {



    int defaultScreen = DefaultScreen(mainDspy);
    XWindowAttributes wa;

    Window returnedRoot, parent;
    Window* children;
    int numChildren;
    XQueryTree(mainDspy, rootWin, &returnedRoot, &parent, &children, &numChildren);

    Pixmap* openedWindows = (Pixmap*)malloc(sizeof(Pixmap) * numChildren);
    XRenderPictFormat** formats = (XRenderPictFormat**)malloc(sizeof(XRenderPictFormat) * numChildren);
    int checkIndex = 0;
    int openIndex = 0;
    while (checkIndex < numChildren && children[checkIndex] != mainWin) {
        // get the atricubes of the child window
        XWindowAttributes attr;
        XGetWindowAttributes(mainDspy, children[checkIndex], &attr);
        //turn all valid children into pixmaps
        if (attr.map_state == IsViewable && !attr.override_redirect) {
            printf("width: %d\n", attr.width);

            XRenderPictFormat* format = XRenderFindVisualFormat(mainDspy, attr.visual);

            formats[openIndex] = format;
            openedWindows[openIndex] = XCompositeNameWindowPixmap(mainDspy, children[checkIndex]);
            openIndex++;
        }
        checkIndex++;
    }

    Pixmap finalPixmap = XCreatePixmap(mainDspy, rootWin, DisplayWidth(mainDspy, defaultScreen), DisplayHeight(mainDspy, defaultScreen), DefaultDepth(mainDspy, defaultScreen));

    XRenderPictureAttributes pa;
    Picture finalPic = XRenderCreatePicture(mainDspy, finalPixmap, XRenderFindVisualFormat(mainDspy, DefaultVisual(mainDspy, defaultScreen)), 0, &pa);


    //loop over all windows and composite them into one picture
    Picture someWindow;
    for (int i = 0; i < openIndex; i++) {
        someWindow = XRenderCreatePicture(mainDspy, openedWindows[i], formats[i], 0, NULL);
        XRenderComposite(mainDspy, PictOpOver, someWindow, None, finalPic, 0,0,0,0, 0,0, 1000, 1000);
        XRenderFreePicture(mainDspy, someWindow);
    }

    XGetWindowAttributes(mainDspy, mainWin, &wa);

    XImage* img = XGetImage(mainDspy, finalPixmap, 0,0, wa.width, wa.height, AllPlanes, ZPixmap);

    // copy pixels into a new XImage with malloc'd data
    XImage* copy = XCreateImage(mainDspy, DefaultVisual(mainDspy, defaultScreen), img->depth, ZPixmap, 0,
                                (char*)malloc(img->bytes_per_line * img->height),
                                img->width, img->height, img->bitmap_pad, img->bytes_per_line);

    memcpy(copy->data, img->data, img->bytes_per_line * img->height);
    return copy;
    XDestroyImage(img);
}


int main() {

    Display* mainDisplay = XOpenDisplay(0);
    Window rootWindow = XDefaultRootWindow(mainDisplay);
    int defaultScreen = DefaultScreen(mainDisplay);
    
    unsigned int windowX = 0;
    unsigned int windowY = 0;
    unsigned int windowWidth = 1920;
    unsigned int windowHeight = 1080;

    Window mainWindow = XCreateSimpleWindow(mainDisplay, rootWindow, 50, 50, windowWidth, windowHeight, 1, BlackPixel(mainDisplay, defaultScreen), WhitePixel(mainDisplay, defaultScreen));
    XWindowAttributes wa;

    XMapWindow(mainDisplay, mainWindow);
    XFlush(mainDisplay);

    int eventBase, errorBase;
    XCompositeQueryExtension(mainDisplay, &eventBase, &errorBase);

    //get all the windows except mine

    //XCompositeRedirectSubwindows(mainDisplay, rootWindow, CompositeRedirectAutomatic);

    XGetWindowAttributes(mainDisplay, mainWindow, &wa);
    XImage* img;
    img = getScreenshotPixmap1234(mainDisplay, rootWindow, mainWindow);
    
    
    
    //finalPic now contains someWindow and is the size of the sceen. someWindow is placed at 0,0 with a wifth of 100,100

    

    GC gc = XCreateGC(mainDisplay, mainWindow, 0, NULL);

    XPutImage(mainDisplay, mainWindow, gc, img, 0,0,0,0, wa.width, wa.height);    


    

    for (;;) {

        XPutImage(mainDisplay, mainWindow, gc, img, 0,0,0,0, wa.width, wa.height);  

    }


    return 0;


}