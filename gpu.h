// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef GPU_H
#define GPU_H

#include "timloader.h"
#include "drawing.h"

#define TEXPAGE_WIDTH 64

void SendToVRAM( unsigned long memAddr, short xPos, short yPos, short width, short height );

// Draw to screen using a TIM we've uploaded to RAM
void DrawTIMData( TIMData * inData, unsigned long inX, unsigned long inY, unsigned long inWidth, unsigned long inHeight );

// DrawTIMData - but with pos/width defined in the Sprite struct
void DrawSprite( Sprite * inSprite );

#endif