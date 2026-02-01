# MCUME-C64-8-BIT-parallel-lcd-320x240
MCUME emulator modified to work with a 3.2-inch 8-bit parallel 320x240 LCD

Components:
Rasberry PICO Purple
LCD - RB3205 34 PIN ILI 9341 PARALLEL 8 BIT
AMPLI - PAM8302A
DC-DC converter
Capacitor 470 uf
n. 11 micro buttons

original code: github.com/Jean-MarcHarvengt/MCUME/

Button functions:

left/right/up/down/fire - control joystick, virtual keyboard and start menu.

user 1 - configurable button

key/joy - joystick or virtual keyboard selection

set - in emulation opens setting menu (Joystic port, audio volume, color palette, user function button), in initial menu delete the files in SD, confirm with fire, cancel with set

plus/minus - volume control, used to control parameters in setting menu

reset - resets the pico

SD reader integrated into the LCD display is used. Inside the SD, insert only .PRG files organized into multiple folders, maximum 64 folders and 64 files per folder. Initial menu displays SD contents.

AUDIO FILTER:
to improve the PWM audio output from the pico add PWM pin---R680---x---C10nf---gnd x---R680---o---C10nf---gnd o---PAM pin A+
To eliminate noise due to the pico's USB Vbus power supply, the PAM is powered at 3.3 V by a mini DC-DC converter, 470uf capacitor must be placed near the PAM.

![IMG_4342](https://github.com/user-attachments/assets/048bebeb-d719-4f3f-8421-526b48eb2785)
![IMG_4350](https://github.com/user-attachments/assets/4a342a9d-7391-4c0e-8d39-aad7f20643e1)
