/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# (c) 2001-2004, ps2dev - http://www.ps2dev.org
# (c) 2006 Eugene Plotnikov <e-plotnikov@operamail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/
#define SMS_USE_ROM_RMMAN2 0

#include "SMS_RC.h"
#include "SMS_SIF.h"

#define NEWLIB_PORT_AWARE
#include <fileio.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <malloc.h>
#include <loadfile.h>
#include <sifrpc.h>

#define RC_SERVER  0x80000C00
#define RCX_SERVER 0x80000C01

#define RCX_CMD_END   0x00000001
#define RCX_CMD_INIT  0x00000002
#define RCX_CMD_CLOSE 0x00000003
#define RCX_CMD_OPEN  0x00000004

#define RC_CMD_END   0x00000001
#define RC_CMD_INIT  0x00000003
#define RC_CMD_CLOSE 0x00000004
#define RC_CMD_OPEN  0x00000005

static char s_pROM1RMMAN             [] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "rom1:RMMAN";
       char g_pSMSRMMAN              [] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "mc0:/SMS/RMMAN.IRX";
static char s_pSIO2MAN               [] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "sio2man";
#if SMS_USE_ROM_RMMAN2
static char s_pROM1RMMAN2            [] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "rom1:RMMAN2";
#endif  /* SMS_USE_ROM_RMMAN2 */
static SifRpcClientData_t s_ClientRC    __attribute__(   (  aligned( 64 ), section ( ".bss" )  )   );
static SifRpcClientData_t s_ClientRCX   __attribute__(   (  aligned( 64 ), section ( ".bss" )  )   );
static unsigned int       s_CmdBuf[ 5 ] __attribute__(   (  aligned( 64 ), section ( ".bss" )  )   );

extern unsigned char g_RCData[ 256 ] __attribute__(   (  aligned( 64 ), section ( ".sbss" )  )   );

extern unsigned int RC_ReadDummy ( void );
extern unsigned int RC_ReadX     ( void );
extern unsigned int RC_ReadI     ( void );

unsigned int ( *RC_Read ) ( void ) = RC_ReadDummy;

static unsigned char s_RCMask = 0;

static void inline _patch_version ( unsigned char* apData, int aSize, char* apName, unsigned int aVersion ) {

 unsigned int* lpBegin = ( unsigned int* )apData;
 unsigned int* lpEnd   = lpBegin + (  ( aSize + 3 ) >> 2  ) - 1;
 int           lLen    = strlen ( apName );

 while ( lpBegin != lpEnd ) {

  if (  lpBegin[ 0 ] == 0x41E00000 &&
        lpBegin[ 1 ] == 0x00000000 &&
        !memcmp ( lpBegin + 3, apName, lLen )
  ) {

   lpBegin[ 2 ] = aVersion;
   return;

  }  /* end if */

  ++lpBegin;

 }  /* end while */

}  /* end _patch_version */

static int inline _load_rmman ( void ) {

 static char* s_Paths[] __attribute__(   (  section( ".data" )  )   ) = {
  s_pROM1RMMAN, g_pSMSRMMAN, NULL
 };

 char** lppPtr = s_Paths;
 int    retVal = -1;

 while ( *lppPtr ) {

  int lFD  = fioOpen ( *lppPtr, O_RDONLY );
  int lRes = -1;

  if ( lFD >= 0 ) {

   int lSize = fioLseek ( lFD, 0, SEEK_END );

   fioLseek ( lFD, 0, SEEK_SET );

   if ( lSize > 0 ) {

    unsigned char* lpData = ( unsigned char* )malloc ( lSize );

    fioRead ( lFD, lpData, lSize );
    _patch_version ( lpData, lSize, s_pSIO2MAN, 0x00000102 );
    lRes = SifExecModuleBuffer ( lpData, lSize, 0, NULL, &retVal );

    free ( lpData );

   }  /* end if */

   fioClose ( lFD );

  }  /* end if */

  if ( lRes >= 0 && retVal >= 0 ) {

   retVal = lRes;
   break;

  }  /* end if */

  ++lppPtr;

 }  /* end while */

 return retVal;

}  /* end _load_rmman */

static void _set_reader ( int aPort ) {

 if ( !aPort ) {

  if ( s_RCMask & 1 )
   RC_Close ( 0 );
  else if ( s_RCMask & 2 ) RC_Close ( 1 );

  RC_Read  = RC_ReadI;
  s_RCMask = 4;

 } else {

  if ( s_RCMask & 4 ) RCX_Close ();

  if (  ( aPort & 1 ) && ( s_RCMask & 2 )  )
   RC_Close ( 1 );
  else if ( s_RCMask & 1 ) RC_Close ( 0 );

  s_RCMask = aPort;
  RC_Read  = RC_ReadX;

 }  /* end else */

}  /* end _set_reader */

static void _reset_reader ( void ) {

 RC_Read  = RC_ReadDummy;
 s_RCMask = 0;

}  /* end _reset_reader */

int RC_Load ( void ) {

 int retVal = _load_rmman () >= 0;

 if ( retVal ) SIF_BindRPC ( &s_ClientRC, RC_SERVER );

 return retVal;

}  /* end RC_Load */

