//------------------------------------------------------------------------------
// Makelangelo - firmware for various robot kinematic models
// dan@marginallycelver.com 2013-12-26
// Please see http://www.github.com/MarginallyClever/makelangeloFirmware for more information.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// INCLUDES
//------------------------------------------------------------------------------
#include "configure.h"
#include "lcd.h"
#include "sdcard.h"


#ifdef HAS_LCD

#ifdef LCD_IS_SMART
#include <LiquidCrystal.h>
#endif
#ifdef LCD_IS_128X64
#include <Arduino.h>
#include <U8glib.h>
#include <SPI.h>
#include <Wire.h>
#include "dogm_font_data_6x9.h"
#endif

//------------------------------------------------------------------------------
// GLOBALS
//------------------------------------------------------------------------------
#ifdef LCD_IS_SMART
LiquidCrystal lcd(LCD_PINS_RS, LCD_PINS_ENABLE, LCD_PINS_D4, LCD_PINS_D5, LCD_PINS_D6, LCD_PINS_D7);
#endif
#ifdef LCD_IS_128X64
// This is not ideal - will not work when board models change.
U8GLIB_ST7920_128X64_1X u8g(LCD_PINS_D4, LCD_PINS_ENABLE, LCD_PINS_RS);
#endif

uint32_t lcd_draw_delay  = 0;

int lcd_rot_old  = 0;
int lcd_turn     = 0;
int lcd_posx = 0, lcd_posy = 0;
char lcd_click_old = HIGH;
char lcd_click_now = false;
uint8_t speed_adjust = 100;
char lcd_message[LCD_MESSAGE_LENGTH + 1];
char lcd_dirty=0;

int menu_position_sum = 0, menu_position = 0, screen_position = 0, num_menu_items = 0, ty, screen_end;


void (*current_menu)();


#ifdef LCD_IS_128X64
/**
   Made with Marlin Bitmap Converter
   http://marlinfw.org/tools/u8glib/converter.html

   This bitmap from the file 'icon_mono.png'
   Filesize: 906 Bytes
   Size bitmap: 108 bytes
*/
#define logoImageWidth 32
#define logoImageHeight 32

const unsigned char logoImage [] PROGMEM = {
  0x00, 0x60, 0x00, 0x00, // .........##.....................
  0x00, 0xF0, 0x00, 0x00, // ........####....................
  0x03, 0x10, 0x00, 0x00, // ......##...#....................
  0x01, 0x60, 0x00, 0x00, // .......#.##.....................
  0x01, 0xC0, 0x00, 0x00, // .......###......................
  0x00, 0x9F, 0xF0, 0x00, // ........#..#########............
  0x00, 0xF0, 0x3C, 0x00, // ........####......####..........
  0x01, 0xC0, 0x3F, 0x00, // .......###........######........
  0x03, 0x00, 0x1F, 0x80, // ......##...........######.......
  0x06, 0x00, 0x1F, 0xC0, // .....##............#######......
  0x0C, 0x02, 0x0F, 0xE0, // ....##........#.....#######.....
  0x08, 0x06, 0x0F, 0xE0, // ....#........##.....#######.....
  0x18, 0x02, 0x0F, 0xF0, // ...##.........#.....########....
  0x10, 0x02, 0x0F, 0xF0, // ...#..........#.....########....
  0x31, 0x80, 0x07, 0xF0, // ..##...##............#######....
  0x23, 0x80, 0x07, 0xF8, // ..#...###............########...
  0x23, 0x00, 0x07, 0xF8, // ..#...##.............########...
  0x23, 0x00, 0x07, 0xF8, // ..#...##.............########...
  0x23, 0x00, 0x07, 0xF8, // ..#...##.............########...
  0x21, 0x80, 0x07, 0xF8, // ..#....##............########...
  0x20, 0x00, 0x07, 0xF8, // ..#..................########...
  0x30, 0x00, 0x37, 0xF8, // ..##..............##.########...
  0x30, 0x00, 0x63, 0xF0, // ..##.............##...######....
  0x10, 0x00, 0x63, 0xF0, // ...#.............##...######....
  0x18, 0x00, 0x63, 0xF0, // ...##............##...######....
  0x08, 0x60, 0x37, 0xE0, // ....#....##.......##.######.....
  0x04, 0x20, 0x0F, 0xC0, // .....#....#.........######......
  0x06, 0x00, 0x1F, 0xC0, // .....##............#######......
  0x03, 0x00, 0x1F, 0x80, // ......##...........######.......
  0x00, 0xC0, 0x3E, 0x00, // ........##........#####.........
  0x00, 0x70, 0x3C, 0x00, // .........###......####..........
  0x00, 0x1F, 0xE0, 0x00 // ...........########.............
};
#endif

