#define KEYMAP_PRESENT 1
#define PROGMEM
#include "pico.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include <stdio.h>
#include <string.h>
extern "C" {
  #include "emuapi.h"
  #include "iopins.h"
}
static bool emu_writeConfig(void);
static bool emu_readConfig(void);
static bool emu_eraseConfig(void);
static bool emu_writeGfxConfig(void);
static bool emu_readGfxConfig(void);
static bool emu_eraseGfxConfig(void);

#include "pico_dsp.h"
extern PICO_DSP display;

#define MAX_FILENAME_PATH   64
#define NB_FILE_HANDLER     4
#define AUTORUN_FILENAME    "autorun.txt"
#define GFX_CFG_FILENAME    "gfxmode.txt"

#define MAX_FILES           64
#define MAX_FILENAME_SIZE   40
#define MAX_MENULINES       16
#define TEXT_HEIGHT         16
#define TEXT_WIDTH          8
#define MENU_FILE_XOFFSET   (6*TEXT_WIDTH)
#define MENU_FILE_YOFFSET   (2*TEXT_HEIGHT)
#define MENU_FILE_W         (MAX_FILENAME_SIZE*TEXT_WIDTH)
#define MENU_FILE_H         (MAX_MENULINES*TEXT_HEIGHT)
#define MENU_FILE_BGCOLOR   RGBVAL16(0x00,0x00,0x40)
#define MENU_JOYS_YOFFSET   (12*TEXT_HEIGHT)
#define MENU_VBAR_XOFFSET   (0*TEXT_WIDTH)
#define MENU_VBAR_YOFFSET   (MENU_FILE_YOFFSET)

#define MENU_TFT_XOFFSET    (MENU_FILE_XOFFSET+MENU_FILE_W+8)
#define MENU_TFT_YOFFSET    (MENU_VBAR_YOFFSET+32)
#define MENU_VGA_XOFFSET    (MENU_FILE_XOFFSET+MENU_FILE_W+8)
#define MENU_VGA_YOFFSET    (MENU_VBAR_YOFFSET+MENU_FILE_H-32-37)

static int nbFiles=0;
static int curFile=0;
static int topFile=0;
static char selection[MAX_FILENAME_PATH]="";
static char selected_filename[MAX_FILENAME_SIZE]="";
static char files[MAX_FILES][MAX_FILENAME_SIZE];
static bool menuRedraw=true;
static int keyMap;
static bool joySwapped = false;
static uint16_t bLastState;
static int xRef;
static int yRef;
static uint8_t usbnavpad=0;
static bool menuOn=true;
static bool autorun=false;

static uint32_t lastKeyTime = 0;
static uint16_t lastKeyPressed = 0;

#ifndef  Generic_output
  /********************************
   * Generic output
  ********************************/ 
  void emu_printf(const char * text) {printf("%s\n",text);}
  void emu_printf(int val) {printf("%d\n",val);}
  void emu_printi(int val) {printf("%d\n",val);}
  void emu_printh(int val) {printf("0x%.8\n",val);}
  void emu_Free(void * pt) {free(pt);}
#endif //  Generic output

#ifndef  Input_keyboard
/********************************
 * Input and keyboard
********************************/ 

static uint16_t readAnalogJoystick(void) {
  uint16_t joysval = 0;
  if ( !gpio_get(PIN_JOY2_1) ) joysval |= MASK_JOY2_DOWN;
  if ( !gpio_get(PIN_JOY2_2) ) joysval |= MASK_JOY2_UP;
  if ( !gpio_get(PIN_JOY2_3) ) joysval |= MASK_JOY2_LEFT;
  if ( !gpio_get(PIN_JOY2_4) ) joysval |= MASK_JOY2_RIGHT;
  joysval |= (gpio_get(PIN_JOY2_BTN) ? 0 : MASK_JOY2_BTN);
  return (joysval);     
}