static unsigned char s_RMMAN2[ 3813 ] __attribute__(   (  aligned( 16 ), section( ".data" )  )   ) = {
	0x7f, 0x45, 0x4c, 0x46, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x80, 0xff, 0x08, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 
	0xfc, 0x09, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x34, 0x00, 0x20, 0x00, 0x02, 0x00, 0x28, 0x00, 
	0x0c, 0x00, 0x09, 0x00, 0x80, 0x00, 0x00, 0x70, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xa0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x50, 0x0a, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 
	0x10, 0x00, 0x00, 0x00, 0xf0, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x88, 0x00, 0x00, 
	0xb0, 0x08, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x50, 0x01, 0x00, 0x00, 0x04, 0x02, 0x72, 0x6d, 
	0x6d, 0x61, 0x6e, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0xe8, 0xff, 0xbd, 0x27, 0x10, 0x00, 0xbf, 0xaf, 0x00, 0x00, 0x04, 0x3c, 0xe0, 0x06, 0x84, 0x24, 
	0xcd, 0x01, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x40, 0x14, 0x01, 0x00, 0x02, 0x24, 
	0x9e, 0x01, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x40, 0x10, 0x21, 0x10, 0x00, 0x00, 
	0x00, 0x00, 0x03, 0x3c, 0x6c, 0x09, 0x63, 0x24, 0x00, 0x00, 0x60, 0xa0, 0x12, 0x00, 0x00, 0x08, 
	0x01, 0x00, 0x60, 0xa0, 0x01, 0x00, 0x02, 0x24, 0x10, 0x00, 0xbf, 0x8f, 0x00, 0x00, 0x00, 0x00, 
	0x08, 0x00, 0xe0, 0x03, 0x18, 0x00, 0xbd, 0x27, 0xc0, 0xff, 0xbd, 0x27, 0x10, 0x00, 0xa4, 0x27, 
	0x02, 0x00, 0x02, 0x24, 0x3c, 0x00, 0xbf, 0xaf, 0x38, 0x00, 0xb0, 0xaf, 0x10, 0x00, 0xa2, 0xaf, 
	0xf2, 0x01, 0x00, 0x0c, 0x18, 0x00, 0xa0, 0xaf, 0x00, 0x00, 0x10, 0x3c, 0x68, 0x09, 0x10, 0x26, 
	0x15, 0x00, 0x40, 0x10, 0x00, 0x00, 0x02, 0xae, 0x20, 0x00, 0xa4, 0x27, 0x00, 0x02, 0x02, 0x3c, 
	0x20, 0x00, 0xa2, 0xaf, 0x00, 0x00, 0x02, 0x3c, 0xec, 0x01, 0x42, 0x24, 0x28, 0x00, 0xa2, 0xaf, 
	0x2e, 0x00, 0x02, 0x24, 0x30, 0x00, 0xa2, 0xaf, 0x00, 0x08, 0x02, 0x24, 0xe1, 0x01, 0x00, 0x0c, 
	0x2c, 0x00, 0xa2, 0xaf, 0x08, 0x00, 0x40, 0x10, 0xfc, 0xff, 0x02, 0xae, 0x21, 0x20, 0x40, 0x00, 
	0xe5, 0x01, 0x00, 0x0c, 0x21, 0x28, 0x00, 0x00, 0x01, 0x00, 0x02, 0x24, 0x04, 0x00, 0x02, 0xa2, 
	0x37, 0x00, 0x00, 0x08, 0x01, 0x00, 0x02, 0x24, 0x21, 0x10, 0x00, 0x00, 0x3c, 0x00, 0xbf, 0x8f, 
	0x38, 0x00, 0xb0, 0x8f, 0x08, 0x00, 0xe0, 0x03, 0x40, 0x00, 0xbd, 0x27, 0x00, 0x00, 0xa0, 0xac, 
	0x00, 0x00, 0x05, 0x3c, 0x6d, 0x09, 0xa5, 0x24, 0x00, 0x00, 0xa2, 0x90, 0x00, 0x00, 0x00, 0x00, 
	0x08, 0x00, 0x40, 0x14, 0x01, 0x00, 0x02, 0x24, 0x02, 0x00, 0x03, 0x24, 0xe3, 0xff, 0xa3, 0xac, 
	0x21, 0x18, 0x40, 0x00, 0xe7, 0xff, 0xa0, 0xac, 0xef, 0xff, 0xa4, 0xac, 0x08, 0x00, 0xe0, 0x03, 
	0x00, 0x00, 0xa3, 0xa0, 0x08, 0x00, 0xe0, 0x03, 0x21, 0x10, 0x00, 0x00, 0x00, 0x00, 0x03, 0x3c, 
	0x6d, 0x09, 0x63, 0x24, 0x00, 0x00, 0x62, 0x90, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x40, 0x10, 
	0x01, 0x00, 0x02, 0x24, 0x08, 0x00, 0xe0, 0x03, 0x00, 0x00, 0x60, 0xa0, 0x08, 0x00, 0xe0, 0x03, 
	0x21, 0x10, 0x00, 0x00, 0xe0, 0xff, 0xbd, 0x27, 0x18, 0x00, 0xb0, 0xaf, 0x00, 0x00, 0x10, 0x3c, 
	0x6c, 0x09, 0x10, 0x26, 0x1c, 0x00, 0xbf, 0xaf, 0x00, 0x00, 0x02, 0x92, 0x00, 0x00, 0x00, 0x00, 
	0x1a, 0x00, 0x40, 0x10, 0x01, 0x00, 0x02, 0x24, 0x01, 0x00, 0x02, 0x92, 0x00, 0x00, 0x00, 0x00, 
	0x03, 0x00, 0x40, 0x10, 0x00, 0x00, 0x00, 0x00, 0x4b, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 
	0xfc, 0xff, 0x04, 0x8e, 0xf6, 0x01, 0x00, 0x0c, 0x01, 0x00, 0x05, 0x24, 0x02, 0x00, 0x05, 0x24, 
	0x10, 0x00, 0x06, 0x24, 0xfc, 0xff, 0x04, 0x8e, 0xf8, 0x01, 0x00, 0x0c, 0x21, 0x38, 0xa6, 0x03, 
	0xfc, 0xff, 0x04, 0x8e, 0xf4, 0x01, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x04, 0x8e, 
	0xe3, 0x01, 0x00, 0x0c, 0xfc, 0xff, 0x00, 0xae, 0x04, 0x00, 0x40, 0x14, 0x21, 0x10, 0x00, 0x00, 
	0x01, 0x00, 0x02, 0x24, 0xf8, 0xff, 0x00, 0xae, 0x00, 0x00, 0x00, 0xa2, 0x1c, 0x00, 0xbf, 0x8f, 
	0x18, 0x00, 0xb0, 0x8f, 0x08, 0x00, 0xe0, 0x03, 0x20, 0x00, 0xbd, 0x27, 0xc0, 0xff, 0xbd, 0x27, 
	0x38, 0x00, 0xb2, 0xaf, 0x01, 0x00, 0x12, 0x24, 0x30, 0x00, 0xb0, 0xaf, 0x00, 0x00, 0x10, 0x3c, 
	0x68, 0x09, 0x10, 0x26, 0x34, 0x00, 0xb1, 0xaf, 0x98, 0xff, 0x11, 0x26, 0x3c, 0x00, 0xbf, 0xaf, 
	0x1f, 0x02, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x8e, 0xfa, 0x01, 0x00, 0x0c, 
	0x10, 0x00, 0xa5, 0x27, 0x1c, 0x00, 0xa2, 0x8f, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x52, 0x14, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x8e, 0xf6, 0x01, 0x00, 0x0c, 0x02, 0x00, 0x05, 0x24, 
	0xe7, 0x01, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x02, 0x92, 0x00, 0x00, 0x00, 0x00, 
	0xef, 0xff, 0x40, 0x10, 0x00, 0x00, 0x00, 0x00, 0x9d, 0x00, 0x00, 0x0c, 0x21, 0x20, 0x20, 0x02, 
	0xf8, 0xff, 0x02, 0xae, 0xc1, 0x00, 0x00, 0x0c, 0x21, 0x20, 0x20, 0x02, 0x84, 0x00, 0x00, 0x08, 
	0x00, 0x00, 0x00, 0x00, 0xd8, 0xff, 0xbd, 0x27, 0x20, 0x00, 0xb0, 0xaf, 0x21, 0x80, 0x80, 0x00, 
	0x1e, 0x00, 0x04, 0x24, 0x21, 0x28, 0x00, 0x00, 0x21, 0x30, 0xa0, 0x00, 0x24, 0x00, 0xbf, 0xaf, 
	0x28, 0x02, 0x00, 0x0c, 0x10, 0x00, 0xa7, 0x27, 0x06, 0x00, 0x40, 0x10, 0x00, 0x00, 0x00, 0x00, 
	0x10, 0x00, 0xa2, 0x93, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x42, 0x30, 0x04, 0x00, 0x40, 0x10, 
	0x21, 0x18, 0x00, 0x00, 0x50, 0x00, 0x00, 0xae, 0xbd, 0x00, 0x00, 0x08, 0x21, 0x10, 0x00, 0x00, 
	0x10, 0x00, 0xa5, 0x27, 0x21, 0x20, 0x03, 0x02, 0x01, 0x00, 0x63, 0x24, 0x21, 0x10, 0xa3, 0x00, 
	0x00, 0x00, 0x42, 0x90, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x82, 0xa0, 0x04, 0x00, 0x62, 0x28, 
	0xf9, 0xff, 0x40, 0x14, 0x21, 0x20, 0x03, 0x02, 0x03, 0x00, 0x02, 0x24, 0x50, 0x00, 0x02, 0xae, 
	0x01, 0x00, 0x02, 0x24, 0x24, 0x00, 0xbf, 0x8f, 0x20, 0x00, 0xb0, 0x8f, 0x08, 0x00, 0xe0, 0x03, 
	0x28, 0x00, 0xbd, 0x27, 0xd0, 0xff, 0xbd, 0x27, 0x21, 0x30, 0x80, 0x00, 0x2c, 0x00, 0xbf, 0xaf, 
	0x28, 0x00, 0xb0, 0xaf, 0x58, 0x00, 0x82, 0x8c, 0x21, 0x28, 0x00, 0x00, 0x20, 0x00, 0xa0, 0xaf, 
	0x01, 0x00, 0x47, 0x30, 0x2c, 0x00, 0x82, 0xac, 0x01, 0x00, 0x42, 0x24, 0x58, 0x00, 0x82, 0xac, 
	0x21, 0x18, 0xc5, 0x00, 0x21, 0x10, 0x85, 0x00, 0x30, 0x00, 0x42, 0x90, 0x01, 0x00, 0xa5, 0x24, 
	0x00, 0x00, 0x62, 0xa0, 0x20, 0x00, 0xa2, 0x28, 0xfa, 0xff, 0x40, 0x14, 0x21, 0x18, 0xc5, 0x00, 
	0x50, 0x00, 0x82, 0x8c, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0xc2, 0xac, 0x60, 0x00, 0x82, 0x8c, 
	0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0xc2, 0xac, 0x10, 0x00, 0xa6, 0xaf, 0x5c, 0x00, 0x82, 0x8c, 
	0x02, 0x00, 0xe0, 0x10, 0x20, 0x00, 0xa4, 0x27, 0x80, 0x00, 0x42, 0x24, 0x14, 0x00, 0xa2, 0xaf, 
	0x80, 0x00, 0x02, 0x24, 0x18, 0x00, 0xa2, 0xaf, 0xd6, 0x01, 0x00, 0x0c, 0x1c, 0x00, 0xa0, 0xaf, 
	0x10, 0x00, 0xa4, 0x27, 0x05, 0x02, 0x00, 0x0c, 0x01, 0x00, 0x05, 0x24, 0x20, 0x00, 0xa4, 0x8f, 
	0xd8, 0x01, 0x00, 0x0c, 0x21, 0x80, 0x40, 0x00, 0x2b, 0x10, 0x10, 0x00, 0x2c, 0x00, 0xbf, 0x8f, 
	0x28, 0x00, 0xb0, 0x8f, 0x08, 0x00, 0xe0, 0x03, 0x30, 0x00, 0xbd, 0x27, 0xd8, 0xff, 0xbd, 0x27, 
	0x20, 0x00, 0xb0, 0xaf, 0x21, 0x80, 0x80, 0x00, 0x20, 0x00, 0x04, 0x24, 0x21, 0x28, 0x00, 0x00, 
	0x21, 0x30, 0xa0, 0x00, 0x24, 0x00, 0xbf, 0xaf, 0x28, 0x02, 0x00, 0x0c, 0x10, 0x00, 0xa7, 0x27, 
	0x09, 0x00, 0x40, 0x10, 0x21, 0x10, 0x00, 0x00, 0x10, 0x00, 0xa2, 0x93, 0x00, 0x00, 0x00, 0x00, 
	0x80, 0x00, 0x42, 0x30, 0x04, 0x00, 0x40, 0x14, 0x21, 0x10, 0x00, 0x00, 0x11, 0x00, 0xa3, 0x93, 
	0x01, 0x00, 0x02, 0x24, 0x00, 0x00, 0x03, 0xa2, 0x24, 0x00, 0xbf, 0x8f, 0x20, 0x00, 0xb0, 0x8f, 
	0x08, 0x00, 0xe0, 0x03, 0x28, 0x00, 0xbd, 0x27, 0x00, 0x00, 0x02, 0x3c, 0xf4, 0x08, 0x42, 0x94, 
	0x08, 0x00, 0xe0, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0xe8, 0xff, 0xbd, 0x27, 0x10, 0x00, 0xb0, 0xaf, 0x14, 0x00, 0xbf, 0xaf, 0x16, 0x00, 0x00, 0x0c, 
	0x21, 0x80, 0x80, 0x00, 0x04, 0x00, 0x02, 0xae, 0x21, 0x10, 0x00, 0x02, 0x14, 0x00, 0xbf, 0x8f, 
	0x10, 0x00, 0xb0, 0x8f, 0x08, 0x00, 0xe0, 0x03, 0x18, 0x00, 0xbd, 0x27, 0xe8, 0xff, 0xbd, 0x27, 
	0x10, 0x00, 0xb0, 0xaf, 0x14, 0x00, 0xbf, 0xaf, 0x55, 0x00, 0x00, 0x0c, 0x21, 0x80, 0x80, 0x00, 
	0x04, 0x00, 0x02, 0xae, 0x21, 0x10, 0x00, 0x02, 0x14, 0x00, 0xbf, 0x8f, 0x10, 0x00, 0xb0, 0x8f, 
	0x08, 0x00, 0xe0, 0x03, 0x18, 0x00, 0xbd, 0x27, 0xe8, 0xff, 0xbd, 0x27, 0x10, 0x00, 0xb0, 0xaf, 
	0x21, 0x80, 0x80, 0x00, 0x14, 0x00, 0xbf, 0xaf, 0x08, 0x00, 0x04, 0x8e, 0x3b, 0x00, 0x00, 0x0c, 
	0x0c, 0x00, 0x05, 0x26, 0x04, 0x00, 0x02, 0xae, 0x21, 0x10, 0x00, 0x02, 0x14, 0x00, 0xbf, 0x8f, 
	0x10, 0x00, 0xb0, 0x8f, 0x08, 0x00, 0xe0, 0x03, 0x18, 0x00, 0xbd, 0x27, 0xe8, 0xff, 0xbd, 0x27, 
	0x10, 0x00, 0xb0, 0xaf, 0x14, 0x00, 0xbf, 0xaf, 0x4b, 0x00, 0x00, 0x0c, 0x21, 0x80, 0x80, 0x00, 
	0x04, 0x00, 0x02, 0xae, 0x21, 0x10, 0x00, 0x02, 0x14, 0x00, 0xbf, 0x8f, 0x10, 0x00, 0xb0, 0x8f, 
	0x08, 0x00, 0xe0, 0x03, 0x18, 0x00, 0xbd, 0x27, 0xe8, 0xff, 0xbd, 0x27, 0x10, 0x00, 0xb0, 0xaf, 
	0x14, 0x00, 0xbf, 0xaf, 0x06, 0x01, 0x00, 0x0c, 0x21, 0x80, 0x80, 0x00, 0x04, 0x00, 0x02, 0xae, 
	0x21, 0x10, 0x00, 0x02, 0x14, 0x00, 0xbf, 0x8f, 0x10, 0x00, 0xb0, 0x8f, 0x08, 0x00, 0xe0, 0x03, 
	0x18, 0x00, 0xbd, 0x27, 0xe8, 0xff, 0xbd, 0x27, 0x10, 0x00, 0xb0, 0xaf, 0x21, 0x80, 0x80, 0x00, 
	0x14, 0x00, 0xbf, 0xaf, 0xef, 0x00, 0x00, 0x0c, 0x08, 0x00, 0x04, 0x26, 0x04, 0x00, 0x02, 0xae, 
	0x21, 0x10, 0x00, 0x02, 0x14, 0x00, 0xbf, 0x8f, 0x10, 0x00, 0xb0, 0x8f, 0x08, 0x00, 0xe0, 0x03, 
	0x18, 0x00, 0xbd, 0x27, 0xe8, 0xff, 0xbd, 0x27, 0x10, 0x00, 0xb0, 0xaf, 0x21, 0x80, 0xa0, 0x00, 
	0x14, 0x00, 0xbf, 0xaf, 0x00, 0x00, 0x02, 0x8e, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x43, 0x24, 
	0x06, 0x00, 0x62, 0x2c, 0x1e, 0x00, 0x40, 0x10, 0x21, 0x20, 0x00, 0x02, 0x80, 0x10, 0x03, 0x00, 
	0x00, 0x00, 0x01, 0x3c, 0x21, 0x08, 0x22, 0x00, 0xc0, 0x08, 0x22, 0x8c, 0x00, 0x00, 0x00, 0x00, 
	0x08, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x01, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 
	0x79, 0x01, 0x00, 0x08, 0x21, 0x10, 0x00, 0x02, 0x17, 0x01, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 
	0x79, 0x01, 0x00, 0x08, 0x21, 0x10, 0x00, 0x02, 0x22, 0x01, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 
	0x79, 0x01, 0x00, 0x08, 0x21, 0x10, 0x00, 0x02, 0x2f, 0x01, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 
	0x79, 0x01, 0x00, 0x08, 0x21, 0x10, 0x00, 0x02, 0x3a, 0x01, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 
	0x79, 0x01, 0x00, 0x08, 0x21, 0x10, 0x00, 0x02, 0x45, 0x01, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 
	0x21, 0x10, 0x00, 0x02, 0x14, 0x00, 0xbf, 0x8f, 0x10, 0x00, 0xb0, 0x8f, 0x08, 0x00, 0xe0, 0x03, 
	0x18, 0x00, 0xbd, 0x27, 0xd8, 0xff, 0xbd, 0x27, 0x24, 0x00, 0xbf, 0xaf, 0x07, 0x02, 0x00, 0x0c, 
	0x20, 0x00, 0xb0, 0xaf, 0x03, 0x00, 0x40, 0x14, 0x00, 0x00, 0x00, 0x00, 0x03, 0x02, 0x00, 0x0c, 
	0x00, 0x00, 0x00, 0x00, 0x10, 0x02, 0x00, 0x0c, 0x21, 0x20, 0x00, 0x00, 0xe9, 0x01, 0x00, 0x0c, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x3c, 0x74, 0x09, 0x10, 0x26, 0x21, 0x20, 0x00, 0x02, 
	0x14, 0x02, 0x00, 0x0c, 0x21, 0x28, 0x40, 0x00, 0x18, 0x00, 0x04, 0x26, 0x00, 0x80, 0x05, 0x3c, 
	0x01, 0x0c, 0xa5, 0x34, 0x00, 0x00, 0x06, 0x3c, 0x44, 0x05, 0xc6, 0x24, 0x5c, 0x00, 0x07, 0x26, 
	0x10, 0x00, 0xa0, 0xaf, 0x14, 0x00, 0xa0, 0xaf, 0x12, 0x02, 0x00, 0x0c, 0x18, 0x00, 0xb0, 0xaf, 
	0x16, 0x02, 0x00, 0x0c, 0x21, 0x20, 0x00, 0x02, 0x24, 0x00, 0xbf, 0x8f, 0x20, 0x00, 0xb0, 0x8f, 
	0x08, 0x00, 0xe0, 0x03, 0x28, 0x00, 0xbd, 0x27, 0xd0, 0xff, 0xbd, 0x27, 0x10, 0x00, 0xa4, 0x27, 
	0x00, 0x02, 0x02, 0x3c, 0x10, 0x00, 0xa2, 0xaf, 0x00, 0x00, 0x02, 0x3c, 0xf4, 0x05, 0x42, 0x24, 
	0x18, 0x00, 0xa2, 0xaf, 0x2e, 0x00, 0x02, 0x24, 0x20, 0x00, 0xa2, 0xaf, 0x00, 0x08, 0x02, 0x24, 
	0x28, 0x00, 0xbf, 0xaf, 0xe1, 0x01, 0x00, 0x0c, 0x1c, 0x00, 0xa2, 0xaf, 0x00, 0x00, 0x01, 0x3c, 
	0x70, 0x09, 0x22, 0xac, 0x05, 0x00, 0x40, 0x10, 0x21, 0x20, 0x40, 0x00, 0xe5, 0x01, 0x00, 0x0c, 
	0x21, 0x28, 0x00, 0x00, 0xb4, 0x01, 0x00, 0x08, 0x01, 0x00, 0x02, 0x24, 0x21, 0x10, 0x00, 0x00, 
	0x28, 0x00, 0xbf, 0x8f, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0xe0, 0x03, 0x30, 0x00, 0xbd, 0x27, 
	0x00, 0x00, 0xc0, 0x41, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x72, 0x6d, 0x6d, 0x61, 
	0x6e, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x07, 0x00, 0x00, 0x18, 0x07, 0x00, 0x00, 
	0x18, 0x07, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00, 0xec, 0x00, 0x00, 0x00, 0x2c, 0x01, 0x00, 0x00, 
	0x54, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0xe0, 0x03, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0xe0, 0x41, 0x00, 0x00, 0x00, 0x00, 0x03, 0x01, 0x00, 0x00, 0x6c, 0x6f, 0x61, 0x64, 
	0x63, 0x6f, 0x72, 0x65, 0x08, 0x00, 0xe0, 0x03, 0x06, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x41, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 
	0x69, 0x6e, 0x74, 0x72, 0x6d, 0x61, 0x6e, 0x00, 0x08, 0x00, 0xe0, 0x03, 0x11, 0x00, 0x00, 0x24, 
	0x08, 0x00, 0xe0, 0x03, 0x12, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0xe0, 0x41, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x74, 0x68, 0x62, 0x61, 
	0x73, 0x65, 0x00, 0x00, 0x08, 0x00, 0xe0, 0x03, 0x04, 0x00, 0x00, 0x24, 0x08, 0x00, 0xe0, 0x03, 
	0x05, 0x00, 0x00, 0x24, 0x08, 0x00, 0xe0, 0x03, 0x06, 0x00, 0x00, 0x24, 0x08, 0x00, 0xe0, 0x03, 
	0x08, 0x00, 0x00, 0x24, 0x08, 0x00, 0xe0, 0x03, 0x14, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x41, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 
	0x74, 0x68, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x00, 0x08, 0x00, 0xe0, 0x03, 0x04, 0x00, 0x00, 0x24, 
	0x08, 0x00, 0xe0, 0x03, 0x05, 0x00, 0x00, 0x24, 0x08, 0x00, 0xe0, 0x03, 0x06, 0x00, 0x00, 0x24, 
	0x08, 0x00, 0xe0, 0x03, 0x0a, 0x00, 0x00, 0x24, 0x08, 0x00, 0xe0, 0x03, 0x0d, 0x00, 0x00, 0x24, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x41, 0x00, 0x00, 0x00, 0x00, 
	0x01, 0x01, 0x00, 0x00, 0x73, 0x69, 0x66, 0x6d, 0x61, 0x6e, 0x00, 0x00, 0x08, 0x00, 0xe0, 0x03, 
	0x05, 0x00, 0x00, 0x24, 0x08, 0x00, 0xe0, 0x03, 0x07, 0x00, 0x00, 0x24, 0x08, 0x00, 0xe0, 0x03, 
	0x1d, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x41, 
	0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x73, 0x69, 0x66, 0x63, 0x6d, 0x64, 0x00, 0x00, 
	0x08, 0x00, 0xe0, 0x03, 0x0e, 0x00, 0x00, 0x24, 0x08, 0x00, 0xe0, 0x03, 0x11, 0x00, 0x00, 0x24, 
	0x08, 0x00, 0xe0, 0x03, 0x13, 0x00, 0x00, 0x24, 0x08, 0x00, 0xe0, 0x03, 0x16, 0x00, 0x00, 0x24, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x41, 0x00, 0x00, 0x00, 0x00, 
	0x01, 0x01, 0x00, 0x00, 0x76, 0x62, 0x6c, 0x61, 0x6e, 0x6b, 0x00, 0x00, 0x08, 0x00, 0xe0, 0x03, 
	0x04, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x41, 
	0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x63, 0x64, 0x76, 0x64, 0x6d, 0x61, 0x6e, 0x00, 
	0x08, 0x00, 0xe0, 0x03, 0x70, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x72, 0x6d, 0x6d, 0x61, 0x6e, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x98, 0x05, 0x00, 0x00, 0x88, 0x05, 0x00, 0x00, 0xb8, 0x05, 0x00, 0x00, 0xa8, 0x05, 0x00, 0x00, 
	0xc8, 0x05, 0x00, 0x00, 0xd8, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x50, 0x73, 0x49, 0x49, 0x72, 0x6d, 0x6d, 0x61, 0x6e, 0x32, 0x20, 0x20, 0x32, 0x38, 0x30, 0x31, 
	0xb0, 0x08, 0x00, 0x00, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x2e, 0x69, 0x6f, 0x70, 0x6d, 0x6f, 0x64, 0x00, 0x2e, 0x74, 0x65, 0x78, 0x74, 0x00, 0x2e, 
	0x72, 0x65, 0x6c, 0x2e, 0x74, 0x65, 0x78, 0x74, 0x00, 0x2e, 0x72, 0x6f, 0x64, 0x61, 0x74, 0x61, 
	0x00, 0x2e, 0x72, 0x65, 0x6c, 0x2e, 0x72, 0x6f, 0x64, 0x61, 0x74, 0x61, 0x00, 0x2e, 0x64, 0x61, 
	0x74, 0x61, 0x00, 0x2e, 0x72, 0x65, 0x6c, 0x2e, 0x64, 0x61, 0x74, 0x61, 0x00, 0x2e, 0x62, 0x73, 
	0x73, 0x00, 0x2e, 0x73, 0x68, 0x73, 0x74, 0x72, 0x74, 0x61, 0x62, 0x00, 0x2e, 0x73, 0x79, 0x6d, 
	0x74, 0x61, 0x62, 0x00, 0x2e, 0x73, 0x74, 0x72, 0x74, 0x61, 0x62, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x74, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 
	0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x00, 0x00, 0x00, 
	0xb0, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0xdc, 0x0b, 0x00, 0x00, 0xc0, 0x02, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 
	0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 
	0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0xb0, 0x08, 0x00, 0x00, 0x50, 0x09, 0x00, 0x00, 
	0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x9c, 0x0e, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x2d, 0x00, 0x00, 0x00, 
	0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0xe0, 0x08, 0x00, 0x00, 0x80, 0x09, 0x00, 0x00, 
	0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0xcc, 0x0e, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 
	0x06, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x00, 0x00, 
	0x08, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0xa0, 0x09, 0x00, 0x00, 
	0x50, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0xa0, 0x09, 0x00, 0x00, 0x5c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4c, 0x00, 0x00, 0x00, 
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd4, 0x0e, 0x00, 0x00, 
	0x10, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 
	0x10, 0x00, 0x00, 0x00, 0x54, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0xe4, 0x0e, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 
	0x05, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 
	0x05, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 
	0x05, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x94, 0x00, 0x00, 0x00, 
	0x05, 0x00, 0x00, 0x00, 0x98, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0xac, 0x00, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xd0, 0x00, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0xf4, 0x00, 0x00, 0x00, 
	0x06, 0x00, 0x00, 0x00, 0x2c, 0x01, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x30, 0x01, 0x00, 0x00, 
	0x06, 0x00, 0x00, 0x00, 0x5c, 0x01, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x60, 0x01, 0x00, 0x00, 
	0x06, 0x00, 0x00, 0x00, 0x88, 0x01, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x94, 0x01, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0xa8, 0x01, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xb4, 0x01, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0xc0, 0x01, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xfc, 0x01, 0x00, 0x00, 
	0x05, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x1c, 0x02, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x38, 0x02, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x40, 0x02, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x58, 0x02, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x64, 0x02, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x6c, 0x02, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x90, 0x02, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xb8, 0x02, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x88, 0x03, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x94, 0x03, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0xa0, 0x03, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xd8, 0x03, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x18, 0x04, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x1c, 0x04, 0x00, 0x00, 
	0x06, 0x00, 0x00, 0x00, 0x3c, 0x04, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x68, 0x04, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x9c, 0x04, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xc8, 0x04, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0xf4, 0x04, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x24, 0x05, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x70, 0x05, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x78, 0x05, 0x00, 0x00, 
	0x06, 0x00, 0x00, 0x00, 0x88, 0x05, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x90, 0x05, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x98, 0x05, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xa0, 0x05, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0xa8, 0x05, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xb0, 0x05, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0xb8, 0x05, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xc0, 0x05, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0xc8, 0x05, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xd0, 0x05, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0xd8, 0x05, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xfc, 0x05, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x0c, 0x06, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x14, 0x06, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x1c, 0x06, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x24, 0x06, 0x00, 0x00, 
	0x05, 0x00, 0x00, 0x00, 0x28, 0x06, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x30, 0x06, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x44, 0x06, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x48, 0x06, 0x00, 0x00, 
	0x06, 0x00, 0x00, 0x00, 0x58, 0x06, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x88, 0x06, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x8c, 0x06, 0x00, 0x00, 
	0x06, 0x00, 0x00, 0x00, 0xa4, 0x06, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xac, 0x06, 0x00, 0x00, 
	0x05, 0x00, 0x00, 0x00, 0xb0, 0x06, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0xbc, 0x06, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0xc4, 0x06, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xf4, 0x06, 0x00, 0x00, 
	0x02, 0x00, 0x00, 0x00, 0xf8, 0x06, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0xfc, 0x06, 0x00, 0x00, 
	0x02, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x07, 0x00, 0x00, 
	0x02, 0x00, 0x00, 0x00, 0x08, 0x07, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0c, 0x07, 0x00, 0x00, 
	0x02, 0x00, 0x00, 0x00, 0x10, 0x07, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0xc0, 0x08, 0x00, 0x00, 
	0x02, 0x00, 0x00, 0x00, 0xc4, 0x08, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0xc8, 0x08, 0x00, 0x00, 
	0x02, 0x00, 0x00, 0x00, 0xcc, 0x08, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0xd0, 0x08, 0x00, 0x00, 
	0x02, 0x00, 0x00, 0x00, 0xd4, 0x08, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0xf0, 0x08, 0x00, 0x00, 
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 
};