//------------------------------------------------------------------------------
// MACROS
//------------------------------------------------------------------------------

/**
   Clear the screen
*/
#ifdef LCD_IS_SMART
inline void LCD_clear() {
  for (int i = 0; i < LCD_MESSAGE_LENGTH; ++i) lcd_message[i] = ' ';
  lcd_message[LCD_MESSAGE_LENGTH - 1] = 0;
}
#endif
#ifdef LCD_IS_128X64
inline void LCD_clear() {
  //u8g.firstPage();
  //while( u8g.nextPage() );
  u8g.setColorIndex(0);
  u8g.drawBox(0, 0, LCD_PIXEL_WIDTH, LCD_PIXEL_HEIGHT);
  u8g.setColorIndex(1);
}
#endif

/**
   print text to the LCD
*/
#ifdef LCD_IS_SMART
inline void LCD_advance() {
  lcd_posx++;
  if (lcd_posx >= LCD_WIDTH) {
    lcd_posx = 0;
    lcd_posy++;
    if (lcd_posy >= LCD_HEIGHT) {
      lcd_posy = 0;
    }
  }
}

inline void LCD_print(const char *x) {
  while ((*x) != 0) {
    lcd_message[lcd_posy * LCD_WIDTH + lcd_posx] = *x;
    x++;
    LCD_advance();
  }
}

//inline void LCD_print(const __FlashStringHelper *x) {
//  LCD_print((const PROGMEM char*)x);
//}

// see https://en.wikibooks.org/wiki/C_Programming/stdlib.h/itoa
inline void LCD_print(long x) {
  char b[12];
  itoa(x, b, 10);
  LCD_print(b);
}

inline void LCD_print(int x) {
  char b[12];
  itoa(x, b, 10);
  LCD_print(b);
}

inline void LCD_print(const char x) {
  lcd_message[lcd_posy * LCD_WIDTH + lcd_posx] = x;
  LCD_advance();
}
#endif
#ifdef LCD_IS_128X64
#define LCD_print      u8g.print
#endif


/**
   Set the row/column of text at which to begin printing
*/
#ifdef LCD_IS_SMART
#define LCD_setCursor(x,y)   {lcd_posx=x; lcd_posy=y;}
#endif
#ifdef LCD_IS_128X64
#define LCD_setCursor(x,y)   u8g.setPrintPos(((x)+1)*FONT_WIDTH,((y)+1)*FONT_HEIGHT)
#endif



// Convenience macros that make it easier to generate menus

#define MENU_START \
  ty=0;

#define MENU_END \
  num_menu_items=ty; \

#define MENU_ITEM_START(key) \
  if(ty>=screen_position && ty<screen_end) { \
    LCD_setCursor(0,ty-screen_position); \
    LCD_print((menu_position==ty)?'>':' '); \
    LCD_print(key); \

#define MENU_ITEM_END() \
  } \
  ++ty;

#define MENU_GOTO(new_menu) {  \
    lcd_click_now=false;  \
    LCD_clear();  \
    num_menu_items=screen_position=menu_position=menu_position_sum=0;  \
    screen_end = screen_position + LCD_HEIGHT;  \
    current_menu=new_menu;  \
    return;  \
  }

#define MENU_LABEL(menu_label) \
  MENU_ITEM_START(menu_label) \
  MENU_ITEM_END()

#define MENU_SUBMENU(menu_label,menu_method) \
  MENU_ITEM_START(menu_label) \
  if(menu_position==ty && lcd_click_now) MENU_GOTO(menu_method); \
  MENU_ITEM_END()

#define MENU_ACTION(menu_label,menu_method) MENU_SUBMENU(menu_label,menu_method)

#define MENU_LONG(key,value) \
  MENU_ITEM_START(key) \
  LCD_print_long(value); \
  if(menu_position==ty && lcd_click_now) LCD_update_long(key,value); \
  MENU_ITEM_END()