int emu_SwapJoysticks(int statusOnly) {
  if (!statusOnly) if (joySwapped) joySwapped = false; else joySwapped = true;
  return(joySwapped?1:0);
}

int emu_GetPad(void) {return(bLastState);}

int emu_ReadKeys(void) {
  uint16_t retval = 0;
  
  // Abilita i tasti combinati solo quando il menu non è attivo (emulatore avviato)
  if (!menuOn) {
    if ( !gpio_get(MASK_JOY2_LEFT) && !gpio_get(MASK_JOY2_RIGHT)  ) retval |= MASK_KEY_USER1;
    if ( !gpio_get(MASK_JOY2_UP) && !gpio_get(MASK_JOY2_DOWN) ) retval |= MASK_KEY_USER2;
  }
  
  uint16_t j1 = readAnalogJoystick();
  uint16_t j2 = 0;
  if (joySwapped) retval = ((j1 << 8) | j2); else retval = ((j2 << 8) | j1);
  return (retval);
}

unsigned short emu_DebounceLocalKeys(void) {
  uint16_t bCurState = emu_ReadKeys();
  uint16_t bClick = bCurState & ~bLastState;
  bLastState = bCurState;
  return (bClick);
}

void emu_InitJoysticks(void) { 
  gpio_set_pulls(PIN_JOY2_1,true,false);
  gpio_set_dir(PIN_JOY2_1,GPIO_IN);
  gpio_set_input_enabled(PIN_JOY2_1, true);    

  gpio_set_pulls(PIN_JOY2_2,true,false);
  gpio_set_dir(PIN_JOY2_2,GPIO_IN);  
  gpio_set_input_enabled(PIN_JOY2_2, true);      

  gpio_set_pulls(PIN_JOY2_3,true,false);
  gpio_set_dir(PIN_JOY2_3,GPIO_IN);  
  gpio_set_input_enabled(PIN_JOY2_3, true);         

  gpio_set_pulls(PIN_JOY2_4,true,false);
  gpio_set_dir(PIN_JOY2_4,GPIO_IN);  
  gpio_set_input_enabled(PIN_JOY2_4, true); 
  
  gpio_set_pulls(PIN_JOY2_BTN,true,false);
  gpio_set_dir(PIN_JOY2_BTN,GPIO_IN);  
  gpio_set_input_enabled(PIN_JOY2_BTN, true); 
}

int emu_setKeymap(int index) { return 0; }

#endif //  Input and keyboard

#ifndef  Menu_File_loader
/********************************
 * Menu file loader UI
********************************/ 
#include "ff.h"
static FATFS fatfs;
static FIL file; 
extern "C" int sd_init_driver(void);

#ifdef FILEBROWSER


static void drawFilenameNoExt(int x, int y, const char* filename, uint16_t color, uint16_t bgcolor) {
  char displayName[MAX_FILENAME_SIZE];
  strncpy(displayName, filename, MAX_FILENAME_SIZE - 1);
  char* dot = strrchr(displayName, '.');
  if (dot && dot != displayName) *dot = '\0'; // taglia estensione
  display.drawTextNoDma(x, y, displayName, color, bgcolor, true);
}


static void drawMenuLine(int i, int fileIndex) {
    if (fileIndex >= nbFiles) return;

    char* filename = files[fileIndex];
    uint16_t color = (fileIndex == curFile) ? RGBVAL16(0xff, 0xff, 0x00) : RGBVAL16(0xff, 0xff, 0xff);
    uint16_t bgcolor = (fileIndex == curFile) ? RGBVAL16(0xff, 0x00, 0x00) : MENU_FILE_BGCOLOR;

    // Pulisce la riga
    display.fillRectNoDma(MENU_FILE_XOFFSET, i * TEXT_HEIGHT + MENU_FILE_YOFFSET, MENU_FILE_W, TEXT_HEIGHT, bgcolor);

    // Controlla se è directory
    char fullpath[MAX_FILENAME_PATH];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", selection, filename);

    FILINFO entry;
    FRESULT fr = f_stat(fullpath, &entry);
    bool isDir = (fr == FR_OK) && (entry.fattrib & AM_DIR);
    if (strcmp(filename, "..") == 0 || isDir) {
        display.drawTextNoDma(MENU_FILE_XOFFSET, i * TEXT_HEIGHT + MENU_FILE_YOFFSET, "> ", color, bgcolor, true);
    }

    drawFilenameNoExt(MENU_FILE_XOFFSET + 16, i * TEXT_HEIGHT + MENU_FILE_YOFFSET, filename, color, bgcolor);
}

