// Rendering of the game's pre-decoded full-screen images (EGA/IN/*).
#ifndef LOF_PORT_SCREENS_H
#define LOF_PORT_SCREENS_H

#include "gfx.h"
#include <string>

namespace lofport {

// Decode an EGA image and blit it.  If `center`, the image is centred on the
// (blank) screen; otherwise it is drawn at the top-left.  Returns false (with a
// message in *err) if the file cannot be read/decoded.
bool show_ega(Screen& scr, const std::string& path, bool center, std::string* err);

// Decode an EGA image and blit it at an explicit position.
bool blit_ega(Screen& scr, const std::string& path, int x, int y, std::string* err);

}  // namespace lofport

#endif
