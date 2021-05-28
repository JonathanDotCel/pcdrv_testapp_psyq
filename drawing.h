// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.


#ifndef DRAWING_H
#define DRAWING_H

#include "timloader.h"

typedef struct Sprite{

    // Size + Position we intend to draw to on screen
    // in the future we'll add rotation, etc
    unsigned long xPos;
    unsigned long yPos;
    unsigned long width;
    unsigned long height;
    
    // The TIM data in VRAM
    // e.g. position, palette location etc
    TIMData * data;
    
} Sprite;


#endif