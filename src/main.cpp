// #define PREFER_SDFAT_LIBRARY true
/* there is a mess in here, where the Adafruit_VS1053 music requires SD.h (it has the #if defined(PREFER_SDFAT_LIBRARY) but seems like things are not matching up at these used versions)
// example SdFat.h and SD.h mismatch: SdFile vs File
// SdFat does not have File (used in Adafruit_VS1053.h!); and also doesn't have File.openNextFile() which I use in main
// so, do not "#define PREFER_SDFAT_LIBRARY true" and let Adafruit_VS1053 use SD.h ...


// SdFat use:
"SdFat SDfs;  "  which is passed to the adafruit ImageReader
"SdFile lastPlayedFile;" which is then used to read and write to the SD card

// vs SD functions which I use with
int countDirectory(File); and "dir.openNextFile()" in countDirectory(File) function...
int countMP3Files(File);
String findBMP(File); (not needed, commented out)
and is used internal to Adafruit_VS1053


So: maybe in the future can adjust to move to all SdFat if I just take care of the 'countDirectory' and 'countMP3Files' functions?

*/

// color defines
#define WHITE 0xFFFF
#define BLACK 0x0000
#define LIGHTGREEN 0x9772
#define ORANGE 0xFD20
#define RED 0xF800

// for display
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>

// for display: bmp image display
#include <Adafruit_ImageReader.h> // Image-reading functions
#include <SdFat.h>                // SD card & FAT filesystem library --> needed for Adafruit ImageReader Arduino Library
#include <Adafruit_SPIFlash.h>    // SPI / QSPI flash library --> expect needed based on Adafruit ImageReader Arduino Library

// for musicmaker
// #include <SPI.h>
// #include <SD.h> // Adafruit_VS1053.h includes the SD already
#include <Adafruit_VS1053.h>

/*
// for neopixel
#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel neoPixel = Adafruit_NeoPixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
*/

// power monitor
#include "Adafruit_MAX1704X.h"
Adafruit_MAX17048 maxlipo;

// for buttons
#include "Adafruit_seesaw.h"

Adafruit_seesaw ss;

// GPIO pins on the Joy Featherwing for reading button presses. These should not be changed.
#define BUTTON_RIGHT 6
#define BUTTON_DOWN 7
#define BUTTON_LEFT 9
#define BUTTON_UP 10
#define BUTTON_SEL 14
// GPIO Analog pins on the Joy Featherwing for reading the analog stick. These should not be changed.
#define STICK_H 3
#define STICK_V 2

uint32_t button_mask = (1 << BUTTON_RIGHT) | (1 << BUTTON_DOWN) | (1 << BUTTON_LEFT) | (1 << BUTTON_UP) | (1 << BUTTON_SEL);

// Use dedicated hardware SPI pins for TFT
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

SdFat SDfs;                        // SD card filesystem
Adafruit_ImageReader reader(SDfs); // Image-reader object, pass in SD filesys

Adafruit_Image img; // An image loaded into RAM
int32_t width = 0,  // BMP image dimensions
    height = 0;

// for onboard LED:
int led = LED_BUILTIN;

SdFile lastPlayedFile;

// music maker setup
#define VS1053_RESET -1 // VS1053 reset pin (not used!) \
                         // Feather ESP32 \
                         //#elif defined(ESP32) && !defined(ARDUINO_ADAFRUIT_FEATHER_ESP32S2)

#define VS1053_CS 6   // VS1053 chip select pin (output)
#define VS1053_DCS 10 // VS1053 Data/command select pin (output)
#define CARDCS 5      // Card chip select pin
#define VS1053_DREQ 9 // VS1053 Data request, ideally an Interrupt pin

Adafruit_VS1053_FilePlayer musicPlayer =
    Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);

// program checking:
bool debug = false; // debug couts and function checks enabled or not

int8_t volume = 36; // initial volume

// initial book and track, to load up from SD card file in setup, then use in void loop
int seriesInd = 1;
int bookInd = 1;
int trackInd = 1;

char coverfilename[21];
char playtrackfilename[21]; // check length
char activedirname[21];     // check length
char bookDir[21];           // longer than needed
char seriesDir[21];         // longer than needed

char lastTrackLine[10]; // longer line than needed for load last track