#define MENU_FLOAT(key,value) \
  MENU_ITEM_START(key) \
  LCD_print_float(value); \
  if(menu_position==ty && lcd_click_now) LCD_update_float(key,value); \
  MENU_ITEM_END()


//------------------------------------------------------------------------------
// METHODS
//------------------------------------------------------------------------------

void LCD_status_menu();
void LCD_main_menu();
void LCD_drawSplash();
void LCD_driveX();
void LCD_driveY();
void LCD_driveZ();
void LCD_driveF();
void LCD_print_float(float);
void LCD_print_long(long);
void LCD_refresh_display();
void LCD_draw_border();


void LCD_read() {
  // detect potentiometer changes
  int rot = ((digitalRead(BTN_EN1) == LOW) << BLEN_A)
          | ((digitalRead(BTN_EN2) == LOW) << BLEN_B);
  // potentiometer uses grey code.  Pattern is 0 3
  if (lcd_rot_old != rot) {
    switch (rot) {
      case ENCROT0:
        if ( lcd_rot_old == ENCROT3 ) lcd_turn++;
        else if ( lcd_rot_old == ENCROT1 ) lcd_turn--;
        break;
      case ENCROT1:
        if ( lcd_rot_old == ENCROT0 ) lcd_turn++;
        else if ( lcd_rot_old == ENCROT2 ) lcd_turn--;
        break;
      case ENCROT2:
        if ( lcd_rot_old == ENCROT1 ) lcd_turn++;
        else if ( lcd_rot_old == ENCROT3 ) lcd_turn--;
        break;
      case ENCROT3:
        if ( lcd_rot_old == ENCROT2 ) lcd_turn++;
        else if ( lcd_rot_old == ENCROT0 ) lcd_turn--;
        break;
    }
    //if(lcd_turn !=0) Serial.print(lcd_turn>0?'+':'-');  // for debugging potentiometer
    //else Serial.print(' ');
    //Serial.println(lcd_turn);  // for debugging potentiometer
  }
  //Serial.println(rot);  // for debugging potentiometer

  lcd_rot_old = rot;


  // find click state
  int btn = digitalRead(BTN_ENC);
  if ( btn != lcd_click_old && btn == HIGH ) {
    lcd_click_now = true;
  }
  lcd_click_old = btn;
}


void LCD_pause() {
  // TODO: if pen down before pause, lift pen on pause, lower pen on unpause.
  // problem: machine does not know what is pen up or down.
#ifdef HAS_SD
  sd_printing_paused = (sd_printing_paused == true ? false : true);
#endif
  MENU_GOTO(LCD_main_menu);
}


void LCD_stop() {
#ifdef HAS_SD
  sd_printing_now = false;
#endif
  MENU_GOTO(LCD_main_menu);
}

void LCD_disable_motors() {
  motor_disengage();
  MENU_GOTO(LCD_main_menu);
}

void LCD_enable_motors() {
  motor_engage();
  MENU_GOTO(LCD_main_menu);
}


void LCD_find_home() {
  robot_findHome();
  MENU_GOTO(LCD_main_menu);
}


void LCD_this_is_home() {
  float offset[NUM_AXIES];
  for (int i = 0; i < NUM_AXIES; ++i) offset[i] = axies[i].homePos;
  teleport(offset);
  MENU_GOTO(LCD_main_menu);
}


void LCD_go_home() {
  float homes[NUM_AXIES];
  for (int i = 0; i < NUM_AXIES; ++i) homes[i] = axies[i].homePos;
  lineSafe( homes, DEFAULT_FEEDRATE );
  MENU_GOTO(LCD_main_menu);
}


void LCD_drive_menu() {
  MENU_START
  MENU_SUBMENU("Back", LCD_main_menu);
  MENU_ACTION("Disable motors", LCD_disable_motors);
  MENU_ACTION("Enable motors", LCD_enable_motors);
  MENU_SUBMENU("X", LCD_driveX);
  MENU_SUBMENU("Y", LCD_driveY);
  MENU_SUBMENU("Z", LCD_driveZ);
  MENU_SUBMENU("Feedrate", LCD_driveF);
  MENU_END
}


