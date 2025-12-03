
#include "lcd_i2c.h"
#include "main.h"

static void LCD_I2C_Write(LCD_I2C_HandleTypeDef *hlcd, uint8_t data, uint8_t mode);
static void LCD_I2C_SendCommand(LCD_I2C_HandleTypeDef *hlcd, uint8_t cmd);
static void LCD_I2C_SendData(LCD_I2C_HandleTypeDef *hlcd, uint8_t data);
static void LCD_I2C_Write4Bits(LCD_I2C_HandleTypeDef *hlcd, uint8_t value);
static void LCD_I2C_PulseEnable(LCD_I2C_HandleTypeDef *hlcd, uint8_t value);

void LCD_I2C_Init(LCD_I2C_HandleTypeDef *hlcd, I2C_HandleTypeDef *hi2c, uint8_t addr) {
    hlcd->hi2c = hi2c;
    hlcd->addr = addr;
    hlcd->displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    hlcd->displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    hlcd->backlight = LCD_BACKLIGHT;

    HAL_Delay(50);

    // Инициализация в 4-битном режиме
    LCD_I2C_Write4Bits(hlcd, 0x03 << 4);
    HAL_Delay(5);
    LCD_I2C_Write4Bits(hlcd, 0x03 << 4);
    HAL_Delay(1);
    LCD_I2C_Write4Bits(hlcd, 0x03 << 4);
    HAL_Delay(1);
    LCD_I2C_Write4Bits(hlcd, 0x02 << 4);
    HAL_Delay(1);

    // Установка режима работы
    LCD_I2C_SendCommand(hlcd, LCD_FUNCTIONSET | LCD_2LINE | LCD_5x8DOTS | LCD_4BITMODE);
    LCD_I2C_SendCommand(hlcd, LCD_DISPLAYCONTROL | hlcd->displaycontrol);
    LCD_I2C_Clear(hlcd);
    LCD_I2C_SendCommand(hlcd, LCD_ENTRYMODESET | hlcd->displaymode);

    HAL_Delay(100);
}

void LCD_I2C_Clear(LCD_I2C_HandleTypeDef *hlcd) {
    LCD_I2C_SendCommand(hlcd, LCD_CLEARDISPLAY);
    HAL_Delay(2);
}

void LCD_I2C_Home(LCD_I2C_HandleTypeDef *hlcd) {
    LCD_I2C_SendCommand(hlcd, LCD_RETURNHOME);
    HAL_Delay(2);
}

void LCD_I2C_SetCursor(LCD_I2C_HandleTypeDef *hlcd, uint8_t col, uint8_t row) {
    uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};
    if (row > 1) row = 1;
    LCD_I2C_SendCommand(hlcd, LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

void LCD_I2C_Print(LCD_I2C_HandleTypeDef *hlcd, const char *str) {
    while (*str) {
        LCD_I2C_PrintChar(hlcd, *str++);
    }
}

void LCD_I2C_PrintChar(LCD_I2C_HandleTypeDef *hlcd, char ch) {
    LCD_I2C_SendData(hlcd, ch);
}

void LCD_I2C_Backlight(LCD_I2C_HandleTypeDef *hlcd, uint8_t state) {
    hlcd->backlight = state ? LCD_BACKLIGHT : 0;
    LCD_I2C_Write(hlcd, 0, 0);
}

// Вспомогательные функции
static void LCD_I2C_Write(LCD_I2C_HandleTypeDef *hlcd, uint8_t data, uint8_t mode) {
    uint8_t high = mode | (data & 0xF0) | hlcd->backlight;
    uint8_t low = mode | ((data << 4) & 0xF0) | hlcd->backlight;

    LCD_I2C_Write4Bits(hlcd, high);
    LCD_I2C_PulseEnable(hlcd, high);
    LCD_I2C_Write4Bits(hlcd, low);
    LCD_I2C_PulseEnable(hlcd, low);
}

static void LCD_I2C_SendCommand(LCD_I2C_HandleTypeDef *hlcd, uint8_t cmd) {
    LCD_I2C_Write(hlcd, cmd, 0);
}

static void LCD_I2C_SendData(LCD_I2C_HandleTypeDef *hlcd, uint8_t data) {
    LCD_I2C_Write(hlcd, data, LCD_REGISTER_SELECT_BIT);
}

static void LCD_I2C_Write4Bits(LCD_I2C_HandleTypeDef *hlcd, uint8_t value) {
    uint8_t data = value | hlcd->backlight;
    HAL_I2C_Master_Transmit(hlcd->hi2c, hlcd->addr << 1, &data, 1, HAL_MAX_DELAY);
}

static void LCD_I2C_PulseEnable(LCD_I2C_HandleTypeDef *hlcd, uint8_t value) {
    LCD_I2C_Write4Bits(hlcd, value | LCD_ENABLE_BIT);
    HAL_Delay(1);
    LCD_I2C_Write4Bits(hlcd, value & ~LCD_ENABLE_BIT);
    HAL_Delay(1);
}