// global variable defines (not initialized values with functions yet!)

// initialize global variables
// button debounce and press variables
int last_x = 0, last_y = 0;
bool bA = false, bB = false, bX = false, bY = false, bS = false;
bool bAp = false, bBp = false, bXp = false, bYp = false, bSp = false;

bool sL = false, sR = false, sU = false, sD = false;
bool sLp = false, sRp = false, sUp = false, sDp = false;

// timer variables, for browser and tft lighting
int idlecounter = 0;
int browsercounter = 0;

bool screenOn = true;
bool browsermode = false;

int screenOnSec = 60, browserModeSec = 30, delayMillis = 100;
int browserOutCount = (browserModeSec * 1000 / delayMillis);
int screenOffCount = (screenOnSec * 1000 / delayMillis);

int seriesIndB;
int bookIndB;
int trackIndB;

bool userPlaying = false;

int numTracks;
int numSeries;

// function prototypes
void tftPrintTitle();
int countDirectory(File);
int countMP3Files(File);
// String findBMP(File); // not actually used
void batteryCapacity();
void displayPlaying();
void displayBrowse(int, int);
int numSeriesCount();
int numBooksCount(int);
int numTracksCount(int, int);
void PlayTrack(int, int, int);

//------------------------------------------------------------------------------
void setup()
{
  // Some boards work best if we also make a serial connection
  Serial.begin(115200);

  ImageReturnCode stat; // Status from image-reading functions

  // turn on backlite
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);

  // turn on the TFT / I2C power supply
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  delay(10);

  // initialize TFT
  tft.init(135, 240); // Init ST7789 240x135
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  // show title
  tftPrintTitle();

  // set LED to be an output pin
  pinMode(led, OUTPUT);

  if (debug)
    tft.print("LED output OK");
  digitalWrite(led, HIGH); // turn the LED on (HIGH is the voltage level)

  // initialise the music player
  if (!musicPlayer.begin())
  {
    Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
    while (1)
      ; // don't do anything more
  }
  if (debug)
    tft.println("VS1053 found");

  if (!SD.begin(CARDCS))
  {
    Serial.println(F("SD failed, or not present"));
    if (debug)
      tft.print(" / SD fail");
    while (1)
      ; // don't do anything more
  }
  Serial.println("SD OK!");

  if (!SDfs.begin(CARDCS))
  { // Breakouts require 10 MHz limit due to longer wires
    Serial.println(F("SDfat begin() failed"));
    for (;;)
      ; // Fatal error, do not continue
  }
  Serial.println("SDfat OK!");

  // load up last playing track from SD card:
  if (lastPlayedFile.open("lastPlayed.txt", O_READ))
  {

    lastPlayedFile.fgets(lastTrackLine, sizeof(lastTrackLine));
    seriesInd = atoi(lastTrackLine);

    lastPlayedFile.fgets(lastTrackLine, sizeof(lastTrackLine));
    bookInd = atoi(lastTrackLine);

    lastPlayedFile.fgets(lastTrackLine, sizeof(lastTrackLine));
    trackInd = atoi(lastTrackLine);

    lastPlayedFile.close();
  }

  Serial.println(seriesInd);
  Serial.println(bookInd);
  Serial.println(trackInd);

  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(volume, volume);

  // If DREQ is on an interrupt pin we can do background
  // audio playing
  musicPlayer.useInterrupt(18); // VS1053_FILEPLAYER_TIMER0_INT); // 18); //VS1053_FILEPLAYER_PIN_INT);  // DREQ int

  /*
    neoPixel.begin();
    neoPixel.clear();
    neoPixel.show();
    */

  // battery monitor initialize
  if (!maxlipo.begin())
  {
    Serial.println(F("Couldnt find Adafruit MAX17048?\nMake sure a battery is plugged in!"));
    while (1)
      delay(10);
  }
  Serial.print(F("Found MAX17048"));
  Serial.print(F(" with Chip ID: 0x"));
  Serial.println(maxlipo.getChipID(), HEX);

  // start joyfeatherwing
  if (!ss.begin(0x49))
  {

    while (1)
      Serial.println("ERROR! seesaw not found");
  }
  else
  {
    Serial.println("seesaw started");
    Serial.print("version: ");
    Serial.println(ss.getVersion(), HEX);
  }
  ss.pinModeBulk(button_mask, INPUT_PULLUP);
  ss.setGPIOInterrupts(button_mask, 1);

  // initialize global variables with calcs and functions

  screenOn = true;
  browsermode = false;

  // browserOutCount = (browserModeSec * 1000 / delayMillis);
  // screenOffCount = (screenOnSec * 1000 / delayMillis);

  seriesIndB = seriesInd;
  bookIndB = bookInd;
  trackIndB = trackInd;

  userPlaying = false;

  numTracks = numTracksCount(seriesInd, bookInd);
  numSeries = numSeriesCount();

  // display the screen you want to see at bootup
  displayPlaying();
}

