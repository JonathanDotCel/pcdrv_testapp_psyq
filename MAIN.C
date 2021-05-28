// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include <stdlib.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include <libetc.h>

#include "timloader.h"
#include "gpu.h"

// the one we're interested in
// the full source is kicking about somewhere in the psyq sdk folder
#include <libsn.h>

#define ulong unsigned long

#define SCREEN_WIDTH 512
#define SCREEN_HEIGHT 240

#define FILEMODE_READONLY 0
#define FILEMODE_WRITEONLY 1
#define FILEMODE_READWRITE 2


#define MODE_PAL 1
#define MODE_NTSC 0

// standard PSYQ stuffs
#define OTLEN       16
#define BUFFSIZE    256
#define TIMEOUT     999999

static int activeBuffer;
static long	ot[2][OTLEN];
static DISPENV	disp[2];
static DRAWENV	draw[2];

// current and last pad vals to detect
// a button a press (vs holding it)
int lastPadVals = 0;
int padVals = 0;

// Origin, Current, End
int seekMode = 0;

// PCDrv return val
int lastOpsVal = 0;

void UpdatePads(){
    lastPadVals = padVals;
    padVals = PadRead( 0 );
}

// Button released on pad
int Released( ulong inButton ){

    char returnVal = ( !(padVals & inButton) && (lastPadVals & inButton ) );

    // pad's not ready or something's fucky
    if ( padVals == 0xFFFFFFFF )
        return 0;

    // Clear this event
    if ( returnVal ){        
        lastPadVals ^= inButton;
    }
    
    return returnVal;
    
}


// Clear the display list, wait for a vsync and flip buffers
void StartDraw(){

    ClearOTag( ot[activeBuffer], OTLEN );

    VSync(0);

    activeBuffer ^= 1;
    
}

// Draw it
void EndDraw(){

    PutDispEnv(&disp[activeBuffer]);
    PutDrawEnv(&draw[activeBuffer]);

    DrawOTag( ot[activeBuffer^1] );
    FntFlush(-1);

}

// Print printable chars, escape the rest.
void ShowFileContents( char * addr , int length ){
    
    char clamped = 0;
    if ( length > 500 ){
         length = 500;
         clamped = 1;
    }

    while( !Released( PADRdown ) ){

        int i;        
        char * originalAddr = addr;

        StartDraw();
        FntPrint( "\n---- %d bytes start ----\n", length );

        for( i = 0; i < length; i++ ){

            unsigned char c = *originalAddr++;

            // vaguely printable range            
            if ( c >= 10 && c <= 'z' ){
                FntPrint( "%c", c );
            } else {
                FntPrint( " %%%x ", c );
            }
            
        }
        FntPrint( "\n---- end ----\n " );

        if ( clamped ){
            FntPrint( "Clamped at 500 chars\n" );
        }

        UpdatePads();
        EndDraw();
    }

}

void QuickMessage( char * message, int param0, int param1 ){

    while ( !Released( PADRdown ) ){
        StartDraw();        
        FntPrint( message, param0, param1 );
        FntPrint( "\n\n Press X to continue" );
        UpdatePads();
        EndDraw();
    }
    
}

void ShowStatus(){
    
    QuickMessage( "\n\n\n\n Returned status %d\n", lastOpsVal, 0 );

}

// Handles for each individual file
int handle_bubblenuggets = -1;
int handle_biosdump = -1;
int handle_HELLOTIM = -1;

char didLoadLobster = 0;
TIMData lobsterTimData;