void LCD_driveX() {
  if (lcd_click_now) MENU_GOTO(LCD_drive_menu);

  float offset[NUM_AXIES];
  get_end_plus_offset(offset);

  if (lcd_turn) {
    offset[0] += lcd_turn > 0 ? 1 : -1;
    lineSafe(offset, feed_rate);
  }

  LCD_setCursor( 0, 0);
  LCD_print('X');
  LCD_print_float(offset[0]);
}


void LCD_driveY() {
  if (lcd_click_now) MENU_GOTO(LCD_drive_menu);

  float offset[NUM_AXIES];
  get_end_plus_offset(offset);

  if (lcd_turn) {
    offset[1] += lcd_turn > 0 ? 1 : -1;
    lineSafe(offset, feed_rate);
  }

  LCD_setCursor( 0, 0);
  LCD_print('Y');
  LCD_print_float(offset[1]);
}


void LCD_driveZ() {
  if (lcd_click_now) MENU_GOTO(LCD_drive_menu);

  float offset[NUM_AXIES];
  get_end_plus_offset(offset);

  if (lcd_turn) {
    // protect servo, don't drive beyond physical limits
    offset[2] += lcd_turn > 0 ? 1 : -1;
    lineSafe(offset, feed_rate);
  }

  LCD_setCursor( 0, 0);
  LCD_print('Z');
  LCD_print_float(offset[2]);
}


void LCD_driveF() {
  if (lcd_click_now) MENU_GOTO(LCD_drive_menu);

  if (lcd_turn) {
    // protect servo, don't drive beyond physical limits
    float newF = feed_rate + lcd_turn > 0 ? 1 : -1;
    if (newF < MIN_FEEDRATE) newF = MIN_FEEDRATE;
    if (newF > MAX_FEEDRATE) newF = MAX_FEEDRATE;
    // move
    feed_rate = newF;
  }

  LCD_setCursor( 0, 0);
  LCD_print('F');
  LCD_print_float(feed_rate);
}


void LCD_start_menu() {
#ifdef HAS_SD
  if (!sd_inserted) MENU_GOTO(LCD_main_menu);

  /*
    Serial.print(menu_position    );  Serial.print("\t");  // 0
    Serial.print(menu_position_sum);  Serial.print("\t");  // 1
    Serial.print(screen_position  );  Serial.print("\t");  // 0
    Serial.print(num_menu_items   );  Serial.print("\n");  // 8
  */
  if(lcd_turn!=0 || lcd_click_now==1) lcd_dirty=1;

  if(lcd_dirty==1) {
    //long t0=micros();
    
    MENU_START
    MENU_SUBMENU("Back", LCD_main_menu);
    
    root.rewindDirectory();
    while ( true ) {
      //long tStart = millis();
      File entry = root.openNextFile();
      //long tEnd = millis();
      //Serial.print(tEnd-tStart);
      //Serial.print('\t');
      if (!entry) {
        // no more files, return to the first file in the directory
        break;
      }
      const char *filename = entry.name();
      //Serial.print( entry.isDirectory()?">":" " );
      //Serial.println(filename);
      if (!entry.isDirectory() && filename[0] != '_') {
        MENU_ITEM_START(filename)
        if (menu_position == ty && lcd_click_now) {
          lcd_click_now = false;
          SD_StartPrintingFile(filename);
          MENU_GOTO(LCD_status_menu);
        }
        MENU_ITEM_END()
      }
      entry.close();
    }
    MENU_END
    
    //long t1=micros();
    //Serial.print(menu_position,DEC);
    //Serial.print(' ');
    //Serial.print(num_menu_items,DEC);
    //Serial.print(' ');
    //Serial.print(t1-t0);
    //Serial.println();
    lcd_dirty=0;
  }
  
#else
  // i don't know how you got here surfing the LCD panel.
  // someone messed up in the logic.  Go back to the main menu.
  MENU_GOTO(LCD_main_menu);
#endif
}