int RCX_Load ( void ) {
#if SMS_USE_ROM_RMMAN2
 int retVal = SifLoadModule ( s_pROM1RMMAN2, 0, NULL ) >= 0;

 if ( retVal ) SIF_BindRPC ( &s_ClientRCX, RCX_SERVER );
#else
 int i, retVal = SifExecModuleBuffer ( s_RMMAN2, sizeof ( s_RMMAN2 ), 0, NULL, &i ) >= 0;

 if ( retVal ) SIF_BindRPC ( &s_ClientRCX, RCX_SERVER );
#endif  /* SMS_USE_ROM_RMMAN2 */
 return retVal;

}  /* end RCX_Load */

int RC_Start ( void ) {

 int retVal = 0;

 s_CmdBuf[ 0 ] = RC_CMD_INIT;

 if (  SifCallRpc (
        &s_ClientRC, 0, 0, s_CmdBuf, 4, s_CmdBuf, 16, 0, 0
       ) >= 0
 ) retVal = s_CmdBuf[ 3 ];

 return retVal;

}  /* end RC_Start */

int RC_Shutdown ( void ) {

 int retVal = 0;

 s_CmdBuf[ 0 ] = RC_CMD_END;

 if (  SifCallRpc (
        &s_ClientRC, 0, 0, s_CmdBuf, 4, s_CmdBuf, 16, 0, 0
       ) >= 0
 ) retVal = s_CmdBuf[ 3 ];

 return retVal;

}  /* end RC_Shutdown */