static int readNbFiles(char * rootdir) {
  int totalFiles = 0;

  DIR dir;
  FILINFO entry;

    if (strcmp(rootdir, "/") != 0 && strcmp(rootdir, ROMSDIR) != 0) {
        strcpy(files[totalFiles], "..");
        totalFiles++;
    }

  FRESULT fr = f_findfirst(&dir, &entry, rootdir, "*");
  while ((fr == FR_OK) && entry.fname[0] && (totalFiles < MAX_FILES)) {
    if (strcmp(entry.fname, ".") == 0 || strcmp(entry.fname, "..") == 0) {
      fr = f_findnext(&dir, &entry);
      continue;
    }

    strncpy(files[totalFiles], entry.fname, MAX_FILENAME_SIZE - 1);
    totalFiles++;
    fr = f_findnext(&dir, &entry);
  }
  f_closedir(&dir);
  return totalFiles;
}  

static void drawMenuItems() {
    if (curFile < nbFiles) strcpy(selected_filename, files[curFile]);

    // Calcola la finestra visibile
int currentPage = curFile / MAX_MENULINES;
int start = currentPage * MAX_MENULINES;
int end = std::min(nbFiles, start + MAX_MENULINES);
   // start = std::max(0, start);
    

    // Pulisce l'intera area del menu per evitare residui della pagina precedente
    display.fillRectNoDma(MENU_FILE_XOFFSET, MENU_FILE_YOFFSET, 
                        MENU_FILE_W, MENU_FILE_H, MENU_FILE_BGCOLOR);

    for (int i = start; i < end; i++) {
        int displayPos = i - start;
        char* filename = files[i];
        uint16_t color = (i == curFile) ? RGBVAL16(0xff,0xff,0x00) : RGBVAL16(0xff,0xff,0xff);
        uint16_t bgcolor = (i == curFile) ? RGBVAL16(0xff,0x00,0x00) : MENU_FILE_BGCOLOR;

        // Disegna la riga
        drawMenuLine(displayPos, i);
    }
char pageInfo[32];
snprintf(pageInfo, sizeof(pageInfo), "Page %d/%d", currentPage+1, (nbFiles+MAX_MENULINES-1)/MAX_MENULINES);
display.drawTextNoDma(MENU_FILE_XOFFSET, MENU_FILE_YOFFSET + MENU_FILE_H + 8,
                      pageInfo, RGBVAL16(0xff,0xff,0xff), RGBVAL16(0x00,0x00,0x00), true);


}

static void backgroundMenu(void) {
    menuRedraw = true;  
    // Pulisce l'intero schermo
    display.fillScreenNoDma(RGBVAL16(0x00,0x00,0x00));
    display.drawTextNoDma(0, 0, TITLE, RGBVAL16(0x00,0xff,0xff), RGBVAL16(0x00,0x00,0xff), true);
    // Pulisce l'area del menu
    display.fillRectNoDma(MENU_FILE_XOFFSET, MENU_FILE_YOFFSET, MENU_FILE_W, MENU_FILE_H, MENU_FILE_BGCOLOR);
}


bool menuActive(void) 
{
  return (menuOn);
}

void toggleMenu(bool on) {
  if (on) {
    menuOn = true;
    backgroundMenu();
    drawMenuItems(); 
  } else {
    display.fillScreenNoDma(RGBVAL16(0x00,0x00,0x00));    
    menuOn = false;    
  }
}

