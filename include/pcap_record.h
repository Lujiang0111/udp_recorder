#ifndef UDP_RECORDER_INCLUDE_PCAP_RECORD_H_
#define UDP_RECORDER_INCLUDE_PCAP_RECORD_H_

#include <string>
#include "lccl/oss/json.h"
#include "pcap_dump.h"

class PcapRecord
{
public:
    struct Param
    {
        std::string ip;
        uint16_t port;
        std::string itf;
    };

public:
    PcapRecord(const PcapRecord &) = delete;
    PcapRecord &operator=(const PcapRecord &) = delete;

    PcapRecord();
    virtual ~PcapRecord();

    bool Init(const rapidjson::Value &param_val);
    void Deinit();

    const std::string &GetFilePrefix() const;

private:
    bool ParseParam(const rapidjson::Value &param_val);

private:
    std::shared_ptr<Param> param_;

    std::string file_prefix_;
    std::shared_ptr<pcapdump::IDumper> dumper_;
};

#endif // !UDP_RECORDER_INCLUDE_PCAP_RECORD_H_