int RCX_Start ( void ) {

 int retVal = 0;

 s_CmdBuf[ 0 ] = RCX_CMD_INIT;

 if (  SifCallRpc (
        &s_ClientRCX, 0, 0, s_CmdBuf, 4, s_CmdBuf, 8, 0, 0
       ) >= 0
 ) retVal = s_CmdBuf[ 1 ];

 return retVal;

}  /* end RCX_Start */

int RCX_Shutdown ( void ) {

 int retVal = 0;

 s_CmdBuf[ 0 ] = RCX_CMD_END;

 if (  SifCallRpc (
        &s_ClientRCX, 0, 0, s_CmdBuf, 4, s_CmdBuf, 8, 0, 0
       ) >= 0
 ) retVal = s_CmdBuf[ 1 ];

 return retVal;

}  /* end RCX_Shutdown */

int RC_Open ( int aPort ) {

 int retVal = 0;

 s_CmdBuf[ 0 ] = RC_CMD_OPEN;
 s_CmdBuf[ 1 ] = aPort;
 s_CmdBuf[ 2 ] = 0;
 s_CmdBuf[ 4 ] = ( unsigned int )&g_RCData[ 0 ];

 if (  SifCallRpc (
        &s_ClientRC, 0, 0, s_CmdBuf, 20, s_CmdBuf, 16, 0, 0
       ) >= 0
 ) {

  retVal = s_CmdBuf[ 3 ];
  _set_reader ( 1 << aPort );

 }  /* end if */

 return retVal;

}  /* end RC_Open */

