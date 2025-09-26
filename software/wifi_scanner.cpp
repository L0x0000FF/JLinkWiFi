#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <wiringPi.h>
#include <unistd.h>
#include <signal.h>
#include <algorithm>
#include "oled.h"

// NetworkManager头文件
#include <NetworkManager.h>
#include <nm-dbus-interface.h>
#include <glib.h>

OLED *oled = nullptr;

// WiFi网络信息结构体
struct WiFiNetwork {
    std::string ssid;
    int signal_strength;  // 信号强度 (0-100)
    bool secured;         // 是否加密
};

// 信号处理函数
void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    if (oled != nullptr) {
        oled->clear();
        oled->sleep();
        delete oled;
    }
    // 不需要调用nm_client_stop()，直接退出
    exit(signum);
}

// 获取WiFi网络列表
std::vector<WiFiNetwork> scanWiFiNetworks() {
    std::vector<WiFiNetwork> networks;
    
    GError *error = nullptr;
    NMClient *client = nm_client_new(nullptr, &error);
    
    if (!client) {
        std::cerr << "Failed to create NMClient: " << error->message << std::endl;
        g_error_free(error);
        return networks;
    }
    
    // 获取所有设备
    const GPtrArray *devices = nm_client_get_devices(client);
    if (!devices) {
        std::cerr << "No devices found" << std::endl;
        g_object_unref(client);
        return networks;
    }
    
    // 查找WiFi设备
    NMDeviceWifi *wifi_device = nullptr;
    for (guint i = 0; i < devices->len; i++) {
        NMDevice *device = (NMDevice *)devices->pdata[i];
        if (NM_IS_DEVICE_WIFI(device)) {
            wifi_device = NM_DEVICE_WIFI(device);
            break;
        }
    }
    
    if (!wifi_device) {
        std::cerr << "No WiFi device found" << std::endl;
        g_object_unref(client);
        return networks;
    }
    
    // 请求扫描 - 使用正确的参数数量
    nm_device_wifi_request_scan(wifi_device, nullptr, &error);
    std::cout << "Scanning for WiFi networks..." << std::endl;
    
    // 等待扫描完成（简单延时）
    sleep(5);
    
    // 获取扫描结果
    const GPtrArray *aps = nm_device_wifi_get_access_points(wifi_device);
    if (!aps) {
        std::cerr << "No access points found" << std::endl;
        g_object_unref(client);
        return networks;
    }
    
    std::cout << "Found " << aps->len << " access points" << std::endl;
    
    // 提取网络信息
    for (guint i = 0; i < aps->len; i++) {
        NMAccessPoint *ap = (NMAccessPoint *)aps->pdata[i];
        
        WiFiNetwork network;
        
        // 获取SSID
        GBytes *ssid_bytes = nm_access_point_get_ssid(ap);
        if (ssid_bytes) {
            gsize len;
            const char *ssid_data = (const char *)g_bytes_get_data(ssid_bytes, &len);
            network.ssid = std::string(ssid_data, len);
            // 过滤不可打印字符
            for (char &c : network.ssid) {
                if (c < 32 || c > 126) c = '?';
            }
        } else {
            network.ssid = "Hidden Network";
        }
        
        // 获取信号强度
        network.signal_strength = nm_access_point_get_strength(ap);
        
        // 检查是否加密
        NM80211ApFlags flags = nm_access_point_get_flags(ap);
        NM80211ApSecurityFlags wpa_flags = nm_access_point_get_wpa_flags(ap);
        NM80211ApSecurityFlags rsn_flags = nm_access_point_get_rsn_flags(ap);
        
        network.secured = (flags & NM_802_11_AP_FLAGS_PRIVACY) || 
                         (wpa_flags != NM_802_11_AP_SEC_NONE) || 
                         (rsn_flags != NM_802_11_AP_SEC_NONE);
        
        networks.push_back(network);
    }
    
    // 按信号强度排序
    std::sort(networks.begin(), networks.end(), 
              [](const WiFiNetwork &a, const WiFiNetwork &b) {
                  return a.signal_strength > b.signal_strength;
              });
    
    g_object_unref(client);
    return networks;
}