int handleMenu(uint16_t /*bClick*/) {
    int oldCurFile = curFile;
    int action = ACTION_NONE;

    if (autorun) {
        toggleMenu(false);
        menuRedraw=false;
        return (ACTION_RUN);
    }  

  static uint32_t lastKeyTime = 0;
    static uint16_t lastKeyPressed = 0;
    static uint32_t debounceTime = 0;   // per filtrare i rimbalzi

    // Leggiamo lo stato attuale dei tasti
    uint16_t bCurState = emu_ReadKeys();
    uint32_t now = to_ms_since_boot(get_absolute_time());

    int keyDelay = 300;   // ms di attesa prima del repeat
    int keyRepeat = 100;  // ms tra ripetizioni successive
    int debounceMs = 150;   // ms per ignorare rimbalzi

uint16_t bClick = 0;
if (bCurState != 0) {
    if (bCurState != lastKeyPressed) {
        // tasto cambiato: applica debounce
        if (now - debounceTime > (uint32_t)debounceMs) {
            bClick = bCurState;
            lastKeyPressed = bCurState;
            lastKeyTime = now;
            debounceTime = now;
        }
    } else {
        // stesso tasto tenuto premuto → autorepeat
        if (now - lastKeyTime > (uint32_t)keyDelay) {
            bClick = bCurState;
            lastKeyTime = now - (keyDelay - keyRepeat);
        }
    }
} else {
    lastKeyPressed = 0;
}

    // Ignora UP e DOWN se sono premuti contemporaneamente (USER2)
    if ((bClick & MASK_JOY2_UP) && (bClick & MASK_JOY2_DOWN)) {
        bClick &= ~(MASK_JOY2_UP | MASK_JOY2_DOWN);
    }
    if ((bClick & MASK_JOY1_UP) && (bClick & MASK_JOY1_DOWN)) {
        bClick &= ~(MASK_JOY1_UP | MASK_JOY1_DOWN);
    }




    if ((bClick & MASK_JOY2_BTN) || (bClick & MASK_JOY1_BTN)) {
        if (curFile >= nbFiles) return ACTION_NONE;  

        char newpath[MAX_FILENAME_PATH];
        strcpy(newpath, selection);

        if (strcmp(selected_filename, "..") == 0) {
            char* lastSlash = strrchr(newpath, '/');
            if (lastSlash) {
                *lastSlash = '\0';
                if (newpath[0] == '\0') strcpy(newpath, "/");
            }
        } else {
            if (strcmp(newpath, "/") != 0) strcat(newpath, "/");
            strcat(newpath, selected_filename);
        }

        FILINFO entry;
        if (f_stat(newpath, &entry) == FR_OK) {
            if (entry.fattrib & AM_DIR) {
                strcpy(selection, newpath);
                curFile = 0;
                strcpy(selected_filename, files[0]);
                topFile = 0;
                nbFiles = readNbFiles(selection);
                backgroundMenu();
                drawMenuItems(); 
                return ACTION_NONE;
            } else {
                toggleMenu(false);
                return ACTION_RUN;
            }
        }
    }

    // Gestione navigazione
    if (bClick & (MASK_JOY2_UP | MASK_JOY1_UP)) {
        if (curFile > 0) {
            curFile--;
            strcpy(selected_filename, files[curFile]);
            int newTop = (curFile / MAX_MENULINES) * MAX_MENULINES;
            if (newTop != topFile) {
                topFile = newTop;
                drawMenuItems();
            } else {
                drawMenuLine(oldCurFile - topFile, oldCurFile);
                drawMenuLine(curFile - topFile, curFile);
            }
        }
    }
    else if (bClick & (MASK_JOY2_DOWN | MASK_JOY1_DOWN)) {
        if (curFile < nbFiles - 1) {
            curFile++;
            strcpy(selected_filename, files[curFile]);
            int newTop = (curFile / MAX_MENULINES) * MAX_MENULINES;
            if (newTop != topFile) {
                topFile = newTop;
                drawMenuItems();
            } else {
                drawMenuLine(oldCurFile - topFile, oldCurFile);
                drawMenuLine(curFile - topFile, curFile);
            }
        }
    }
    else if ((bClick & MASK_JOY2_RIGHT) || (bClick & MASK_JOY1_RIGHT)) {
        // Vai alla pagina precedente
        int currentPage = curFile / MAX_MENULINES;
        if (currentPage > 0) {
            curFile = (currentPage - 1) * MAX_MENULINES;
            strcpy(selected_filename, files[curFile]);
            topFile = (curFile / MAX_MENULINES) * MAX_MENULINES;
            drawMenuItems();
        }
    }  
    else if ((bClick & MASK_JOY2_LEFT) || (bClick & MASK_JOY1_LEFT)) {
        // Vai alla pagina successiva
        int currentPage = curFile / MAX_MENULINES;
        int maxPage = (nbFiles - 1) / MAX_MENULINES;
        if (currentPage < maxPage) {
            curFile = (currentPage + 1) * MAX_MENULINES;
            if (curFile >= nbFiles) curFile = nbFiles - 1;
            strcpy(selected_filename, files[curFile]);
            topFile = (curFile / MAX_MENULINES) * MAX_MENULINES;
            drawMenuItems();
        }
    }
    else if ((bClick & MASK_KEY_USER1)) {
        emu_SwapJoysticks(0);
        // Disegna lo stato dello swap joystick
        display.drawTextNoDma(350, 300, 
                            (emu_SwapJoysticks(1) ? "SWAP=1" : "SWAP=0"),
                            RGBVAL16(0x00,0xff,0xff), RGBVAL16(0x00,0x00,0xff), false);
    } 
    return action;
}

