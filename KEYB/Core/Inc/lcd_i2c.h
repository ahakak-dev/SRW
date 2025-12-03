// lcd_i2c.h
#ifndef LCD_I2C_H
#define LCD_I2C_H

#include "stm32f1xx_hal.h"

// Адрес I2C для большинства дисплеев PCF8574
#define LCD_I2C_ADDR 0x27

// Команды LCD
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// Флаги для режима ввода
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// Флаги управления дисплеем
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// Флаги сдвига
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// Флаги настройки функции
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

// Биты PCF8574
#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE_BIT 0x04
#define LCD_READ_WRITE_BIT 0x02
#define LCD_REGISTER_SELECT_BIT 0x01

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t addr;
    uint8_t displaycontrol;
    uint8_t displaymode;
    uint8_t backlight;
} LCD_I2C_HandleTypeDef;

// Основные функции
void LCD_I2C_Init(LCD_I2C_HandleTypeDef *hlcd, I2C_HandleTypeDef *hi2c, uint8_t addr);
void LCD_I2C_Clear(LCD_I2C_HandleTypeDef *hlcd);
void LCD_I2C_Home(LCD_I2C_HandleTypeDef *hlcd);
void LCD_I2C_SetCursor(LCD_I2C_HandleTypeDef *hlcd, uint8_t col, uint8_t row);
void LCD_I2C_Print(LCD_I2C_HandleTypeDef *hlcd, const char *str);
void LCD_I2C_PrintChar(LCD_I2C_HandleTypeDef *hlcd, char ch);
void LCD_I2C_Backlight(LCD_I2C_HandleTypeDef *hlcd, uint8_t state);

#endif

// #include "lcd_i2c.c"
