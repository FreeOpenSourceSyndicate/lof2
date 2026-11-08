#include "screens.h"

namespace lofport {

bool show_ega(Screen& scr, const std::string& path, bool center, std::string* err) {
    EgaImage img;
    if (!load_ega_image(path, img, err)) return false;
    int x = center ? (scr.width - img.width_bytes * 8) / 2 : 0;
    int y = center ? (scr.height - img.height) / 2 : 0;
    blit_image(scr, img, x, y);
    return true;
}

bool blit_ega(Screen& scr, const std::string& path, int x, int y, std::string* err) {
    EgaImage img;
    if (!load_ega_image(path, img, err)) return false;
    blit_image(scr, img, x, y);
    return true;
}

}  // namespace lofport