void draw_border(int width, int height, int landscape) {
#if NUM_AXIES == 3
  LCD_clear();
  LCD_setCursor(0, 0);
  LCD_print("Drawing border...");

  width /= 2;
  height /= 2;

  if (landscape == 1) {
    // swap the two values.
    int temp = width;
    width = height;
    height = temp;
  }

  // get start position
  float start[NUM_AXIES];
  get_end_plus_offset(start);

  float pos[NUM_AXIES];
  // lift pen at current position
  pos[0] = start[0];
  pos[1] = start[1];
  pos[2] = PEN_UP_ANGLE;
  lineSafe( pos, feed_rate );
  // move to first corner
  pos[0] = -width;  pos[1] =  height;  lineSafe( pos, feed_rate );
  // lower pen
  pos[2] = PEN_DOWN_ANGLE;
  lineSafe( pos, feed_rate );
  // move around border
  pos[0] =  width;  pos[1] =  height;  lineSafe( pos, feed_rate );
  pos[0] =  width;  pos[1] = -height;  lineSafe( pos, feed_rate );
  pos[0] = -width;  pos[1] = -height;  lineSafe( pos, feed_rate );
  pos[0] = -width;  pos[1] =  height;  lineSafe( pos, feed_rate );
  // lift pen
  pos[2] = PEN_UP_ANGLE;  lineSafe( pos, feed_rate );

  // return to start position
  lineSafe( start, feed_rate );
#endif // NUM_AXIES
  MENU_GOTO(LCD_draw_border);
}

void draw_A2_portrait() {
  draw_border(420, 594, 0);
}
void draw_A3_portrait() {
  draw_border(297, 420, 0);
}
void draw_A4_portrait() {
  draw_border(210, 297, 0);
}
void draw_A5_portrait() {
  draw_border(148, 210, 0);
}
void draw_USletter_portrait() {
  draw_border(216, 279, 0);
}
void draw_USlegal_portrait() {
  draw_border(216, 356, 0);
}

void draw_A2_landscape() {
  draw_border(420, 594, 1);
}
void draw_A3_landscape() {
  draw_border(297, 420, 1);
}
void draw_A4_landscape() {
  draw_border(210, 297, 1);
}
void draw_A5_landscape() {
  draw_border(148, 210, 1);
}
void draw_USletter_landscape() {
  draw_border(216, 279, 1);
}
void draw_USlegal_landscape() {
  draw_border(216, 356, 1);
}

void LCD_draw_border() {
  MENU_START
  MENU_SUBMENU("Back", LCD_main_menu);
  MENU_ACTION("A2 portrait", draw_A2_portrait);
  MENU_ACTION("A3 portrait", draw_A3_portrait);
  MENU_ACTION("A4 portrait", draw_A4_portrait);
  MENU_ACTION("A5 portrait", draw_A5_portrait);
  MENU_ACTION("US legal portrait", draw_USlegal_portrait);
  MENU_ACTION("US letter portrait", draw_USletter_portrait);

  MENU_ACTION("A2 landscape", draw_A2_landscape);
  MENU_ACTION("A3 landscape", draw_A3_landscape);
  MENU_ACTION("A4 landscape", draw_A4_landscape);
  MENU_ACTION("A5 landscape", draw_A5_landscape);
  MENU_ACTION("US legal landscape", draw_USlegal_landscape);
  MENU_ACTION("US letter landscape", draw_USletter_landscape);
  MENU_END
}


void LCD_update_long(char *label, long &value) {
  LCD_clear();
  do {
    LCD_read();
    if ( lcd_turn ) {
      value += lcd_turn > 0 ? 1 : -1;
      lcd_turn = 0;
    }
    LCD_setCursor(0, 0);
    LCD_print(label);
    LCD_setCursor(0, 1);
    LCD_print_long(value);
  } while ( !lcd_click_now );
}


void LCD_update_float(char *label, float &value, float stepSize = 0.01) {
  LCD_clear();
  do {
    LCD_read();
    if ( lcd_turn ) {
      value += lcd_turn > 0 ? stepSize : -stepSize;
      lcd_turn = 0;
    }
    LCD_setCursor(0, 0);
    LCD_print(label);
    LCD_setCursor(0, 1);
    LCD_print_float(value);
  } while ( !lcd_click_now );
}


void LCD_print_long(long v) {
  long av = abs(v);
  int x = 1000;
  while (x > av && x > 1) {
    LCD_print(' ');
    x /= 10;
  };
  if (v > 0) LCD_print(' ');
  LCD_print(v);
}



