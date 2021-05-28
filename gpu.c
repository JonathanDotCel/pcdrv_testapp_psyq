// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// some reading
// http://problemkaputt.de/psx-spx.htm#gpuioportsdmachannelscommandsvram
// http://problemkaputt.de/psx-spx.htm#gpudisplaycontrolcommandsgp1
// https://www.reddit.com/r/EmuDev/comments/fmhtcn/article_the_ps1_gpu_texture_pipeline_and_how_to/
// https://www.reddit.com/r/EmuDev/comments/fmhtcn/article_the_ps1_gpu_texture_pipeline_and_how_to/fs1x8n4/?utm_medium=android_app&utm_source=share
// 


#include "gpu.h"
#include "hwregs.h"
#include "drawing.h"
#include "timloader.h"
#include "drawing.h" // the sprite type

#define ulong unsigned long
#define ushort unsigned short
#define uchar unsigned char



#pragma region hardware registers and addresses

//
// Hardware Registers
//

#define MODE_NTSC 0
#define MODE_PAL 1 

#define SCREEN_WIDTH  512 // screen width
#define SCREEN_HEIGHT 240 // screen height

#define DMA_CTRL 0xBF8010F0
#define pDMA_CTRL *(volatile ulong*)DMA_CTRL

#define GP0 0xBF801810
#define pGP0 *(volatile ulong*)GP0

#define GP1 0xBF801814
#define pGP1 *(volatile ulong*)GP1

	// GP1 Status

	// bit 10, drawing is allowed?
	#define GP1_DRAWING_ALLOWED 0x400
	// bit 26 - ready for another command?
	#define GP1_READYFORCOMMAND 0x4000000	
	// bit 28 - ready to recieve DMA block
	#define GP1_READYFORDMABLOCK 0x10000000

	// GP0 Commands
	#define GP0_CLEAR_CACHE			0x01000000
	#define GP0_COPY_RECT_VRAM2VRAM	0x80000000
	#define GP0_COPY_RECT_CPU2VRAM	0xA0000000
	#define GP0_COPY_RECT_VRAM2CPU	0xC0000000

	// GP1 Commands

	#define GP1_RESET_GPU		0x00000000
	#define GP1_RESET_BUFFER	0x01000000
	#define GP1_ACK_GPU_INTR	0x02000000
	#define GP1_DISPLAY_ENABLE	0x03000000
	#define GP1_DMA_DIR_REQ		0x04000000
		#define GP1_DR_OFF		0x0
		#define GP1_DR_FIFO		0x01
		#define GP1_DR_CPUTOGP0	0x02
		#define GP1_DR_GPUTOCPU	0x03

	#define GP1_TEXPAGE 0xE1000000
		#define PAGE_8_BIT 0x80
		#define PAGE_4_BIT 0x00
		#define PAGE_DITHER 0x200
		#define PAGE_ALLOW_DISPLAY_AREA_DRAWING 0x400

// DMA Control Reg
#define DPCR 0xBF8010F0
#define pDPCR *(volatile ulong*)DPCR

// DMA Interrupt Reg
#define DICR 0xBF8010F4
#define pDICR *(volatile ulong*)DICR

#define D2_MADR 0xBF8010A0
#define pD2_MADR *(volatile ulong*)D2_MADR

#define D2_BCR	0xBF8010A4
#define pD2_BCR *(volatile ulong*)D2_BCR

// facilitates mem to vram transfer/cancelation/monitoring, etc
#define D2_CHCR 0xBF8010A8
#define pD2_CHCR *(volatile ulong*)D2_CHCR
	
	// 0x01000401
	// "link mode , mem->GPU, DMA enable"
	#define D2_TO_RAM 0
	#define D2_FROM_RAM 1

	// bits 10, 11
	#define SYNCMODE_IMMEDIATE	0x0	
	#define SYNCMODE_DMA		0x200
	#define SYNCMODE_LINKEDLIST	0x400

	// bit 24
	#define D2_START_BUSY	0x1000000

#define INT_VBLANK 0x01

// For testing edge drawing tolerances
#define testOffsetY 20
#define testOffsetX 2

#pragma endregion hardware registers and addresses

//
// Various GPU routines used in Greentro, Herben's Import Player, etc.
//

