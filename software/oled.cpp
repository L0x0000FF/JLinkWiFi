#include "oled.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

// 幂函数保持不变
uint32_t oled_pow(uint8_t m, uint8_t n) {
    uint32_t result = 1;
    while (n--) result *= m;
    return result;
}

OLED::OLED(uint8_t i2c_bus, uint8_t addr) {
    this->addr = addr;
    char device[20];
    snprintf(device, sizeof(device), "/dev/i2c-%d", i2c_bus);
    this->i2c_fd = wiringPiI2CSetupInterface(device, addr);
    
    // 初始化GRAM为0
    clearGRAM();
    
    if (this->i2c_fd < 0) {
        printf("Error: Failed to initialize I2C on bus %d, address 0x%02X\n", i2c_bus, addr);
    } else {
        printf("I2C initialized successfully: fd=%d, address=0x%02X\n", this->i2c_fd, addr);
    }
}

bool OLED::init() {
    if (this->i2c_fd < 0) {
        printf("I2C not initialized!\n");
        return false;
    }
    
    // 初始化序列保持不变
    this->writeCommand(0xAE);//--display off
    this->writeCommand(0x00);//---set low column address
    this->writeCommand(0x10);//---set high column address
    this->writeCommand(0x40);//--set start line address
    this->writeCommand(0xB0);//--set page address
    this->writeCommand(0x81); // contract control
    this->writeCommand(0xFF);//--128
    this->writeCommand(0xA1);//set segment remap
    this->writeCommand(0xA6);//--normal / reverse
    this->writeCommand(0xA8);//--set multiplex ratio(1 to 64)
    this->writeCommand(0x3F);//--1/32 duty
    this->writeCommand(0xC8);//Com scan direction
    this->writeCommand(0xD3);//-set display offset
    this->writeCommand(0x00);//
    this->writeCommand(0xD5);//set osc division
    this->writeCommand(0x80);//
    this->writeCommand(0xD8);//set area color mode off
    this->writeCommand(0x05);//
    this->writeCommand(0xD9);//Set Pre-Charge Period
    this->writeCommand(0xF1);//
    this->writeCommand(0xDA);//set com pin configuartion
    this->writeCommand(0x12);//
    this->writeCommand(0xDB);//set Vcomh
    this->writeCommand(0x30);//
    this->writeCommand(0x8D);//set charge pump enable
    this->writeCommand(0x14);//
    this->writeCommand(0xAF);//--turn on oled panel
    
    this->clear();
    return true;
}

void OLED::writeCommand(unsigned char command) {
    wiringPiI2CWriteReg8(this->i2c_fd, 0x00, command);
    usleep(100);
}

void OLED::writeData(unsigned char data) {
    wiringPiI2CWriteReg8(this->i2c_fd, 0x40, data);
    usleep(100);
}

// GRAM缓冲区操作
void OLED::clearGRAM() {
    memset(gram, 0, sizeof(gram));
}

void OLED::refresh() {
    // 刷新整个GRAM到OLED
    for (int page = 0; page < OLED_PAGES; page++) {
        setPos(0, page);
        
        // 发送该页的所有数据
        for (int col = 0; col < OLED_MAX_COLUMN; col++) {
            writeData(gram[page][col]);
        }
    }
}

void OLED::refreshArea(uint8_t page, uint8_t start_col, uint8_t end_col) {
    if (page >= OLED_PAGES || start_col >= OLED_MAX_COLUMN || end_col >= OLED_MAX_COLUMN) 
        return;
        
    setPos(start_col, page);
    
    for (int col = start_col; col <= end_col; col++) {
        writeData(gram[page][col]);
    }
}

// 像素级绘图函数
void OLED::drawPixel(uint8_t x, uint8_t y, uint8_t color) {
    if (x >= OLED_MAX_COLUMN || y >= OLED_MAX_ROW) return;
    
    uint8_t page = y / 8;
    uint8_t bit = y % 8;
    
    switch(color) {
        case WHITE:
            gram[page][x] |= (1 << bit);
            break;
        case BLACK:
            gram[page][x] &= ~(1 << bit);
            break;
        case INVERSE:
            gram[page][x] ^= (1 << bit);
            break;
    }
}

