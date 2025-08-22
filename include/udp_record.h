#ifndef UDP_RECORDER_INCLUDE_UDP_RECORD_H_
#define UDP_RECORDER_INCLUDE_UDP_RECORD_H_

#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "lccl/event.h"
#include "lccl/oss/json.h"

class UdpRecord
{
public:
    struct Param
    {
        enum class Types
        {
            kUdp = 0,
            kRtp,
        };

        Types type = Types::kUdp;
        std::string ip;
        uint16_t port;
        std::string itf;
        std::string file_prefix;
    };

public:
    UdpRecord(const UdpRecord &) = delete;
    UdpRecord &operator=(const UdpRecord &) = delete;

    UdpRecord();
    virtual ~UdpRecord();

    bool Init(const rapidjson::Value &param_val, std::string file_prefix);
    void Deinit();

private:
    bool ParseParam(const rapidjson::Value &param_val, std::string file_prefix);
    void HandleCallback(int fd, int triggered_events);

    void WorkThread();
    void Monitor();

private:
    std::shared_ptr<Param> param_;

    int fd_;
    std::vector<uint8_t> recv_buffer_;
    std::shared_ptr<lccl::evt::IMultiplex> multiplex_;

    std::thread work_thread_;
    bool work_thread_running_;

    std::ofstream frecord_;

    std::chrono::steady_clock::time_point start_monitor_time_;
    int64_t prev_duration_div_;
    int64_t monitor_byte_;
    int64_t total_byte_;
};

#endif // !UDP_RECORDER_INCLUDE_UDP_RECORD_H_