char * menuSelection(void) {
  static char fullpath[MAX_FILENAME_PATH + MAX_FILENAME_SIZE];
  snprintf(fullpath, sizeof(fullpath), "%s%s%s",
    selection,
    (strcmp(selection, "/") != 0) ? "/" : "",
    selected_filename);
  return fullpath;
}
#endif
#endif //  Menu file loader UI ^^^^

#ifndef  File_IO
/********************************
 * File IO
********************************/ 
int emu_FileOpen(const char * filepath, const char * mode)
{
  int retval = 0;

  emu_printf("FileOpen...");
  emu_printf(filepath);
  if( !(f_open(&file, filepath, FA_READ)) ) {
    retval = 1;  
  }
  else {
    emu_printf("FileOpen failed");
  }
  return (retval);
}

int emu_FileRead(void * buf, int size, int handler)
{
  unsigned int retval=0; 
  f_read (&file, (void*)buf, size, &retval);
  return retval; 
}

int emu_FileGetc(int handler)
{
  unsigned char c;
  unsigned int retval=0;
  if( !(f_read (&file, &c, 1, &retval)) )
  if (retval != 1) {
    emu_printf("emu_FileGetc failed");
  }  
  return (int)c; 
}

void emu_FileClose(int handler)
{
  f_close(&file); 
}

int emu_FileSeek(int handler, int seek, int origin)
{
  f_lseek(&file, seek);
  return (seek);
}

int emu_FileTell(int handler)
{
  return (f_tell(&file));
}


unsigned int emu_FileSize(const char * filepath)
{
  int filesize=0;
  emu_printf("FileSize...");
  emu_printf(filepath);
  FILINFO entry;
  f_stat(filepath, &entry);
  filesize = entry.fsize; 
  return(filesize);    
}