void DrawLoop(){

    padVals = 0;
    lastPadVals = 0;    

    printf( "Hello there...\n" );

	while(1)
	{
        
        // didn't actually implement this.
        #define FILE_DIRECTORY  16

        char L1Held_Open = 0; // PCOpen modifier
        char L2Held_Close = 0; // PCClose modifier
        char R1Held_Create = 0; // PCCreate modifier
        char R2Held_Write = 0; // PCWrite modifier

        StartDraw();

        UpdatePads();

        L1Held_Open = padVals & PADL1;
        L2Held_Close = padVals & PADL2;
        R1Held_Create = padVals & PADR1;
        R2Held_Write = padVals & PADR2;

        FntPrint( "\n" );

        FntPrint("PCDrv sample - github.com/JonathanDotCel\n");
        FntPrint(" \n" );

        FntPrint("Before loading this .exe:\n\n" );
        FntPrint("   put unirom into debugmode (nops /debug or L1+Square)\n");
        FntPrint("   put nops into monitor mode (nops /m)\n");        
        FntPrint(" \n" );
        
        FntPrint( "Debug:\n\n" );
        FntPrint( "   LastResponse= %x, Handles= %d,%d,%d, Pad 1= %x\n", lastOpsVal, handle_bubblenuggets, handle_biosdump, handle_HELLOTIM, padVals );
        FntPrint(" \n" );

        FntPrint( "Key Combos:\n\n" );
        FntPrint( "   Start: PCInit() (Break 0x101) (not required)\n" );
        FntPrint(" \n" );
        if ( Released( PADstart ) ){
            lastOpsVal = PCinit();
            ShowStatus();
        }

        // I hate nested inline ifs too. but hey...
        FntPrint( "   Left/Right: Toggle Write mode %s\n\n", seekMode == 0 ? "[0] 1 2 (from start)" : seekMode == 1 ? "0 [1] 2 (current pos)" : "0 1 [2] (append)" );
        if ( Released( PADLleft ) && seekMode > 0 ){
            seekMode--;
        }
        if ( Released( PADLright ) && seekMode < 2 ){
            seekMode++;
        }
        
        // This should really all be reformed into discrete functions
        // but for the purpose of making a readable demo that can be
        // easily referenced at a glance, we're going linear.

        FntPrint("   L1, L2, R1, R2 = switch to open/close/read/write/etc \n\n" );
        
        if ( R2Held_Write ){

            int targetHandle = -2;
            char * targetString;

            FntPrint( "   Write - Break 0x106\n" );
            FntPrint( "   /\\: PCWrite(bubblenuggets.txt, 'HELLO WORLD!', 12 )\n" );
            FntPrint( "   []: PCWrite(biosdump, <bios at 0xBFC00000>, 512kb )\n"  );
            FntPrint( "   X : PCWrite(bins/lobster.tim, gibberish, 20  )\n"  );
            
            if ( Released( PADRup ) ){
                targetHandle = handle_bubblenuggets;
                targetString = "HELLO WORLD!\n";
            }

            if ( Released( PADRleft ) ){
                targetHandle = handle_biosdump;
                targetString = (char*)0xBFC00000;   // the bios
            }

            if ( Released( PADRdown ) ){
                targetHandle = handle_HELLOTIM;
                targetString = "RUMPLE, RUMPLE, RUMPLE CHUNKS!\n";  // shouldn't write - it's read only
            }

            if ( targetHandle == -2 ){

            } else if ( targetHandle == -1 ){
                QuickMessage( "\nThat file hasn't been opened yet!", 0, 0 );
            } else {
                
                int seekStatus = PClseek( targetHandle, 0, seekMode );

                if ( seekStatus == -1 ){
                    QuickMessage( "Error seeking!", 0, 0 );
                } else {

                    int len = ( targetHandle == handle_biosdump ? (512*1024) : strlen(targetString));
                    int result = PCwrite( targetHandle, targetString, len );

                    if ( result == -1 ){
                        QuickMessage( "Error writing! Code: %x\n", result, 0 );
                    } else{                        
                        QuickMessage( "Wrote %d bytes!\n", result, 0 );
                    }

                }
                
            }

        } else if ( R1Held_Create ){
            
            char didCreate = 0;

            FntPrint( "   Create - Break 0x102\n" );
            FntPrint( "   /\\: PCCreat(bubblenuggets.txt)\n" );
            FntPrint( "   []: PCCreat(biosdump)\n" );
            FntPrint( "   X : PCCreat(bins/lobster.tim)\n" );

            if ( Released( PADRup ) ){
                lastOpsVal = handle_bubblenuggets = PCcreat( "bubblenuggets.txt", 0 );
                didCreate = 1;            
            }
            if ( Released( PADRleft ) ){
                lastOpsVal = handle_biosdump = PCcreat( "biosdump", 0 );
                didCreate = 1;
            }        
            if ( Released( PADRdown ) ){
                lastOpsVal = handle_HELLOTIM = PCcreat( "bins/lobster.tim", 0);
                didCreate = 1;
            }

            if ( didCreate ){
                if ( lastOpsVal == -1 ){
                    ShowStatus();
                } else {
                    QuickMessage( "\n\nCreated file handle is %d\n", lastOpsVal, 0 );
                }
            }
            
        } else if ( L2Held_Close ){
            
            char didClose = 0;

            FntPrint( "   Close - Break 0x104\n" );
            FntPrint( "   /\\: PCClose(bubblenuggets.txt)\n" );
            FntPrint( "   []: PCClose(biosdump)\n" );
            FntPrint( "   X : PCClose(bins/lobster.tim)\n" );

            if ( Released( PADRup ) ){
                lastOpsVal = PCclose( handle_bubblenuggets );
                didClose = 1;
            }
            if ( Released( PADRleft ) ){
                lastOpsVal = PCclose( handle_biosdump );
                didClose = 1;
            }        
            if ( Released( PADRdown ) ){
                lastOpsVal = PCclose( handle_HELLOTIM );
                didClose = 1;
            }

            if ( didClose ){
                if ( lastOpsVal == -1 ){
                    ShowStatus();
                } else {
                    QuickMessage( "Closed file with return code %x\n", lastOpsVal, 0 );
                }
            }

        } else if ( L1Held_Open ){
            
            char didOpen = 0;

            FntPrint( "   Open - Break 0x103\n" );
            FntPrint( "   /\\: PCOpen(bubblenuggets.txt)\n" );
            FntPrint( "   []: PCOpen(biosdump)\n" );
            FntPrint( "   X : PCOpen(bins/lobster.tim)   (READONLY)\n" );

            if ( Released( PADRup ) ){
                lastOpsVal = handle_bubblenuggets = PCopen( "bubblenuggets.txt", FILEMODE_READWRITE, 0);
                didOpen = 1;
            }
            if ( Released( PADRleft ) ){
                lastOpsVal = handle_biosdump = PCopen( "biosdump", FILEMODE_READWRITE, 0 );
                didOpen = 1;
            }        
            if ( Released( PADRdown ) ){
                lastOpsVal = handle_HELLOTIM = PCopen( "bins/lobster.tim", FILEMODE_READONLY, 0);
                didOpen = 1;
            }

            if ( didOpen ){
                if ( lastOpsVal == -1 ){
                    ShowStatus();
                } else {
                    QuickMessage( "\nOpened file handle %d\n", lastOpsVal, 0 );
                }
            }

        } else {
            
            // PCRead

            int targetHandle = -2;

            // slap bang in the middle of RAM
            #define BUFFER 0x80100000
            #define pBuffer (char*)BUFFER
            
            FntPrint( "   Read - Break 0x105\n" );
            FntPrint( "   /\\: Read(bubblenuggets.txt, 0x%x, 100 )\n", BUFFER );
            FntPrint( "   []: Read(biosdump, 0x%x, 100 )\n", BUFFER );
            FntPrint( "   X : Read(bins/lobster.tim) 0x%x\n", BUFFER ); // empty/missing file (by default), expect zeros

            if ( Released( PADRup ) ){
                targetHandle = handle_bubblenuggets;                
            }

            if ( Released( PADRleft ) ){
                targetHandle = handle_biosdump;                
            }

            if ( Released( PADRdown ) ){
                targetHandle = handle_HELLOTIM;
            }

            if ( targetHandle == -2 ){

            } else if ( targetHandle == -1 ){
                QuickMessage( "That file isn't open yet... Hold L1 to open...", 0 , 0 );
            } else {

                // seek to the end to get the file size
                int fileSize = PClseek( targetHandle, 0, 2 );
                if ( fileSize == -1 ){
                    QuickMessage( "Couldn't seek to find the file size...", 0, 0 );
                } else {
                    
                    int returnToStart;

                    printf( "File size 0x%x\n", fileSize );

                    returnToStart = PClseek( targetHandle, 0, 0 );

                    if ( fileSize == -1 ){
                        QuickMessage( "Couldn't seek back to the start of the file...", 0, 0 );
                    } else {
                        
                        lastOpsVal = PCread( targetHandle, pBuffer, fileSize );

                        if ( lastOpsVal == -1 ){
                            QuickMessage( "Error reading the file!", 0, 0 );
                        } else {
                            if ( targetHandle == handle_HELLOTIM ){
                                didLoadLobster = 1;

                                // "Hello world and flappy credits has a good example
                                // of this code with diagrams and stuff if you want to play
                                // with sprites.
                                UploadTim( pBuffer, &lobsterTimData, SCREEN_WIDTH + TEXPAGE_WIDTH, 1, SCREEN_WIDTH + TEXPAGE_WIDTH, 16 );
                            } else {
                                ShowFileContents( pBuffer, fileSize );
                            }
                        }

                    }

                }
                
            }

        }

        // Reboot
        if ( Released( PADRright ) ){
            goto *(ulong*)0xBFC00000;
        }

        
        // mad flickering from trying to mix nuggety things with psyq
        // but it's just an example
        if ( didLoadLobster ){

            Sprite * s;
            s->data = &lobsterTimData;

            s->xPos = 20;
            s->yPos = 20;

            s->height = 40;
            s->width = 40;

            DrawSprite( s );
            
        }
                

        EndDraw();
		
	}


}


