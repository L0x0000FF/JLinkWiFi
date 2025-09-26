#include <iostream>
#include <wiringPi.h>
#include "oled.h"
#include <unistd.h>
#include <signal.h>
#include <string.h>

OLED *oled = nullptr;

void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    if (oled != nullptr) {
        oled->clear();
        oled->sleep();
        delete oled;
    }
    exit(signum);
}

// 测试位图数据（16x16笑脸）
const uint8_t smiley[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

int main() {
    signal(SIGINT, signalHandler);
    
    std::cout << "Initializing wiringPi..." << std::endl;
    if (wiringPiSetup() == -1) {
        std::cout << "wiringPi setup failed!" << std::endl;
        return 1;
    }
    
    // 创建OLED对象
    oled = new OLED(0, 0x3C);
    
    // 初始化OLED
    std::cout << "Initializing OLED..." << std::endl;
    if (!oled->init()) {
        std::cout << "OLED initialization failed!" << std::endl;
        delete oled;
        return 1;
    }
    
    std::cout << "OLED initialized successfully!" << std::endl;
    
    // 测试1: 基础文本显示（使用GRAM）
    std::cout << "Test 1: Basic text with GRAM..." << std::endl;
    oled->clear();
    oled->showString(0, 0, "GRAM Test", 16);
    oled->showString(0, 16, "Efficient!", 16);
    oled->refresh();  // 一次性刷新
    delay(2000);
    
    // 测试2: 图形绘制
    std::cout << "Test 2: Graphics drawing..." << std::endl;
    oled->clear();
    
    // 画各种图形
    oled->drawLine(0, 0, 127, 63, WHITE);
    oled->drawRect(10, 10, 50, 30, WHITE);
    oled->fillRect(70, 10, 110, 30, WHITE);
    oled->drawCircle(64, 32, 15, WHITE);
    
    // 显示说明文本
    oled->showString(5, 40, "Lines & Shapes", 12);
    oled->refresh();
    delay(3000);
    
    // 测试3: 动画效果（使用局部刷新）
    std::cout << "Test 3: Animation with partial refresh..." << std::endl;
    for (int i = 0; i < 5; i++) {
        oled->clear();
        
        // 移动的方块
        oled->fillRect(10 + i*20, 10, 30 + i*20, 30, WHITE);
        oled->showString(5, 40, "Animation Test", 12);
        oled->refresh();
        delay(500);
    }
    
    // 测试4: 像素级操作
    std::cout << "Test 4: Pixel-level operations..." << std::endl;
    oled->clear();
    
    // 创建图案
    for (int y = 0; y < 64; y += 4) {
        for (int x = 0; x < 128; x += 4) {
            if ((x/4 + y/4) % 2 == 0) {
                oled->drawPixel(x, y, WHITE);
                oled->drawPixel(x+1, y, WHITE);
                oled->drawPixel(x, y+1, WHITE);
                oled->drawPixel(x+1, y+1, WHITE);
            }
        }
    }
    oled->showString(20, 25, "Pixel Art", 16);
    oled->refresh();
    delay(3000);
    
    // 测试5: 性能对比
    std::cout << "Test 5: Performance comparison..." << std::endl;
    oled->clear();
    
    // 传统方式（逐个字符）
    unsigned int start = millis();
    oled->showString(0, 0, "Old Method:", 12);
    for (int i = 0; i < 10; i++) {
        oled->showChar(0, 16 + i*8, 'A' + i, 12);
    }
    unsigned int end = millis();
    oled->showString(70, 40, "Time:", 12);
    char time_str[10];
    sprintf(time_str, "%ums", end - start);  // 改为%u
    oled->showString(70, 52, time_str, 12);
    oled->refresh();
    delay(3000);
    
    // 最终显示
    std::cout << "All tests completed!" << std::endl;
    oled->clear();
    oled->showString(5, 10, "GRAM Test Complete!", 12);
    oled->showString(5, 25, "High Efficiency", 12);
    oled->showString(5, 40, "Graphics Ready", 12);
    
    // 画个边框
    oled->drawRect(0, 0, 127, 63, WHITE);
    oled->refresh();
    
    std::cout << "All tests completed. Press Ctrl+C to exit." << std::endl;
    
    // 保持程序运行
    while (true) {
        delay(1000);
    }
    
    return 0;
}