// int numTracks = numTracksCount(seriesInd, bookInd);
// int numSeries = numSeriesCount();

// bool userPlaying = false;

void loop()
{
  int x = ss.analogRead(STICK_V);
  int y = ss.analogRead(STICK_H);

  if ((abs(x - last_x) > 3) || (abs(y - last_y) > 3))
  {
    if (debug)
      Serial.print(x);
    if (debug)
      Serial.print(", ");
    if (debug)
      Serial.println(y);
    last_x = x;
    last_y = y;
  }

  // stick select identify and debounce
  if (x < 350)
  {
    if (!(sU))
    {
      Serial.println("Stick Up");
      sU = true;
      sUp = true;
      idlecounter = 0;
      browsercounter = 0;
      browsermode = true;
    }
    else
    {
      sUp = false;
    }
  }
  else
  {
    sU = false;
    sUp = false;
  }

  if (x > 550)
  {
    if (!(sD))
    {
      Serial.println("Stick Down");
      sD = true;
      sDp = true;
      idlecounter = 0;
      browsercounter = 0;
      browsermode = true;
    }
    else
    {
      sDp = false;
    }
  }
  else
  {
    sD = false;
    sDp = false;
  }

  if (y < 300)
  {
    if (!(sL))
    {
      Serial.println("Stick Left");
      sL = true;
      sLp = true;
      idlecounter = 0;
      browsercounter = 0;
      browsermode = true;
    }
    else
    {
      sLp = false;
    }
  }
  else
  {
    sL = false;
    sLp = false;
  }

  if (y > 600)
  {
    if (!(sR))
    {
      Serial.println("Stick Right");
      sR = true;
      sRp = true;
      idlecounter = 0;
      browsercounter = 0;
      browsermode = true;
    }
    else
    {
      sRp = false;
    }
  }
  else
  {
    sR = false;
    sRp = false;
  }

  uint32_t buttons = ss.digitalReadBulk(button_mask);

  // button click debounce system
  if (!(buttons & (1 << BUTTON_RIGHT)))
  {
    if (!(bA))
    {
      Serial.println("Button A pressed = skip track forward");
      bA = true;
      bAp = true;
      idlecounter = 0;
    }
    else
    {
      bAp = false;
    }
  }
  else
  {
    bA = false;
    bAp = false;
  }

  if (!(buttons & (1 << BUTTON_DOWN)))
  {
    if (!(bB))
    {
      Serial.println("Button B pressed = volume down");
      bB = true;
      bBp = true;
      idlecounter = 0;
    }
    else
    {
      bBp = false;
    }
  }
  else
  {
    bB = false;
    bBp = false;
  }

  if (!(buttons & (1 << BUTTON_LEFT)))
  {
    if (!(bY))
    {
      Serial.println("Button Y pressed = skip back");
      bY = true;
      bYp = true;
      idlecounter = 0;
    }
    else
    {
      bYp = false;
    }
  }
  else
  {
    bY = false;
    bYp = false;
  }

  if (!(buttons & (1 << BUTTON_UP)))
  {
    if (!(bX))
    {
      Serial.println("Button X pressed = volume up");
      bX = true;
      bXp = true;
      idlecounter = 0;
    }
    else
    {
      bXp = false;
    }
  }
  else
  {
    bX = false;
    bXp = false;
  }

  if (!(buttons & (1 << BUTTON_SEL)))
  {
    if (!(bS))
    {
      Serial.println("Button SEL pressed = play/pause");
      bS = true;
      bSp = true;
      idlecounter = 0;
    }
    else
    {
      bSp = false;
    }
  }
  else
  {
    bS = false;
    bSp = false;
  }

  // feed data, and display musicPlayer playing/pause/stop state
  if (musicPlayer.playingMusic)
  {
    musicPlayer.feedBuffer(); // key point: feed data to keep playing music!
    // neoPixel.setPixelColor(0, 0, 5, 0);  // dim green indicator
    // neoPixel.show();
  }
  else if (musicPlayer.stopped())
  {
    // neoPixel.setPixelColor(0, 5, 0, 0);  // dim red indicator
    // neoPixel.show();
  }
  else if (musicPlayer.paused())
  {
    // neoPixel.setPixelColor(0, 0, 0, 5);  // dim blue indicator
    // neoPixel.show();
  }

  if (browsermode)
  {

    // unrelated button pushes will exit browswer mode by timeout (and trigger screen redraw back to playing)
    if (bXp || bYp || bAp || bBp)
    {
      browsercounter = browserOutCount + 1;
    }

    if (bSp)
    { // if in browser mode, bSp selects the showing book to switch to and start playing

      tft.setCursor(20, 60);
      tft.setTextColor(ST77XX_WHITE);
      tft.setTextSize(2);
      tft.print("Counting Tracks");

      seriesInd = seriesIndB;
      bookInd = bookIndB;
      trackInd = 1;
      userPlaying = true;
      numTracks = numTracksCount(seriesInd, bookInd);
      PlayTrack(seriesInd, bookInd, trackInd);
      browsercounter = browserOutCount + 1; // assure exit out of browsermode
    }

    if (sLp)
    { // if in browser mode, stick left displays the previous book in the series
      // numBooks = numBooksCount(seriesIndB);

      if (bookIndB - 1 > 0)
      {
        bookIndB = bookIndB - 1;
      }
      else
      {
        bookIndB = numBooksCount(seriesIndB);
      }
      displayBrowse(seriesIndB, bookIndB);
    }
    if (sRp)
    { // if in browser mode, stick left displays the next book in the series
      if (bookIndB + 1 > numBooksCount(seriesIndB))
      {
        bookIndB = 1;
      }
      else
      {
        bookIndB = bookIndB + 1;
      }
      displayBrowse(seriesIndB, bookIndB);
    }
    if (sUp)
    { // if in browser mode, stick up displays the first book in the previous series
      bookIndB = 1;
      if (seriesIndB - 1 > 0)
      {
        seriesIndB = seriesIndB - 1;
      }
      else
      {
        seriesIndB = numSeries;
      }
      displayBrowse(seriesIndB, bookIndB);
    }
    if (sDp)
    { // if in browser mode, stick left displays the first book in the next series
      bookIndB = 1;
      if (seriesIndB + 1 > numSeries)
      {
        seriesIndB = 1;
      }
      else
      {
        seriesIndB = seriesIndB + 1;
      }
      Serial.print("Series browse: ");
      Serial.print(seriesIndB);
      Serial.print(", Book browse: ");
      Serial.print(bookIndB);

      displayBrowse(seriesIndB, bookIndB);
    }

    browsercounter = browsercounter + 1;

    if (browsercounter > browserOutCount)
    { // if going out of browser mode, display back the current playing image and track number
      displayPlaying();
      browsermode = false;
      int seriesIndB = seriesInd;
      int bookIndB = bookInd;
      int trackIndB = trackInd;
    }
  }
  else
  {

    // button press responses in normal mode

    // select = play/pause
    if (bSp && screenOn)
    {
      // numTracks = numTracksCount(seriesInd, bookInd);

      if (musicPlayer.stopped())
      {
        userPlaying = true;
        PlayTrack(seriesInd, bookInd, trackInd);
      }
      else if (!musicPlayer.paused())
      {
        musicPlayer.pausePlaying(true);
      }
      else
      {
        Serial.print("Resumed");
        musicPlayer.pausePlaying(false);
      }
    }

    // skip forward
    if (bAp && screenOn)
    {
      if (trackInd < numTracks)
      { // numTracksCount(seriesInd, bookInd)) {
        trackInd = trackInd + 1;
        PlayTrack(seriesInd, bookInd, trackInd);
      }
      else
      {
        // blink red LED
        // neoPixel.setPixelColor(0, 255, 0, 0);  // red
        // neoPixel.show();
      }

      // skip backwards
      if (bYp && screenOn)
      {
        if (trackInd > 1)
        {
          trackInd = trackInd - 1;
          PlayTrack(seriesInd, bookInd, trackInd);
        }
        else if (trackInd == 1)
        {
          PlayTrack(seriesInd, bookInd, trackInd);
          // neoPixel.setPixelColor(0, 0, 255, 0);  // green flash, restart track, indicate was 1st
          // neoPixel.show();
        }
      }

      // volume up down
      if (bXp && screenOn && (volume > 00))
      {
        volume = volume - 4;
        if (debug)
          Serial.println(volume);
        musicPlayer.setVolume(volume, volume);
      }

      if (bBp && screenOn && (volume < 72))
      {
        volume = volume + 4;
        if (debug)
          Serial.println(volume);
        musicPlayer.setVolume(volume, volume);
      }
    }
    // all done button presses.  Do normal maintenance things here

    // screen off after idle time
    if ((idlecounter > screenOffCount) && screenOn)
    {
      digitalWrite(TFT_BACKLITE, LOW);
      screenOn = false;
    }

    // screen on if idle time is reset (and draw battery)
    if ((idlecounter < screenOffCount) && !screenOn)
    {
      digitalWrite(TFT_BACKLITE, HIGH);
      screenOn = true;
      if (idlecounter == 0)
      {
        batteryCapacity();
      }
    }

    if (screenOn == true)
    {
      idlecounter = idlecounter + 1;
    }

    if (browsermode == true)
    {
      browsercounter = browsercounter + 1;
    }

    // play next track if not end of book
    if (userPlaying && musicPlayer.stopped())
    {
      if (trackInd < numTracks)
      { // numTracksCount(seriesInd, bookInd)) {
        Serial.println("On to next track: ");
        trackInd = trackInd + 1;
        PlayTrack(seriesInd, bookInd, trackInd);
      }
      else
      {
        userPlaying = false;
        // blink led RED
        // neoPixel.setPixelColor(0, 255, 0, 0);  // dim red indicator
        // neoPixel.show();
      }
    }
  }
  delay(delayMillis);
}