void LCD_print_float(float v) {
  int left = abs(v);
  int right = abs((int)(v * 100) % 100);

  if (left < 1000) LCD_print(' ');
  if (left < 100) LCD_print(' ');
  if (left < 10) LCD_print(' ');

  LCD_print(left);
  LCD_print('.');
  LCD_print(right);
}
#endif  // HAS_LCD




void LCD_update() {
#ifdef HAS_LCD
  LCD_read();

  if (millis() >= lcd_draw_delay ) {
    lcd_draw_delay = millis() + LCD_DRAW_DELAY;

    //Serial.print(lcd_turn,DEC);
    //Serial.print('\t');  Serial.print(menu_position,DEC);
    //Serial.print('\t');  Serial.print(menu_position_sum,DEC);
    //Serial.print('\t');  Serial.print(screen_position,DEC);
    //Serial.print('\t');  Serial.print(screen_end,DEC);
    //Serial.print('\t');  Serial.print(num_menu_items,DEC);
    //Serial.print('\n');

    // update the menu position
    if ( lcd_turn && num_menu_items > 1 ) {
      int originalPosition = menu_position_sum / LCD_TURN_PER_MENU;
      menu_position_sum += lcd_turn;

      menu_position = menu_position_sum / LCD_TURN_PER_MENU;
      if (menu_position > num_menu_items - 1) menu_position = num_menu_items - 1;
      if (menu_position < 0) menu_position = 0;

      if (originalPosition != menu_position) {
        LCD_clear();
      }

      //Serial.println(menu_position);

      if (screen_position > menu_position) screen_position = menu_position;
      if (screen_position < menu_position - (LCD_HEIGHT - 1)) screen_position = menu_position - (LCD_HEIGHT - 1);
      screen_end = screen_position + LCD_HEIGHT;
    }
    
    // draw the new screen contents
    #ifdef LCD_IS_128X64
    u8g.firstPage();
    do {
    #endif
      (*current_menu)();
    #ifdef LCD_IS_128X64
    } while(u8g.nextPage());
    #endif

    lcd_turn = 0;  // reset.  must come after (*current_menu)() because it might be used there.  (damn globals...)
  }
  LCD_refresh_display();
#endif  // HAS_LCD
}


void LCD_refresh_display() {
#ifdef HAS_LCD
#ifdef LCD_IS_SMART
  char temp[LCD_MESSAGE_LENGTH];
  memcpy(temp + (LCD_WIDTH * 0), lcd_message + (LCD_WIDTH * 0), LCD_WIDTH);
  memcpy(temp + (LCD_WIDTH * 1), lcd_message + (LCD_WIDTH * 2), LCD_WIDTH);
  memcpy(temp + (LCD_WIDTH * 2), lcd_message + (LCD_WIDTH * 1), LCD_WIDTH);
  memcpy(temp + (LCD_WIDTH * 3), lcd_message + (LCD_WIDTH * 3), LCD_WIDTH);
  for (int i = 0; i < LCD_MESSAGE_LENGTH-1; ++i) {
    lcd.write(temp[i]);
  }
#endif
#endif  // HAS_LCD
}


void LCD_main_menu() {
#ifdef HAS_LCD
  lcd_dirty=1;
  
  MENU_START

  MENU_SUBMENU("Back", LCD_status_menu);
#ifdef HAS_SD
  if (!sd_printing_now) {
#endif
#if MACHINE_HARDWARE_VERSION  == 5
    MENU_ACTION("Find home", LCD_find_home);
#else
    MENU_ACTION("This is home", LCD_this_is_home);
    MENU_ACTION("Go home", LCD_go_home);
#endif
#ifdef HAS_SD
    if (sd_inserted) {
      MENU_SUBMENU("Print from SD card", LCD_start_menu);
    } else {
      MENU_LABEL("No SD card");
    }
#endif
#if NUM_AXIES == 3
    MENU_SUBMENU("Draw border", LCD_draw_border);
#endif
    MENU_SUBMENU("Drive", LCD_drive_menu);
#ifdef HAS_SD
  } else {
    if (sd_printing_paused) {
      MENU_ACTION("Unpause", LCD_pause);
    } else {
      MENU_ACTION("Pause", LCD_pause);
    }
    MENU_ACTION("Stop", LCD_stop);
  }
#endif
  MENU_END
#endif  // HAS_LCD
}


