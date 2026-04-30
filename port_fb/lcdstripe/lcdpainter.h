#ifndef _LCD_PAINTER_H
#define _LCD_PAINTER_H

#include <stdint.h>

class MyLCDView {
public:
    MyLCDView(const char *jsonpath);
    ~MyLCDView() {}
    void loadStripeTexture(const char *texpath, void *renderer);
    void setPixel(int x, int y, unsigned char v) {}
    void paint(void *render, bool lcdon, bool draw_stripe) {}
    int getLCDWidth() { return 938; }
    int getLCDHeight() { return 400; }
};

#endif