void tftPrintTitle()
{
  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 30);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.println("Kids Player!");
  tft.println("From Dad, 2023");
}

/// count number of folders in dir
int countDirectory(File dir)
{
  int dirCount = 0;
  while (true)
  {
    File entry = dir.openNextFile();
    if (!entry)
    {
      // no more files
      // Serial.println("**nomorefiles**");
      break;
    }
    if (entry.isDirectory())
    {
      dirCount++;
      Serial.print(dirCount);
      Serial.print("counted dir entry is ");
      Serial.println(entry.name());
    }
  }
  return dirCount;
}

/// count number of mp3files in dir
int countMP3Files(File dir)
{
  String fileName;
  int fileCount = 0;
  while (true)
  {
    File entry = dir.openNextFile();
    if (!entry)
    {
      // no more files
      // Serial.println("**nomorefiles**");
      break;
    }
    if (!entry.isDirectory())
    {
      fileName = entry.name();
      if (fileName.endsWith("mp3"))
      {
        fileCount++;
        Serial.print(fileCount);
        Serial.print(" counted file entry is ");
        Serial.println(entry.name());
      }
    }
  }
  return fileCount;
}

// "String findBMP(File dir)" is not actually used! I assume the .bmp name
// Strangely, this uses the SD.h library callout, when the Adafruit imageReader library requires SdFat.h
// anyways, this returns a string... pretty harmless

