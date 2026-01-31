#ifndef settings_h_
#define settings_h_

#ifndef VGA
#define VGA 			0 	//use 0 for ILI9341 Display
#endif

#ifndef PS2KEYBOARD
#define PS2KEYBOARD 	0	//Use 0 for USB-HOST
#endif


//Note: PAL/NTSC are EMULATED - This is not the real videomode!
#ifndef PAL
#define PAL           1 //use 0 for NTSC
#endif

#ifndef FASTBOOT
#define FASTBOOT      1 //0 to disable fastboot
#endif


#define EXACTTIMINGDURATION 600ul //ms exact timing after IEC-BUS activity



#endif