unsigned int emu_LoadFile(const char * filepath, void * buf, int size)
{
  int filesize = 0;
    
  emu_printf("LoadFile...");
  emu_printf(filepath);
  if( !(f_open(&file, filepath, FA_READ)) ) {
    filesize = f_size(&file);
    emu_printf(filesize);
    if (size >= filesize)
    {
      unsigned int retval=0;
      if( (f_read (&file, buf, filesize, &retval)) ) {
        emu_printf("File read failed");        
      }
    }
    f_close(&file);
  }
 
  return(filesize);
}

static FIL outfile; 

static bool emu_writeGfxConfig(void)
{
  bool retval = false;
  if( !(f_open(&outfile, "/" GFX_CFG_FILENAME, FA_CREATE_NEW | FA_WRITE)) ) {
    f_close(&outfile);
    retval = true;
  } 
  return retval;   
}

static bool emu_readGfxConfig(void)
{
  bool retval = false;
  if( !(f_open(&outfile, "/" GFX_CFG_FILENAME, FA_READ)) ) {
    f_close(&outfile);
    retval = true;
  }  
  return retval;   
}

static bool emu_eraseGfxConfig(void)
{
  f_unlink ("/" GFX_CFG_FILENAME);
  return true;
}

static bool emu_writeConfig(void)
{
  bool retval = false;
  if( !(f_open(&outfile, ROMSDIR "/" AUTORUN_FILENAME, FA_CREATE_NEW | FA_WRITE)) ) {
    unsigned int sizeread=0;
    if( (f_write (&outfile, selection, strlen(selection), &sizeread)) ) {
      emu_printf("Config write failed");        
    }
    else {
      retval = true;
    }  
    f_close(&outfile);   
  } 
  return retval; 
}

static bool emu_readConfig(void)
{
  bool retval = false;
  if( !(f_open(&outfile, ROMSDIR "/" AUTORUN_FILENAME, FA_READ)) ) {
    unsigned int filesize = f_size(&outfile);
    unsigned int sizeread=0;
    if( (f_read (&outfile, selection, filesize, &sizeread)) ) {
      emu_printf("Config read failed");        
    }
    else {
      if (sizeread == filesize) {
        selection[filesize]=0;
        retval = true;
      }
    }  
    f_close(&outfile);   
  }  
  return retval; 
}

static bool emu_eraseConfig(void)
{
  f_unlink (ROMSDIR "/" AUTORUN_FILENAME);
  return true;
}

#endif //  File IO ^^^^

#ifndef  Initialization
/********************************
 * Initialization
********************************/ 
void emu_init(void)
{
//bool forceVga = false;
#ifdef FILEBROWSER
  sd_init_driver(); 
  FRESULT fr = f_mount(&fatfs, "0:", 1);    

 // forceVga = emu_readGfxConfig();

  strcpy(selection,ROMSDIR);
  nbFiles = readNbFiles(selection); 

  emu_printf("SD initialized, files found: ");
  emu_printi(nbFiles);
#endif

  emu_InitJoysticks();
#ifdef SWAP_JOYSTICK
  joySwapped = true;   
#else
  joySwapped = false;   
#endif  

int keypressed = emu_ReadKeys();

   if (keypressed & MASK_JOY2_DOWN) {
    display.fillScreenNoDma( RGBVAL16(0xff,0x00,0x00) );
    display.drawTextNoDma(64,48,    (char*)" AUTURUN file erased", RGBVAL16(0xff,0xff,0x00), RGBVAL16(0xff,0x00,0x00), true);
    display.drawTextNoDma(64,48+24, (char*)"Please reset the board!", RGBVAL16(0xff,0xff,0x00), RGBVAL16(0xff,0x00,0x00), true);
    emu_eraseConfig();
  }
  else {
    if (emu_readConfig()) {
      autorun = true;
    }
  }  

#ifdef FILEBROWSER
  toggleMenu(true);
#endif  
}


void emu_start(void)
{
  display.ili9486_pio_init();
  display.ili9486_dma_init();
  usbnavpad = 0;

  keyMap = 0;
}

#endif //  Initialization ^^^^