// String findBMP(File dir) {
//   String fileName;
//   while (true) {
//     File entry = dir.openNextFile();
//     if (!entry) {
//       // no more files
//       //Serial.println("**nomorefiles**");
//       fileName = "noCover";
//       break;
//     }
//     if (!entry.isDirectory()) {
//       fileName = entry.name();
//       if (fileName.endsWith("bmp")) {
//         Serial.print("found cover entry ");
//         Serial.println(entry.name());
//         break;
//       }
//     }
//   }
//   return fileName;
// }

void batteryCapacity()
{
  // tft.fillScreen(ST77XX_YELLOW);

  int xpos = 216, ypos = 20, battWidth = 12, battHeight = 50, nubHeight = 4;
  int fillHeight = maxlipo.cellPercent() / (100 / (battHeight));
  // int testTen = 10;

  Serial.println(maxlipo.cellPercent());
  Serial.println(fillHeight);
  // Serial.println(maxlipo.cellPercent() / testTen);

  // put a battery image in top right corner
  //(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, BLACK);
  tft.drawRect(xpos - 1, ypos - 1, battWidth + 2, battHeight + 2, BLACK);                      // outside rect
  tft.fillRect(xpos, ypos, battWidth, battHeight, WHITE);                                      // fill white
  tft.fillRect(xpos + (battWidth / 4), ypos - 1 - nubHeight, battWidth / 2, nubHeight, BLACK); // make the nubbin

  // draw the battery fill %, including different colors
  if (maxlipo.cellPercent() > 40)
  {
    tft.fillRect(xpos, ypos + (battHeight - fillHeight), battWidth, fillHeight, LIGHTGREEN);
  }
  else if (maxlipo.cellPercent() < 25)
  {
    tft.fillRect(xpos, ypos + (battHeight - fillHeight), battWidth, fillHeight, RED);
  }
  else
  {
    tft.fillRect(xpos, ypos + (battHeight - fillHeight), battWidth, fillHeight, ORANGE);
  }
}

