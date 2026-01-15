#include "ggwave.h"

#include "ggwave-common.h"
#include "ggwave-common-sdl2.h"

#include <SDL.h>

#include <cstdio>
#include <string>
#include <fstream>
#include <vector>

#include <mutex>
#include <thread>
#include <iostream>

// 添加全局退出标志
std::atomic<bool> g_running(true);

// 信号处理函数
void signalHandler(int signum) {
    if (signum == SIGINT) {
        printf("\nReceived Ctrl+C, exiting...\n");
        g_running = false;
    }
}


int main(int argc, char** argv) {

    signal(SIGINT, signalHandler);
    printf("Usage: %s [-cN] [-pN] [-tN] [-lN] [-r] [-s filename] [-f filename]\n", argv[0]);
    printf("    -cN - select capture device N\n");
    printf("    -pN - select playback device N\n");
    printf("    -tN - transmission protocol\n");
    printf("    -lN - fixed payload length of size N, N in [1, %d]\n", GGWave::kMaxLengthFixed);
    printf("    -d  - use Direct Sequence Spread (DSS)\n");
    printf("    -v  - print generated tones on resend\n");
    printf("    -r  - receive only mode (disable transmission)\n");
    printf("    -s filename - save encoded waveform to file (for testing)\n");
    printf("    -f filename - load waveform from file and decode (for testing)\n");
    printf("\n");

    const auto argm          = parseCmdArguments(argc, argv);
    const int  captureId     = argm.count("c") == 0 ?  0 : std::stoi(argm.at("c"));
    const int  playbackId    = argm.count("p") == 0 ?  0 : std::stoi(argm.at("p"));
    const int  txProtocolId  = argm.count("t") == 0 ?  0 : std::stoi(argm.at("t"));
    const int  payloadLength = argm.count("l") == 0 ? -1 : std::stoi(argm.at("l"));
    const bool useDSS        = argm.count("d") >  0;  //强制启动DSS：直接序列扩频(DSS)技术
    const bool printTones    = argm.count("v") >  0;
    const bool receiveOnly   = argm.count("r") >  0;
    const bool saveToFile    = argm.count("s") >  0;
    const bool loadFromFile  = argm.count("f") >  0;
    const std::string saveFilename = saveToFile ? argm.at("s") : "";
    const std::string loadFilename = loadFromFile ? argm.at("f") : "";

    printf("Debug: saveToFile = %d, saveFilename = '%s'\n", saveToFile, saveFilename.c_str());
    printf("Debug: loadFromFile = %d, loadFilename = '%s'\n", loadFromFile, loadFilename.c_str());

    if (GGWave_init(playbackId, captureId, payloadLength, 0.0f, useDSS) == false) {
        fprintf(stderr, "Failed to initialize GGWave\n");
        return -1;
    }

    auto ggWave = GGWave_instance();
    
    printf("Debug: ggWave encode instance parameters:\n");
    printf("  - payloadLength: %d\n", ggWave->rxDurationFrames());
    printf("  - samplesPerFrame: %d\n", ggWave->samplesPerFrame());
    printf("  - sampleRateInp: %f\n", ggWave->sampleRateInp());
    printf("  - sampleRateOut: %f\n", ggWave->sampleRateOut());
    printf("  - soundMarkerThreshold: %f\n", 3.0f);
    fflush(stdout);

    if (loadFromFile) {
        printf("Loading waveform from file: %s\n", loadFilename.c_str());

        std::ifstream file(loadFilename, std::ios::binary);
        if (!file.is_open()) {
            fprintf(stderr, "Failed to open file: %s\n", loadFilename.c_str());
            return -4;
        }

        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> waveform(fileSize);
        file.read(waveform.data(), fileSize);
        file.close();

        printf("Loaded %zu bytes from file\n", fileSize);

        bool decodeResult = ggWave->decode(waveform.data(), fileSize);

        if (decodeResult && ggWave->rxDataLength() > 0) {
            const auto & rxData = ggWave->rxData();
            printf("Decoded payload (%d bytes): %.*s\n", ggWave->rxDataLength(), ggWave->rxDataLength(), rxData.data());
        } else {
            printf("Failed to decode waveform or no payload detected\n");
        }

        return 0;
    }

    std::shared_ptr<GGWave> ggWaveDecode = nullptr;

    printf("Available Tx protocols:\n");
    const auto & protocols = GGWave::Protocols::kDefault();
    for (int i = 0; i < (int) protocols.size(); ++i) {
        const auto & protocol = protocols[i];
        if (protocol.enabled == false) {
            continue;
        }
        printf("      %d - %s\n", i, protocol.name);
    }

    if (!receiveOnly) {
        if (txProtocolId < 0) {
            fprintf(stderr, "Unknown Tx protocol %d\n", txProtocolId);
            return -3;
        }

        printf("Selecting Tx protocol %d\n", txProtocolId);
    } else {
        printf("Receive only mode enabled\n");
    }

    std::mutex mutex;
    std::thread inputThread;

    if (!receiveOnly) {
        inputThread = std::thread([&]() {
            std::string inputOld = "";
            while (g_running) {
                std::string input;
                printf("Enter text: ");
                fflush(stdout);
                getline(std::cin, input);
                if (input.empty()) {
                    printf("Re-sending ...\n");
                    input = inputOld;

                    if (printTones) {
                        printf("Printing generated waveform tones (Hz):\n");
                        const auto & protocol = protocols[txProtocolId];
                        const auto tones = ggWave->txTones();
                        for (int i = 0; i < (int) tones.size(); ++i) {
                            if (tones[i] < 0) {
                                printf(" - end tx\n");
                                continue;
                            }
                            const auto freq_hz = (protocol.freqStart + tones[i])*ggWave->hzPerSample();
                            printf(" - tone %3d: %f\n", i, freq_hz);
                        }
                    }
                } else {
                    printf("Sending ...\n");
                    
                    printf("Debug: Original input bytes (hex): ");
                    for (size_t i = 0; i < input.size(); i++) {
                        printf("%02x ", (unsigned char)input[i]);
                    }
                    printf("\n");
                    
                    printf("Debug: Original input bytes (decimal): ");
                    for (size_t i = 0; i < input.size(); i++) {
                        printf("%d ", (unsigned char)input[i]);
                    }
                    printf("\n");
                    fflush(stdout);
                }
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    ggWave->init(input.size(), input.data(), GGWave::TxProtocolId(txProtocolId), 100);

                    if (saveToFile) {
                        printf("Step 0: Preparing to save waveform to file: %s\n", saveFilename.c_str());
                        fflush(stdout);
                        
                        if (saveFilename.empty()) {
                            fprintf(stderr, "✗ Error: saveFilename is empty!\n");
                            fflush(stderr);
                            continue;
                        }
                        
                        uint32_t waveformSize = ggWave->encodeSize_bytes();
                        printf("Step 1: encodeSize_bytes = %u\n", waveformSize);
                        fflush(stdout);
                        
                        if (waveformSize > 0) {
                            printf("Step 2: Calling encode()...\n");
                            fflush(stdout);
                            
                            uint32_t encodedSize = ggWave->encode();
                            printf("Step 3: encode() returned %u bytes\n", encodedSize);
                            fflush(stdout);
                            
                            const void * waveform = ggWave->txWaveform();
                            printf("Step 4: txWaveform() returned %p\n", waveform);
                            fflush(stdout);
                            
                            uint32_t actualSize = waveformSize;
                            printf("Step 5: actualSize = %u (using waveformSize from Step 1)\n", actualSize);
                            fflush(stdout);

                                    if (waveform && actualSize > 0) {
    // ================= [新增] 自动增益归一化 (Auto Normalization) =================
    // 这段代码会强制把音量拉满，彻底解决 t3 无法解码的问题
    
    // 1. 拷贝原始数据到可修改的 buffer
    std::vector<int16_t> normalizedWaveform(actualSize / 2);
    const int16_t* srcPtr = static_cast<const int16_t*>(waveform);
    std::memcpy(normalizedWaveform.data(), srcPtr, actualSize);

    // 2. 寻找当前最大峰值
    int16_t maxVal = 0;
    for (size_t i = 0; i < normalizedWaveform.size(); i++) {
        if (std::abs(normalizedWaveform[i]) > maxVal) {
            maxVal = std::abs(normalizedWaveform[i]);
        }
    }
    printf("Debug: Peak amplitude before normalization: %d\n", maxVal);

    // 3. 如果信号存在但太弱，就进行放大
    if (maxVal > 0 && maxVal < 20000) { // 20000 是一个保守的安全阈值
        float scale = 25000.0f / (float)maxVal; // 目标峰值 25000 (约 76% 音量)
        printf("Debug: Applying gain factor: %.2f\n", scale);
        
        for (size_t i = 0; i < normalizedWaveform.size(); i++) {
            normalizedWaveform[i] = static_cast<int16_t>(normalizedWaveform[i] * scale);
        }
    }

    // 4. 更新指针，让后面的写入操作使用放大后的数据
    const char* dataToWrite = reinterpret_cast<const char*>(normalizedWaveform.data());
    // ==========================================================================

    printf("Step 6: Opening file for writing...\n");
    fflush(stdout);
    
    std::ofstream file(saveFilename, std::ios::binary);
    if (file.is_open()) {
        printf("Step 7: Writing %u bytes to file...\n", actualSize);
        fflush(stdout);
        
        // 注意：这里改成 write(dataToWrite, ...)
        file.write(dataToWrite, actualSize); 
        file.close();
        printf("✓ Saved normalized waveform to file: %s (%u bytes)\n", saveFilename.c_str(), actualSize);
        fflush(stdout);
        
        // 打印新的 Hex 用于验证 (你会发现数值变大了，比如变成 30 00, A0 00 等)
        printf("Debug: First 20 bytes of NORMALIZED waveform (hex): ");
        for (int i = 0; i < 20 && i < actualSize; i++) {
            printf("%02x ", (unsigned char)dataToWrite[i]);
        }
        printf("\n");
        fflush(stdout);
                                    printf("Step 8: Opening file for reading...\n");
                                    fflush(stdout);
                                    
                                    std::ifstream readFile(saveFilename, std::ios::binary);
                                    if (readFile.is_open()) {
                                        std::vector<char> loadedWaveform(actualSize);
                                        readFile.read(loadedWaveform.data(), actualSize);
                                        readFile.close();
                                        printf("✓ Loaded waveform from file: %s (%zu bytes)\n", saveFilename.c_str(), loadedWaveform.size());
                                        fflush(stdout);

                                        printf("Step 9: Creating new GGWave instance for decoding...\n");
                                        fflush(stdout);
                                        
                                        if (!ggWaveDecode) {
                                            printf("Step 9.1: Configuring Rx protocol %d\n", txProtocolId);
                                            fflush(stdout);
                                            
                                            GGWave::Protocols::rx().only(GGWave::TxProtocolId(txProtocolId));
                                            
                                            printf("Step 9.2: Creating GGWave decode instance\n");
                                            fflush(stdout);
                                            
                                            GGWave::Parameters parameters = ggWave->getDefaultParameters();
                                            parameters.operatingMode = GGWAVE_OPERATING_MODE_RX;
                                            parameters.payloadLength = payloadLength;
                                            parameters.sampleRateInp = 44100.0f;
                                            parameters.sampleRateOut = 44100.0f;
                                            parameters.sampleRate = 44100.0f;
                                            parameters.samplesPerFrame = 1024;
                                            parameters.soundMarkerThreshold = 1.0f;
                                            parameters.sampleFormatInp = GGWave::SampleFormat::GGWAVE_SAMPLE_FORMAT_I16;
                                            parameters.sampleFormatOut = GGWave::SampleFormat::GGWAVE_SAMPLE_FORMAT_I16;
                                            ggWaveDecode = std::make_shared<GGWave>(parameters);
                                            
                                            printf("Step 9.3: Calling prepare() on decode instance\n");
                                            fflush(stdout);
                                            
                                            ggWaveDecode->prepare(parameters, true);
                                            
                                            printf("Step 9.5: GGWave decode instance created and prepared\n");
                                            printf("Step 9.6: payloadLength = %d\n", parameters.payloadLength);
                                            printf("Step 9.7: sampleFormatInp = %d (GGWAVE_SAMPLE_FORMAT_I16 = %d)\n", 
                                                   parameters.sampleFormatInp, GGWave::SampleFormat::GGWAVE_SAMPLE_FORMAT_I16);
                                            fflush(stdout);
                                        }
                                        
                                        printf("Step 10: Decoding waveform...\n");
                                        fflush(stdout);
                                        
                                        int samplesPerFrame = ggWaveDecode->samplesPerFrame();
                                        int sampleSize = ggWaveDecode->sampleSizeInp();
                                        int frameSize = samplesPerFrame * sampleSize;
                                        
                                        printf("Step 10.1: samplesPerFrame = %d, sampleSize = %d, frameSize = %d\n", 
                                               samplesPerFrame, sampleSize, frameSize);
                                        fflush(stdout);
                                        
                                        int totalFrames = actualSize / frameSize;
                                        printf("Step 10.2: Total frames to decode = %d\n", totalFrames);
                                        fflush(stdout);
                                        
                                        bool decodeResult = false;
                                        for (int i = 0; i < totalFrames; i++) {
                                            const char* frameData = loadedWaveform.data() + i * frameSize;
                                            decodeResult = ggWaveDecode->decode(frameData, frameSize);
                                            
                                            if (ggWaveDecode->rxDataLength() > 0) {
                                                printf("Step 10.3: Found data at frame %d\n", i);
                                                fflush(stdout);
                                                break;
                                            }
                                        }
                                        
                                        printf("Step 11: Final decode() returned %d\n", decodeResult);
                                        printf("Step 12: rxDataLength() = %d\n", ggWaveDecode->rxDataLength());
                                        fflush(stdout);

                                        if (decodeResult && ggWaveDecode->rxDataLength() > 0) {
                                            const auto & rxData = ggWaveDecode->rxData();
                                            printf("✓ Decoded payload (%d bytes): %.*s\n", ggWaveDecode->rxDataLength(), ggWaveDecode->rxDataLength(), rxData.data());
                                                                             
                                            printf("Debug: Decoded bytes (hex): ");
                                            for (int i = 0; i < ggWaveDecode->rxDataLength(); i++) {
                                                printf("%02x ", (unsigned char)rxData[i]);
                                            }
                                            printf("\n");
                                            
                                            printf("Debug: Decoded bytes (decimal): ");
                                            for (int i = 0; i < ggWaveDecode->rxDataLength(); i++) {
                                                printf("%d ", (unsigned char)rxData[i]);
                                            }
                                            printf("\n");
                                            fflush(stdout);
                                        } else {
                                            printf("✗ Failed to decode waveform or no payload detected\n");
                                            printf("   decodeResult = %d, rxDataLength = %d\n", decodeResult, ggWaveDecode->rxDataLength());
                                        }
                                        fflush(stdout);
                                    } else {
                                        fprintf(stderr, "✗ Failed to open file for reading: %s\n", saveFilename.c_str());
                                    }
                                } else {
                                    fprintf(stderr, "✗ Failed to save waveform to file: %s\n", saveFilename.c_str());
                                }
                            } else {
                                fprintf(stderr, "✗ Waveform is null or actualSize is 0\n");
                            }
                        } else {
                            fprintf(stderr, "✗ waveformSize is 0\n");
                        }
                    }
                }
                inputOld = input;
            }
        });
    }

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        {
            std::lock_guard<std::mutex> lock(mutex);
            GGWave_mainLoop();
        }
    }

    if (!receiveOnly) {
        inputThread.join();
    }

    GGWave_deinit();

    SDL_CloseAudio();
    SDL_Quit();

    return 0;
    
}
