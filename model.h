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
    double load = 0.5;
    double sim_duration = 1 seconds;
    size_t num_ports = 5;
    size_t num_gens = 5;
    size_t buffer_capacity = 131072;
    size_t mac_table_size = 2048;
    size_t mac_addresses = 1000;
    size_t packet_size = 0;
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
    Packet(std::string src, std::string dst, uint port, size_t size) : src_port_idx(port), src_mac(src), dst_mac(dst), pkt_size(size), port_delay((pkt_size * 8) / 1.0e3) {}
    Packet(uint port, size_t s) : src_port_idx(port), src_mac(""), dst_mac(""), pkt_size(s) {}
    Packet(Packet *p, uint port_to_send) : src_port_idx(p->src_port_idx), dst_port_idx(port_to_send), src_mac(p->src_mac), dst_mac(p->dst_mac), pkt_size(p->pkt_size), port_delay(p->port_delay), clone(true) {}

private:
    double t0;
    uint src_port_idx;
    uint dst_port_idx;
    std::string src_mac;
    std::string dst_mac;
    size_t pkt_id;
    size_t pkt_size;
    double port_delay;
    bool to_flood = false;
    bool clone = false;

    void Behavior();

    void flood();
    void send(uint port);

    void extract_mac();
    void send_clone();
};

class PacketGenerator : public Event
{
    size_t port;

    void Behavior();

public:
    PacketGenerator(int p) : port(p) {}
};