// 在OLED上显示WiFi网络列表
void displayWiFiNetworks(const std::vector<WiFiNetwork> &networks) {
    if (!oled) return;
    
    oled->clear();
    
    // 显示标题
    oled->showString(0, 0, "WiFi Networks", 12);
    
    if (networks.empty()) {
        oled->showString(0, 16, "No networks found", 12);
        oled->refresh();
        return;
    }
    
    // 显示网络列表（最多显示6个）
    int max_display = std::min(6, (int)networks.size());
    
    for (int i = 0; i < max_display; i++) {
        const WiFiNetwork &network = networks[i];
        int y_pos = 16 + i * 8;  // 每行8像素
        
        // 构建显示字符串（SSID和信号强度）
        std::stringstream ss;
        ss << (i + 1) << ". ";
        
        // 截断SSID以适应屏幕
        std::string display_ssid = network.ssid;
        if (display_ssid.length() > 10) {
            display_ssid = display_ssid.substr(0, 10) + "...";
        }
        ss << display_ssid;
        
        // 显示信号强度条
        std::string signal_bar = " [";
        int bars = network.signal_strength / 20;  // 5格信号条
        for (int j = 0; j < 5; j++) {
            signal_bar += (j < bars) ? "=" : " ";
        }
        signal_bar += "]";
        
        // 组合显示字符串
        std::string display_str = ss.str();
        if (display_str.length() + signal_bar.length() <= 16) {
            display_str += signal_bar;
        }
        
        oled->showString(0, y_pos, display_str.c_str(), 12);
    }
    
    // 显示统计信息
    std::stringstream info_ss;
    info_ss << "Total: " << networks.size() << " networks";
    oled->showString(0, 64 - 8, info_ss.str().c_str(), 12);
    
    oled->refresh();
}

// 显示网络详情页面
void displayNetworkDetails(const WiFiNetwork &network, int index, int total) {
    if (!oled) return;
    
    oled->clear();
    
    // 显示网络序号
    std::stringstream header_ss;
    header_ss << "Network " << index << "/" << total;
    oled->showString(0, 0, header_ss.str().c_str(), 12);
    
    // 显示SSID
    oled->showString(0, 12, "SSID:", 12);
    std::string display_ssid = network.ssid;
    if (display_ssid.length() > 16) {
        // 如果SSID太长，分两行显示
        oled->showString(30, 12, display_ssid.substr(0, 16).c_str(), 12);
        if (display_ssid.length() > 16) {
            oled->showString(0, 22, display_ssid.substr(16, 16).c_str(), 12);
        }
    } else {
        oled->showString(30, 12, display_ssid.c_str(), 12);
    }
    
    // 显示信号强度
    oled->showString(0, 32, "Signal:", 12);
    std::stringstream signal_ss;
    signal_ss << network.signal_strength << "%";
    oled->showString(40, 32, signal_ss.str().c_str(), 12);
    
    // 绘制信号强度条
    int bar_width = (network.signal_strength * 100) / 100;  // 百分比转像素
    for (int x = 0; x < 128; x++) {
        for (int y = 42; y < 46; y++) {
            if (x < bar_width) {
                oled->drawPixel(x, y, WHITE);
            }
        }
    }
    
    // 显示安全状态
    oled->showString(0, 50, "Security:", 12);
    oled->showString(55, 50, network.secured ? "Encrypted" : "Open", 12);
    
    oled->refresh();
}

int main() {
    signal(SIGINT, signalHandler);
    
    std::cout << "Initializing WiFi Scanner for NanoPi Duo2..." << std::endl;
    
    // 初始化wiringPi
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
    
    // 显示启动信息
    oled->clear();
    oled->showString(10, 10, "WiFi Scanner", 16);
    oled->showString(5, 30, "NanoPi Duo2", 12);
    oled->showString(15, 45, "Scanning...", 12);
    oled->refresh();
    delay(2000);
    
    while (true) {
        // 扫描WiFi网络
        std::vector<WiFiNetwork> networks;
        
        std::cout << "Scanning for WiFi networks..." << std::endl;
        
        // 尝试使用NetworkManager扫描
        networks = scanWiFiNetworks();
        
        // 如果NetworkManager扫描失败，使用fallback方法
        if (networks.empty()) {
            std::cout << "NetworkManager scan failed, trying fallback method..." << std::endl;
            networks = scanWiFiFallback();
        }
        
        std::cout << "Found " << networks.size() << " WiFi networks" << std::endl;
        
        if (networks.empty()) {
            // 没有找到网络
            oled->clear();
            oled->showString(10, 20, "No WiFi Networks", 12);
            oled->showString(15, 35, "Found!", 12);
            oled->refresh();
        } else {
            // 显示网络列表
            displayWiFiNetworks(networks);
            
            // 等待用户查看列表
            delay(5000);
            
            // 逐个显示网络详情
            for (size_t i = 0; i < networks.size(); i++) {
                displayNetworkDetails(networks[i], i + 1, networks.size());
                delay(3000);  // 每个网络显示3秒
                
                // 如果网络数量多，只显示前8个
                if (i >= 7) break;
            }
        }
        
        // 显示扫描完成信息
        oled->clear();
        oled->showString(5, 20, "Scan Complete!", 12);
        oled->showString(10, 35, "Rescan in 5s", 12);
        oled->refresh();
        
        // 等待5秒后重新扫描
        delay(5000);
    }
    
    return 0;
}
