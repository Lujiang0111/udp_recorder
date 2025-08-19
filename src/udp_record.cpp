#if defined(_MSC_VER)
#else
#include <unistd.h>
#endif

#include "lccl/socket.h"
#include "lccl/oss/fmt.h"
#include "udp_record.h"

constexpr int64_t kMonitorIntervalNs = 1000000000;

UdpRecord::UdpRecord() :
    fd_(-1),
    recv_buffer_(65536, 0),
    work_thread_running_(false),
    prev_duration_div_(0),
    monitor_byte_(0),
    total_byte_(0)
{

}

UdpRecord::~UdpRecord()
{
    Deinit();
}

bool UdpRecord::Init(const rapidjson::Value &param_val, std::string file_prefix)
{
    if (!ParseParam(param_val, file_prefix))
    {
        return false;
    }

    lccl::skt::AddrTypes addr_type = lccl::skt::GetIpType(param_->ip.c_str());
    switch (addr_type)
    {
    case lccl::skt::AddrTypes::kIpv4:
        fd_ = static_cast<int>(socket(AF_INET, SOCK_DGRAM, 0));
        break;
    case lccl::skt::AddrTypes::kIpv6:
        fd_ = static_cast<int>(socket(AF_INET6, SOCK_DGRAM, 0));
        break;
    default:
        return false;
    }

    if (fd_ < 0)
    {
        fmt::println("Create fd fail!");
        return false;
    }

    // 设置socket接收缓冲区
    int opt = 1 << 24;  // 16MB
    setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char *>(&opt), sizeof(opt));

    // 设置REUSEADDR
    int yes = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&yes), sizeof(yes));

    // 设置为非阻塞模式
    lccl::skt::SetBlockMode(fd_, false);

    std::shared_ptr<lccl::skt::IAddr> source_addr = lccl::skt::CreateAddr(param_->ip.c_str(), param_->port, false);
    if (!source_addr)
    {
        fmt::println("Create source addr fail, url={}:{}", param_->ip, param_->port);
        return false;
    }

    std::shared_ptr<lccl::skt::IAddr> local_addr = nullptr;
    if (!param_->itf.empty())
    {
        lccl::skt::AddrTypes local_addr_type = lccl::skt::GetIpType(param_->itf.c_str());
        switch (local_addr_type)
        {
        case lccl::skt::AddrTypes::kIpv4:
        case lccl::skt::AddrTypes::kIpv6:
            local_addr = lccl::skt::CreateAddr(param_->itf.c_str(), 0, true);
            break;

        default:
            local_addr = lccl::skt::CreateLocalAddr(param_->itf.c_str(), addr_type);
            break;
        }
    }

    if ((!local_addr) && (source_addr->IsMulticast()))
    {
        fmt::println("Multicast but not interface, url={}:{}", param_->ip, param_->port);
        return false;
    }

    if (0 != source_addr->Bind(fd_))
    {
        if ((!source_addr->IsMulticast()) || (0 != local_addr->Bind(fd_)))
        {
            fmt::println("Bind fail, url={}:{}, interface={}", param_->ip, param_->port, param_->itf);
            return false;
        }
    }

    if (source_addr->IsMulticast())
    {
        if (!local_addr->JoinMulticastGroup(fd_, source_addr.get()))
        {
            fmt::println("Join multicast group fail, url={}:{}, interface={}", param_->ip, param_->port, param_->itf);
            return false;
        }
    }

    multiplex_ = lccl::evt::CreateMultiplex(lccl::evt::MultiplexTypes::kAuto, 10);
    if (!multiplex_)
    {
        fmt::println("Create multiplex fail");
        return false;
    }

    multiplex_->RegisterHandler(fd_, lccl::evt::EventTypes::kRead,
        std::bind(&UdpRecord::HandleCallback, this, std::placeholders::_1, std::placeholders::_2));

    std::string record_file_name = fmt::format("{}.data", param_->file_prefix);
    frecord_.open(record_file_name.c_str(), std::ios::binary);
    if (!frecord_.is_open())
    {
        fmt::println("Open record file={} fail!", record_file_name);
        return false;
    }

    work_thread_running_ = true;
    work_thread_ = std::thread(&UdpRecord::WorkThread, this);
    return true;
}

void UdpRecord::Deinit()
{
    work_thread_running_ = false;
    if (work_thread_.joinable())
    {
        work_thread_.join();
    }

    multiplex_ = nullptr;

    if (fd_ >= 0)
    {
#if defined(_MSC_VER)
        closesocket(fd_);
#else
        close(fd_);
#endif
        fd_ = -1;
    }

    param_ = nullptr;

    // 最后加个换行
    fmt::println("");
}

bool UdpRecord::ParseParam(const rapidjson::Value &param_val, std::string file_prefix)
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
    param_->file_prefix = file_prefix;
    return true;
}

void UdpRecord::HandleCallback(int fd, int triggered_events)
{
    if (triggered_events | lccl::evt::EventTypes::kRead)
    {
#if defined(_MSC_VER)
        int len = recv(fd, reinterpret_cast<char *>(&recv_buffer_[0]), 65536, 0);
#else
        int len = recv(fd, &recv_buffer_[0], 65536, 0);
#endif

        if (len > 0)
        {
            frecord_.write(reinterpret_cast<char *>(&recv_buffer_[0]), len);
            monitor_byte_ += len;
            total_byte_ += len;
        }
    }
}

void UdpRecord::WorkThread()
{
    start_monitor_time_ = std::chrono::steady_clock::now();
    prev_duration_div_ = 0;
    monitor_byte_ = 0;

    while (work_thread_running_)
    {
        multiplex_->ExecuteOnce();
        Monitor();
    }
}

void UdpRecord::Monitor()
{
    auto curr_time = std::chrono::steady_clock::now();
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(curr_time - start_monitor_time_).count();
    int64_t duration_div = duration_ns / kMonitorIntervalNs;
    if (duration_div != prev_duration_div_)
    {
        fmt::print("\r{}s {:.2f}MB {}kbps",
            duration_ns / 1000000000,
            static_cast<double>(total_byte_) / 1000000,
            monitor_byte_ * 8 / 1000);
        fflush(stdout);
        monitor_byte_ = 0;
    }
    prev_duration_div_ = duration_div;
}
