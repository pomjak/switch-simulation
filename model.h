#include "simlib.h"
#include "time.h"
#include "getopt.h"
#include <map>
#include <string>
#include <iomanip>
#include <sstream>
#include <vector>
#include "cstdlib"

#define nanoseconds *1.0e-3
#define microseconds *1.0
#define milliseconds *1.0e3
#define seconds *1.0e6

struct Config
{
    size_t packets_to_send = 1000;
    double pkt_arrival_rate = 160 microseconds;
    double sim_duration = 1 seconds;
    double port_delay = 512 nanoseconds;
    // double mac_extract_time = 0.001;
    size_t num_ports = 5;
    size_t buffer_capacity = 2048;
    size_t mac_table_size = 2048;
    size_t mac_addresses = 1000;
    std::string out_file = "model.out";
};

extern Config cfg;
extern size_t pkt_count;
extern size_t dropped_unavailable_count;
extern size_t dropped_tail_drop_count;
extern size_t dropped_processed_count;
extern size_t flood_count;
extern Facility *ingress_ports;
extern Facility *egress_ports;
extern Facility MacExtractor;
extern Facility Encapsulator;
extern Store Buffer;
extern bool FailOpenState;
extern std::vector<std::string> mac_addresses;
extern Histogram hist;
extern std::vector<size_t> pkt_flood_ids;

/**
 * @brief Cam Table to store MAC addresses
 */
class CamTable : public Facility
{
public:
    std::map<std::string, size_t> table;

    void add_mac(std::string mac, size_t port);

    int get_port(std::string mac);

    size_t size()
    {
        return table.size();
    }
};

extern CamTable table;

class Packet : public Process
{
public:
    Packet(std::string src, std::string dst, uint port) : src_port_idx(port), src_mac(src), dst_mac(dst) {}
    Packet(uint port) : src_port_idx(port), src_mac(""), dst_mac("") {}

private:
    double t0;
    uint src_port_idx;
    uint dst_port_idx;
    std::string src_mac;
    std::string dst_mac;
    size_t pkt_id;
    bool to_flood = false;

    void Behavior();

    void flood();
    void send(uint port);

    void extract_mac();
};


class PacketGenerator : public Event
{
    int prev_port = -1;

    void Behavior();
};