int main() 
{

	// Standard sony stuff
	SetDispMask(0);
	ResetGraph(0);
	SetGraphDebug(0);
	ResetCallback();
	//ExitCriticalSection();

	SetDefDrawEnv(&draw[0], 0,   0, SCREEN_WIDTH, SCREEN_HEIGHT);
	SetDefDrawEnv(&draw[1], 0, SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT);
	SetDefDispEnv(&disp[0], 0, SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT);
	SetDefDispEnv(&disp[1], 0,   0, SCREEN_WIDTH, SCREEN_HEIGHT);
	draw[0].isbg = draw[1].isbg = 1;
	setRGB0( &draw[0], 0, 0, 64 );
	setRGB0( &draw[1], 0, 0, 64 );
	PutDispEnv(&disp[0]);
	PutDrawEnv(&draw[0]);

	/* Initialize onscreen font and text output system */
	FntLoad(960, 256);
	SetDumpFnt(FntOpen(16, 16, 512 -32, 200, 0, 700));
    	
	VSync(0);			    /* Wait for VBLANK */
	SetDispMask(1);			/* Turn on the display */

    // Prod the bios for the E/J/U identifier
    {
        char isPAL = *(char*)0xBFC7FF52 == 'E';
        SetVideoMode(isPAL);
    }

	activeBuffer = 0;
	
    PadInit( 0 );

    DrawLoop();

	return 0;
    
}