void displayPlaying()
{
  tft.fillScreen(ST77XX_BLACK);
  sprintf(coverfilename, "/%04d/%04d/cover.bmp", seriesInd, bookInd); // make coverfilename
  reader.drawBMP(coverfilename, tft, 0, 0);
  tft.setCursor(180, 100);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(4);
  tft.print(trackInd);
  batteryCapacity();
}

void displayBrowse(int seriesIndgive, int bookIndgive)
{
  tft.fillScreen(ST77XX_BLACK);
  sprintf(coverfilename, "/%04d/%04d/cover.bmp", seriesIndgive, bookIndgive); // make coverfilename
  reader.drawBMP(coverfilename, tft, 0, 0);
}

int numSeriesCount()
{
  int numSeriesresult = countDirectory(SD.open("/")) - 1; // -1 due to extra folder on file system
  // int numSeriesresult = countDirectory(SDfs.open("/")) - 1;  // -1 due to extra folder on file systemSDfs
  Serial.print("number of series on SD card is: ");
  Serial.println(numSeriesresult);
  return numSeriesresult;
}

int numBooksCount(int seriesIndgive)
{
  sprintf(seriesDir, "/%04d", seriesIndgive);
  int numBooksresult = countDirectory(SD.open(seriesDir));
  // int numBooksresult = countDirectory(SDfs.open(seriesDir));
  Serial.print("number of books in this series is: ");
  Serial.println(numBooksresult);
  return numBooksresult;
}

int numTracksCount(int seriesIndgive, int bookIndgive)
{
  sprintf(activedirname, "/%04d/%04d", seriesIndgive, bookIndgive);
  Serial.println(activedirname);
  int numTracksresult = countMP3Files(SD.open(activedirname));
  // int numTracksresult = countMP3Files(SDfs.open(activedirname));
  Serial.print("number of tracks in this book is: ");
  Serial.println(numTracksresult);
  return numTracksresult;
}

// run playtrack EVERY TIME to play the track!  This does the track make, display, and record of which track is playing
void PlayTrack(int seriesIndgive, int bookIndgive, int trackIndGive)
{
  musicPlayer.stopPlaying();

  // write data to SD card for resume from position on restart (open with O_TRUNC to clear the file)
  // https://forum.arduino.cc/t/arduino-sd-card-update-first-line/438240
  lastPlayedFile.open("lastPlayed.txt", O_WRITE | O_CREAT | O_TRUNC);
  lastPlayedFile.println(seriesIndgive);
  lastPlayedFile.println(bookIndgive);
  lastPlayedFile.println(trackIndGive);
  // lastPlayedFile.flush();
  lastPlayedFile.close(); // actually writes the file on close

  sprintf(playtrackfilename, "/%04d/%04d/%03d.mp3", seriesIndgive, bookIndgive, trackIndGive); // make track file name
  musicPlayer.startPlayingFile(playtrackfilename);

  displayPlaying(); // back to playing screen for playing file
}
