// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// References
// http://wiki.ffrtt.ru/index.php/PSX/TIM_format
// 

#include "timloader.h"
#include "gpu.h"
#include "drawing.h"

#define BPP_4BIT 0x00   // indexed
#define BPP_8BIT 0x01   // indexed
#define BPP_16BIT 0x02
#define BPP_32BIT 0x03
#define ulong unsigned long
#define ushort unsigned short

// The colour lookup table.
typedef struct ClutHeader{

    ulong clutLength;
    ushort clutX;
    ushort clutY;
    ushort clutWidth;
    ushort clutHeight;
    

} ClutHeader;

// The actual pixel data
typedef struct PixHeader{

    ulong pixLength;
    ushort pixX;
    ushort pixY;
    ushort pixWidth;
    ushort pixHeight;    

} PixHeader;


void UploadError( char * inChar ){
    
    printf( "tim upload error\n" );

}


void UploadTim(
    const char* inTim, TIMData * data, 
    unsigned long clutX, unsigned long clutY, unsigned 
    long pixX, unsigned long pixY  
){

    ulong bpp = inTim[4] & 0x03;
    ulong clutPresent = inTim[4] & 0x08;
    ClutHeader * clutHeader;
    PixHeader * pixHeader;
    ulong texPage;
    ulong texPageX;
    ulong texPageY;

    printf( "Uploading tim %x\n", inTim );

    if ( pixX % 16 != 0 ){
        UploadError( "Sprite's VRAM x pos should be a multiple of 16!" );
        return;
    }

    if ( inTim[0] != 0x10 ){
        UploadError( "Unknown TIM header" );
        return;
    }

    // Only supporting 8BPP with colour lookup table right now
    
    if ( bpp != BPP_8BIT ){
        UploadError( "Only 8bit indexed (CLUT) TIMs are supported right now...\n" );
    }

    // the "colour lookup table present" bit
    
    if ( !clutPresent ){
        UploadError( "There's no colour lookup table in this. I can't deal!\n" );
    }
    
    //
    // Read the CLUT header
    //

    clutHeader = (ClutHeader*)(inTim+8);

    printf( "CLUT Length %d\n", clutHeader->clutLength );
    printf( "CLUT x,y %d,%d\n", clutHeader->clutX, clutHeader->clutY );
    printf( "CLUT w,h %d,%d\n", clutHeader->clutWidth, clutHeader->clutHeight );
    
    if ( (clutX == 0 && clutY == 0 ) ){
        // try to make do with whatever's in the TIM header
        data->clutX = clutHeader->clutX;
        data->clutY = clutHeader->clutY;
    } else {
        // if we were given nonzero stuff to use... use that
        data->clutX = clutX;
        data->clutY = clutY;
    }

    //
    // Read the actual pixel data header
    //

    pixHeader = (PixHeader*)( inTim + 8 +clutHeader->clutLength );

    printf( "Pix Length %d\n", pixHeader->pixLength );
    printf( "Pix x,y %d,%d\n", pixHeader->pixX, pixHeader->pixY );
    printf( "Pix w,h %d,%d\n", pixHeader->pixWidth, pixHeader->pixHeight );

    if ( (pixX == 0 && pixY == 0) ){        
        // try to use the value in the header
        data->vramX = pixHeader->pixX;
        data->vramY = pixHeader->pixY;
    } else {
        // if we were given nonzero x/y pos to upload to...
        data->vramX = pixX;
        data->vramY = pixY;
    }
    

    printf( "PixelData Offset: %x\n" , ((ulong)pixHeader - (ulong)inTim) + 12 );
    
    data->vramWidth = pixHeader->pixWidth;  //8bpp
    data->vramHeight = pixHeader->pixHeight;

    // now send them to vram
    SendToVRAM( (ulong)(clutHeader) + 12, data->clutX, data->clutY, clutHeader->clutWidth * 2, clutHeader->clutHeight );
    
    SendToVRAM( (ulong)(pixHeader) + 12, data->vramX, data->vramY, data->vramWidth * 2, pixHeader->pixHeight );
    
    texPage = data->vramX / 64;
    // second row...
    if ( data->vramY >= 256 )
        texPage += 16;

    data->texPage = texPage;

    printf( "TexPage: %d\n", texPage );

    // texture UVs are relative to the tex page, so we can precalculate these
    // to save a little overhead
    texPageX = (data->vramX/64) * 64;
    texPageY = data->vramY >= 256 ? 256 : 0;

    data->pixU = data->vramX - texPageX;
    data->pixV = data->vramY - texPageY;

    printf( "Localised UV: %d,%d (for %d,%d)\n", data->pixU, data->pixV, texPageX, texPageY );
    

}