void OLED::drawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color) {
    int dx = abs((int)x2 - (int)x1);
    int dy = abs((int)y2 - (int)y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    while(1) {
        drawPixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void OLED::drawRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color) {
    drawLine(x1, y1, x2, y1, color);
    drawLine(x2, y1, x2, y2, color);
    drawLine(x2, y2, x1, y2, color);
    drawLine(x1, y2, x1, y1, color);
}

void OLED::fillRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color) {
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            drawPixel(x, y, color);
        }
    }
}

void OLED::drawCircle(uint8_t x0, uint8_t y0, uint8_t r, uint8_t color) {
    int x = r;
    int y = 0;
    int err = 0;
    
    while (x >= y) {
        drawPixel(x0 + x, y0 + y, color);
        drawPixel(x0 + y, y0 + x, color);
        drawPixel(x0 - y, y0 + x, color);
        drawPixel(x0 - x, y0 + y, color);
        drawPixel(x0 - x, y0 - y, color);
        drawPixel(x0 - y, y0 - x, color);
        drawPixel(x0 + y, y0 - x, color);
        drawPixel(x0 + x, y0 - y, color);
        
        if (err <= 0) {
            y += 1;
            err += 2*y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2*x + 1;
        }
    }
}

// 基于GRAM的字符显示
void OLED::showChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t Char_Size) {
    unsigned char c = chr - ' ';
    
    if (x > OLED_MAX_COLUMN - 1) {
        x = 0;
        y = y + 2;
    }
    
    if (Char_Size == 16) {
        // 16x16字符
        for (int i = 0; i < 8; i++) {
            uint8_t data = F8X16[c * 16 + i];
            for (int bit = 0; bit < 8; bit++) {
                drawPixel(x + i, y + bit, (data & (1 << bit)) ? WHITE : BLACK);
            }
        }
        for (int i = 0; i < 8; i++) {
            uint8_t data = F8X16[c * 16 + i + 8];
            for (int bit = 0; bit < 8; bit++) {
                drawPixel(x + i, y + bit + 8, (data & (1 << bit)) ? WHITE : BLACK);
            }
        }
    } else {
        // 6x8字符
        for (int i = 0; i < 6; i++) {
            uint8_t data = F6x8[c][i];
            for (int bit = 0; bit < 8; bit++) {
                drawPixel(x + i, y + bit, (data & (1 << bit)) ? WHITE : BLACK);
            }
        }
    }
}

void OLED::showNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size) {
    uint8_t temp;
    uint8_t enshow = 0;
    for (int t = 0; t < len; t++) {
        temp = (num / oled_pow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1)) {
            if (temp == 0) {
                this->showChar(x + (size / 2) * t, y, ' ', size);
                continue;
            } else enshow = 1;
        }
        this->showChar(x + (size / 2) * t, y, temp + '0', size);
    }
}

void OLED::showFloat(uint8_t x, uint8_t y, float num, uint8_t fontSize, const char * format) {
    char str[20];
    sprintf(str, format, num);
    this->showString(x, y, str, fontSize);
}

void OLED::showString(uint8_t x, uint8_t y, const char *str, uint8_t fontSize) {
    unsigned char j = 0;
    while (str[j] != '\0') {
        this->showChar(x, y, str[j], fontSize);
        x += (fontSize == 16) ? 8 : 6;
        if (x > 120) {
            x = 0;
            y += 2;
        }
        j++;
    }
}

// 位图显示（使用GRAM）
void OLED::drawBitmap(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const uint8_t *bitmap) {
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            if (bitmap[j * width + i]) {
                drawPixel(x + i, y + j, WHITE);
            }
        }
    }
}

// 保持原有函数兼容性
void OLED::wakeUp(void) {
    this->writeCommand(0X8D);
    this->writeCommand(0X14);
    this->writeCommand(0XAF);
}

void OLED::sleep(void) {
    this->writeCommand(0X8D);
    this->writeCommand(0X10);
    this->writeCommand(0XAE);
}

void OLED::clear(void) {
    clearGRAM();
    refresh();
}

void OLED::setPos(unsigned char x, unsigned char y) {
    this->writeCommand(0xb0 + y);
    this->writeCommand(((x & 0xf0) >> 4) | 0x10);
    this->writeCommand((x & 0x0f));
}

// 其他保持兼容的函数
void OLED::fill(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t dot) {
    fillRect(x1, y1, x2, y2, dot ? WHITE : BLACK);
}

void OLED::showChinese(uint8_t x, uint8_t y, uint8_t no) {
    // 中文字符显示
}

void OLED::drawBMP(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1, unsigned char *BMP) {
    // 位图显示
}
