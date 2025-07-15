#include "ggwave.h"
#include <portaudio.h>
#include <iostream>
#include <cstring>

// GGWave实例句柄
static ggwave_Instance ggwaveInstance = -1;

// 音频流回调函数
static int audioCallback(const void* inputBuffer, void* outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void* userData) {
    (void)outputBuffer; // 未使用输出
    (void)timeInfo;     // 未使用时间信息
    (void)statusFlags;  // 未使用状态标志
    (void)userData;     // 未使用用户数据
    // std::cout << "11111"  << std::endl;
    // 检查输入数据是否有数据
    if (inputBuffer == nullptr || ggwaveInstance == -1) {
        std::cout << "未检测到麦克风输入数据\n";
        return paContinue;
    }

    // 调用ggwave解码音频数据
    // 注意：根据实际样本格式调整此处的处理
    int decodedLength = ggwave_decode(ggwaveInstance, inputBuffer, framesPerBuffer * sizeof(int16_t), nullptr);

    // 添加调试输出
    if (decodedLength > 0) {
        std::cout << "\n成功解码! 长度: " << decodedLength << "字节\n";
    } else if (decodedLength == 0) {
        // 仅在需要详细调试时启用此行
        // std::cout << "未检测到有效信号\n";
    } else {
        std::cout << "解码错误! 错误码: " << decodedLength << "\n";
    }

    return paContinue;
}

int main() {
    std::cout << "Magical Conch 声音监听程序\n";

    // 初始化PortAudio
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio初始化失败: " << Pa_GetErrorText(err) << std::endl;
        return 1;
    }

    try {
        // 获取GGWave默认参数
        ggwave_Parameters params = ggwave_getDefaultParameters();
        params.operatingMode = GGWAVE_OPERATING_MODE_RX; // 设置为接收模式
        // 显式设置采样率（如果发送端使用不同值）
        // params.sampleRate = 44100;
        // params.sampleRateInp = 44100;

        // 初始化GGWave实例
        ggwaveInstance = ggwave_init(params);
        if (ggwaveInstance == -1) {
            throw std::runtime_error("GGWave初始化失败");
        }

        // 添加协议启用代码 - 启用可听范围正常协议
        ggwave_rxToggleProtocol(GGWAVE_PROTOCOL_AUDIBLE_NORMAL, 1);
        // 如果使用超声波协议，添加以下行
        // ggwave_rxToggleProtocol(GGWAVE_PROTOCOL_ULTRASOUND_NORMAL, 1);
        // 获取默认输入设备
        PaDeviceIndex deviceIndex = Pa_GetDefaultInputDevice();
        if (deviceIndex == paNoDevice) {
            throw std::runtime_error("找不到默认麦克风设备");
        }

        // 配置音频流参数
        PaStreamParameters inputParams;
        inputParams.device = deviceIndex;
        inputParams.channelCount = 1; // 单声道
        inputParams.sampleFormat = paInt16; // 16位整数格式
        inputParams.suggestedLatency = Pa_GetDeviceInfo(deviceIndex)->defaultLowInputLatency;
        inputParams.hostApiSpecificStreamInfo = nullptr;

        // 打开音频流
        PaStream* stream;
        err = Pa_OpenStream(&stream, &inputParams, nullptr, params.sampleRateInp, 256,
                           paClipOff, audioCallback, nullptr);
        if (err != paNoError) {
            throw std::runtime_error("无法打开音频流: " + std::string(Pa_GetErrorText(err)));
        }

        // 启动音频流
        err = Pa_StartStream(stream);
        if (err != paNoError) {
            Pa_CloseStream(stream);
            throw std::runtime_error("无法启动音频流: " + std::string(Pa_GetErrorText(err)));
        }

        std::cout << "正在监听麦克风输入... (按Enter键停止)\n";
        std::cin.get(); // 等待用户输入

        // 停止并关闭音频流
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
    }
    catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
    }

    // 清理资源
    if (ggwaveInstance != -1) {
        ggwave_free(ggwaveInstance);
    }
    Pa_Terminate();

    return 0;
}