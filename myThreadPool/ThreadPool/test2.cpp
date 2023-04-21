#include "ThreadPool.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>

using namespace std;
using namespace chrono;

static const char base16[] = "0123456789abcdef";

/*
二进制转字符串
一个字节8位，拆分为两个4字节（最大值为16）
拆分后字节映射到0123456789abcdef
*/
void Base16Encode(const unsigned char* data, int size, unsigned char* out) {
    for (int i = 0; i != size; ++i) {
        unsigned char d = data[i];
        unsigned char high = base16[d >> 4];
        unsigned char low = base16[d & 0x0f];
        out[i * 2] = high;
        out[i + 1] = low;
    }
}

void Base16EncodeThread(const vector<unsigned char>& data, vector<unsigned char> &out) {
    int size = data.size();
    int th_count = thread::hardware_concurrency();
    ThreadPool pool(16 ,th_count);

    int slice_count = size / th_count;
    if (size < th_count) {
        th_count = 1;
        slice_count = size;
    }
 
    for (int i = 0; i < th_count; i++) {
        int offset = i * slice_count;
        int count = slice_count;
        if (th_count > 1 && i == th_count - 1) {
            count = slice_count + size % th_count;
        }
        pool.commit(Base16Encode, data.data() + offset, count, out.data());
    }
}

int main() {
    // 初始化测试数据
    vector<unsigned char> in_data;
    in_data.resize(1024 * 1024 * 500); // 500M
    for (int i = 0; i != in_data.size(); ++i) {
        in_data[i] = i % 256;
    }
    vector<unsigned char> out_data;
    out_data.resize(in_data.size() * 2);

    // 测试单线程base16编码效率
    auto start = steady_clock::now();
    Base16Encode(in_data.data(), in_data.size(), out_data.data());
    auto end = steady_clock::now();

    auto duration = duration_cast<milliseconds>(end - start);
    cout  << in_data.size() << "字节数据--单线程编码--花费" << duration.count() << "毫秒" << endl;
    
    // 测试多线程base16编码效率
    start = steady_clock::now();
    Base16EncodeThread(in_data, out_data);
    end = steady_clock::now();

    duration = duration_cast<milliseconds>(end - start);
    cout  << in_data.size() << "字节数据--多线程编码--花费" << duration.count() << "毫秒" << endl;

    return 0;
}