int RC_Close ( int aPort ) {

 int retVal = 0;

 s_CmdBuf[ 0 ] = RC_CMD_CLOSE;
 s_CmdBuf[ 1 ] = aPort;
 s_CmdBuf[ 2 ] = 0;

 if (  SifCallRpc (
        &s_ClientRC, 0, 0, s_CmdBuf, 12, s_CmdBuf, 16, 0, 0
       ) >= 0
 ) {

  retVal = s_CmdBuf[ 3 ];
  _reset_reader ();

 }  /* end if */

 return retVal;

}  /* end RC_Close */

int RCX_Open ( void ) {

 int retVal = 0;

 s_CmdBuf[ 0 ] = RCX_CMD_OPEN;
 s_CmdBuf[ 2 ] = ( unsigned int )&g_RCData[ 0 ];

 if (  SifCallRpc (
        &s_ClientRCX, 0, 0, s_CmdBuf, 12, s_CmdBuf, 8, 0, 0
       ) >= 0
 ) {

  retVal = s_CmdBuf[ 1 ];
  _set_reader ( 0 );

 }  /* end if */

 return retVal;

}  /* end RCX_Open */

int RCX_Close ( void ) {

 int retVal = 0;

 s_CmdBuf[ 0 ] = RCX_CMD_CLOSE;

 if (  SifCallRpc (
        &s_ClientRCX, 0, 0, s_CmdBuf, 4, s_CmdBuf, 8, 0, 0
       ) >= 0
 ) {

  retVal = s_CmdBuf[ 1 ];
  _reset_reader ();

 }  /* end if */

 return retVal;

}  /* end RCX_Close */