// WaitGPU - waits until GPU ready to recieve commands
static void WaitGPU(){
    
	int waitCounter = 0;	
	int nullVal = 0;

	while ( 
		( pGP1 & GP1_READYFORCOMMAND ) == 0			// bit 28 in GP1
		//|| ( pD2_CHCR & D2_START_BUSY ) != 0		// bit 24 in D2-CHCR
	){
		waitCounter++;
		__asm__ volatile( "" );
	}

}


// TODO: Add the DMA block check?
static void WaitIdle(){

	int waitCounter = 0;

	while ( ( pGP1 & GP1_READYFORCOMMAND ) == 0 ){
		waitCounter++;
	}

}

// Wait for DMA
static void WaitDMA(){

	int waitCounter = 0;

	while ( ( pGP1 & GP1_READYFORCOMMAND ) == 0 ){
		waitCounter = 0;
	}

}




// Uploads some stuff (Texture, CLUT, etc) to VRAM without using
// DMA. Not the fastest, but it's plenty fast for Unirom.

void SendToVRAM( ulong memAddr, short xPos, short yPos, short width, short height ){
	
	int i = 0;
	int numDWords = ( width * height );
    
    printf( "Uploading to %d,%d %d,%d\n", xPos, yPos, width, height );

	WaitIdle();
    
	// DMA Off
	pGP1 = GP1_DMA_DIR_REQ;

	pGP1 = GP1_RESET_BUFFER;

	pGP0 = GP0_COPY_RECT_CPU2VRAM;
	pGP0 = (( yPos << 16 ) | ( xPos & 0xFFFF  ));
	pGP0 = (( height << 16 ) | ( width / 2 ));

	for( i = memAddr; i < memAddr + numDWords; i+=4 ){
		
		pGP0 = *(ulong*)i;

	}
	
}

void SetPageDepth( int inPage, char in8Bit ){
    pGP0 = GP1_TEXPAGE | (PAGE_8_BIT* in8Bit) | PAGE_DITHER | PAGE_ALLOW_DISPLAY_AREA_DRAWING | inPage;
}


#pragma GCC push options
#pragma GCC optimize("-O0")

// Quick n dirty sprite draw, using the info parsed from a TIM header
// Note: this is likely to be a ton slower than using a linked list / OT!

void DrawSprite( Sprite * inSprite ){
    
    DrawTIMData( inSprite->data, inSprite->xPos, inSprite->yPos, inSprite->width, inSprite->height );
    
}

void DrawTIMData( TIMData * inData, ulong inX, ulong inY, ulong inWidth, ulong inHeight ){   
    
    // x,y - Typical Order: TL, BL, TR, BR    
    // vertex data
    short L = inX;
    ulong T = inY;
    short R = L + inWidth;
    ulong B = T + inHeight;
    
    // Same thing for each letter's UV
    // The UV is relative to the texture page (unlike textured rectangle, the texpage is specified below)
    uchar uvLeft  = inData->pixU;
    ushort uvTop = inData->pixV;
    uchar uvRight    = uvLeft + (inData->vramWidth * 2);
    ushort uvBottom = uvTop + (inData->vramHeight * 1);
    
    // Now we have our four points
    // TODO: we should allow some sort of rotation here
    // based around a centre point
    ushort uvTopLeft    = (uvTop << 8 ) | uvLeft;
    ushort uvBottomLeft = (uvBottom << 8 ) | uvLeft;
    ushort uvTopRight   = (uvTop << 8 ) | uvRight;
    ushort uvBottomRight = (uvBottom << 8 ) | uvRight;

    // May be helpful to visualise this as splitting the VRAM
    // 16-pix long rows, 64 of them per line        
    ulong clutPos = ( inData->clutY << 6 ) | ( inData->clutX / 16 );        

    // functionally similar to SetPageDepth
    ulong pageConfig = ( PAGE_8_BIT | inData->texPage) << 16;

    WaitGPU();
    
    pGP0 = 0;
    pGP0 = 0x2C808080;                      // Command + Color    

    pGP0 = (T << 16) | (L);                 // vert T,L
    pGP0 = (clutPos << 16) | uvTopLeft;

    pGP0 = (B << 16) | (L);                 // vert B,L
    
    pGP0 = pageConfig | uvBottomLeft;

    pGP0 = (T << 16) | (R);                 // vert B,L
    pGP0 = uvTopRight;

    pGP0 = (B << 16) | (R);                 // vert B,L
    pGP0 = uvBottomRight;
    
}

#pragma GCC pop options

