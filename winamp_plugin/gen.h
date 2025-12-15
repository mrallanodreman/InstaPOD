#ifndef _GEN_H_
#define _GEN_H_

// Winamp General Purpose Plugin Header
#define GPPHDR_VER 0x10

typedef struct {
    int version;
    char *description;
    int (*init)();
    void (*config)();
    void (*quit)();
    HWND hwndParent;
    HINSTANCE hDllInstance;
} winampGeneralPurposePlugin;

#endif