// display the current machine position and feed rate on the LCD.
void LCD_status_menu() {
#ifdef HAS_LCD
  MENU_START

  // on click go to the main menu
  if (lcd_click_now) MENU_GOTO(LCD_main_menu);

  if (lcd_turn) {
    speed_adjust += lcd_turn;
  }
  LCD_setCursor( 0, 0);

  // update the current status
  float offset[NUM_AXIES];
  get_end_plus_offset(offset);

  LCD_setCursor(0, 0);  LCD_print('X');  LCD_print_float(offset[0]);
  LCD_setCursor(9, 0);  LCD_print('Z');  LCD_print_float(offset[2]);
#if MACHINE_STYLE == POLARGRAPH && defined(USE_LIMIT_SWITCH)
  LCD_setCursor(8, 0);  LCD_print(( digitalRead(LIMIT_SWITCH_PIN_LEFT) == LOW ) ? '*' : ' ');
#endif

  LCD_setCursor(0, 1);  LCD_print('Y');  LCD_print_float(offset[1]);
  LCD_setCursor(9, 1);  LCD_print('F');  LCD_print_long(speed_adjust);  LCD_print('%');
#if MACHINE_STYLE == POLARGRAPH && defined(USE_LIMIT_SWITCH)
  LCD_setCursor(8, 1);  LCD_print(( digitalRead(LIMIT_SWITCH_PIN_RIGHT) == LOW ) ? '*' : ' ');
#endif

  //LCD_setCursor( 1, 15);
  //if (sd_printing_now == true && sd_printing_paused==false) {
  //if (sd_printing_now == true) {
    //LCD_print_float(sd_percent_complete);
    //LCD_print('%');
  //} else {
    //LCD_print("          ");
  //}
  MENU_END
#endif  // HAS_LCD
}


// initialize the Smart controller LCD panel
void LCD_setup() {
#ifdef HAS_LCD

#ifdef LCD_IS_SMART
  lcd.begin(LCD_WIDTH, LCD_HEIGHT);
#endif
#ifdef LCD_IS_128X64
  u8g.begin();
  u8g.disableCursor();
  u8g.setFont(u8g_font_6x9);
  //  u8g.setFont(u8g_font_helvR08);
#endif

  pinMode(BEEPER, OUTPUT);
  digitalWrite(BEEPER, LOW);

  pinMode(BTN_EN1, INPUT);
  pinMode(BTN_EN2, INPUT);
  pinMode(BTN_ENC, INPUT);
  digitalWrite(BTN_EN1, HIGH);
  digitalWrite(BTN_EN2, HIGH);
  digitalWrite(BTN_ENC, HIGH);
  current_menu = LCD_status_menu;
  menu_position_sum = 1;  /* 20160313-NM-Added so the clicking without any movement will display a menu */

  LCD_drawSplash();
  
  LCD_read();
  lcd_turn=0;
  LCD_clear();
#endif  // HAS_LCD
}


#ifdef HAS_LCD
void LCD_drawSplash() {
  // splash screen
  char message[LCD_WIDTH];
  char mhv[12];
  itoa(MACHINE_HARDWARE_VERSION, mhv, 10);
  char *ptr = message;
  strcpy(ptr, MACHINE_STYLE_NAME );  ptr += strlen(MACHINE_STYLE_NAME);
  strcpy(ptr, " v" );                ptr += strlen(" v");
  strcpy(ptr, mhv );                 ptr += strlen(mhv);

  int x = (LCD_WIDTH - strlen(message)) / 2;
  int y = LCD_HEIGHT - 2;
  int x2 = (LCD_WIDTH - strlen("marginallyclever.com")) / 2;
  int y2 = y + 1;

#ifdef LCD_IS_128X64
  u8g.firstPage();
  do {
#endif
    LCD_clear();
#ifdef LCD_IS_128X64
    u8g.drawBitmapP((LCD_PIXEL_WIDTH - logoImageWidth) / 2, 0, logoImageWidth / 8, logoImageHeight, logoImage);
#endif
    LCD_setCursor(x, y);
    LCD_print(message);
    LCD_setCursor(x2, y2);
    LCD_print("marginallyclever.com");
#ifdef LCD_IS_128X64
  } while (u8g.nextPage());
#endif
  LCD_refresh_display();
  delay(2500);
}
#endif  // HAS_LCD
