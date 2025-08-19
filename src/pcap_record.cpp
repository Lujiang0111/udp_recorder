#include "lccl/oss/fmt.h"
#include "pcap_record.h"

PcapRecord::PcapRecord()
{

}

PcapRecord::~PcapRecord()
{
    Deinit();
}

bool PcapRecord::Init(const rapidjson::Value &param_val)
{
    if (!ParseParam(param_val))
    {
        return false;
    }

    time_t curr_time = time(nullptr);
    std::tm time_tm;
#if defined(_MSC_VER)
    ::localtime_s(&time_tm, &curr_time);
#else
    ::localtime_r(&curr_time, &time_tm);
#endif
    file_prefix_ = fmt::format("record_{:04}{:02}{:02}_{:02}{:02}{:02}",
        time_tm.tm_year + 1900,
        time_tm.tm_mon + 1,
        time_tm.tm_mday,
        time_tm.tm_hour,
        time_tm.tm_min,
        time_tm.tm_sec);

    dumper_ = pcapdump::CreateDumper();
    if (!dumper_)
    {
        fmt::println("Create dumper fail");
        return false;
    }

    dumper_->SetParam(pcapdump::IDumper::ParamNames::kIp, param_->ip.c_str(), param_->ip.length());
    std::string port_str = fmt::format("{}", param_->port);
    dumper_->SetParam(pcapdump::IDumper::ParamNames::kPort, port_str.c_str(), port_str.length());
    dumper_->SetParam(pcapdump::IDumper::ParamNames::kInterface, param_->itf.c_str(), param_->itf.length());
    bool promisc = false;
    dumper_->SetParam(pcapdump::IDumper::ParamNames::kPromisc, &promisc, sizeof(promisc));
    int64_t segment_interval = 0;
    dumper_->SetParam(pcapdump::IDumper::ParamNames::kSegmentInterval, &segment_interval, sizeof(segment_interval));
    size_t segment_size = 0;
    dumper_->SetParam(pcapdump::IDumper::ParamNames::kSegmentSize, &segment_size, sizeof(segment_size));
    std::string dump_dir = ".";
    dumper_->SetParam(pcapdump::IDumper::ParamNames::kDumpDir, dump_dir.c_str(), dump_dir.length());
    std::string dump_name = fmt::format("{}.pcap", file_prefix_);
    dumper_->SetParam(pcapdump::IDumper::ParamNames::kDumpName, dump_name.c_str(), dump_name.length());

    if (!dumper_->Init())
    {
        fmt::println("Init dumper fail");
        return false;
    }
    return true;
}

void PcapRecord::Deinit()
{
    dumper_ = nullptr;
    param_ = nullptr;
}

const std::string &PcapRecord::GetFilePrefix() const
{
    return file_prefix_;
}

bool PcapRecord::ParseParam(const rapidjson::Value &param_val)
{
    param_ = std::make_shared<Param>();
    if (!lccl::GetJsonChild(param_val, "ip", param_->ip))
    {
        fmt::println("Param ip not found");
        return false;
    }

    if (!lccl::GetJsonChild<uint16_t>(param_val, "port", param_->port))
    {
        fmt::println("Param port not found");
        return false;
    }

    lccl::GetJsonChild(param_val, "interface", param_->itf);
    return true;
}
