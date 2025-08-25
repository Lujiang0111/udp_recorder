#include <csignal>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include "lccl/socket.h"
#include "lccl/oss/fmt.h"
#include "pcap_record.h"
#include "udp_record.h"

struct Param
{
    std::string config_name = "config.json";
};

static bool app_running = true;

static void SigIntHandler(int sig_num)
{
    signal(SIGINT, SigIntHandler);
    app_running = false;
}

static void PcapDumpLogCb(void *opaque, pcapdump::LogLevels level, const char *file_name, int file_line, const char *content, size_t len)
{

}

static std::shared_ptr<Param> ParseParam(int argc, char **argv)
{
    std::shared_ptr<Param> param = std::make_shared<Param>();
    if (argc > 1)
    {
        param->config_name = argv[1];
    }
    return param;
}

int main(int argc, char **argv)
{
    signal(SIGINT, SigIntHandler);

    pcapdump::SetLogCallback(PcapDumpLogCb, nullptr);

    lccl::skt::InitEnv();

    std::shared_ptr<Param> param = ParseParam(argc, argv);
    if (!param)
    {
        return 0;
    }

    std::ifstream fin(param->config_name.c_str());
    if (!fin.is_open())
    {
        fmt::println("Error: Can not open file {}", param->config_name);
        return 0;
    }

    std::string config_json;
    std::stringstream buffer;
    buffer << fin.rdbuf();
    config_json = buffer.str();

    rapidjson::Document config_doc;
    if (!lccl::ParseStringToJson(config_doc, config_json))
    {
        return 0;
    }
    int64_t record_time = 0;
    lccl::GetJsonChild(config_doc, "record_time", record_time);

    std::shared_ptr<PcapRecord> pcap_record = std::make_shared<PcapRecord>();
    if (!pcap_record->Init(config_doc))
    {
        return 0;
    }

    std::shared_ptr<UdpRecord> udp_record = std::make_shared<UdpRecord>();
    if (!udp_record->Init(config_doc, pcap_record->GetFilePrefix()))
    {
        return 0;
    }

    auto curr_time = std::chrono::steady_clock::now();
    auto end_time = curr_time + std::chrono::seconds(record_time);
    while ((app_running) && ((record_time <= 0) || (curr_time < end_time)))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        curr_time = std::chrono::steady_clock::now();
    }

    udp_record = nullptr;
    pcap_record = nullptr;

    lccl::skt::DeinitEnv();
    return 0;
}
