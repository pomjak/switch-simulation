/**
 * @file model.cc
 * @brief Network switch model to simulate identification of mac flooding
 */

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

std::vector<std::string> generateMacAddresses(size_t n)
{
    std::vector<std::string> macs;
    for (size_t i = 0; i < n; i++)
    {
        std::stringstream ss;

        for (size_t j = 0; j < 6; j++)
        {
            ss << std::hex << rand() % 256;
            if (j < 5)
            {
                ss << ":";
            }
        }

        macs.push_back(ss.str());
    }
    return macs;
}

struct Config
{
    size_t packets_to_send = 1000;
    double pkt_arrival_rate = 2 microseconds;
    double sim_duration = 1 seconds;
    double port_delay = 0.75 microseconds;
    // double mac_extract_time = 0.001;
    size_t num_ports = 5;
    size_t buffer_capacity = 2048;
    size_t mac_table_size = 2048;
    size_t mac_addresses = 1000;
    std::string out_file = "model.out";
};

Config cfg;

size_t pkt_count = 0;
size_t dropped_unavailable_count = 0;
size_t dropped_tail_drop_count = 0;
size_t dropped_processed_count = 0;
size_t flood_count = 0;

// GLOBALS
Facility *Ports = nullptr;

Facility MacExtractor("MAC");

Store Buffer(cfg.buffer_capacity);

bool FailOpenState = false;

std::vector<std::string> mac_addresses;

Histogram hist("hist", 0, 100, 10);

std::vector<size_t> pkt_flood_ids;

/**
 * @brief Cam Table to store MAC addresses
 */
class CamTable : public Facility
{
public:
    std::map<std::string, size_t> table;

    void add_mac(std::string mac, size_t port)
    {
        table[mac] = port;
    }

    int get_port(std::string mac)
    {
        try
        {
            return table.at(mac);
        }
        catch (const std::exception &e)
        {
            return -1;
        }
    }

    size_t size()
    {
        return table.size();
    }
};

CamTable table;

class Packet : public Process
{
public:
    Packet(std::string src, std::string dst, uint port) : src_port_idx(port), src_mac(src), dst_mac(dst) {}
    Packet(uint port) : src_port_idx(port), src_mac(""), dst_mac("") {}

private:
    double arrivalTime;
    uint src_port_idx;
    uint dst_port_idx;
    std::string src_mac;
    std::string dst_mac;
    size_t pkt_id;

    void Behavior()
    {

        arrivalTime = Time;
        pkt_id = ++pkt_count;

        // try to get one of the ports if open, otherwise drop this packet
        if (Ports[src_port_idx].Busy())
        {
            dropped_unavailable_count++;
            Cancel();
        }
        Seize(Ports[src_port_idx], 1);

        // Process packet from port to buffer
        Wait(cfg.port_delay);
        Release(Ports[src_port_idx]);
        // try to get buffer space, otherwise drop this packet
        if (!Buffer.Full())
        {
            extract_mac();
        }
        else
        {
            dropped_tail_drop_count++;
            Cancel();
        }

        // Wait for CAM table
        Seize(table);

        // Update CAM table
        Wait(Uniform(50 nanoseconds, 100 nanoseconds));

        if (table.size() >= cfg.mac_table_size)
        {
            FailOpenState = true;
        }
        else
        {
            table.add_mac(src_mac, src_port_idx);
        }

        // lookuo wait
        Wait(Uniform(10 nanoseconds, 20 nanoseconds));
        int dst_port = table.get_port(dst_mac);
        // Lookup dst mac
        if (dst_port == -1)
        {
            // Flood
            flood();
        }
        else
        {
            dst_port_idx = dst_port;
        }

        Release(table);

        // try to get one of the ports if open, otherwise drop this packet
        if (!Buffer.Full())
        {
            if (FailOpenState)
            {
                flood();
            }
            else
            {
                send(dst_port_idx);
            }
        }
        else
        {
            dropped_processed_count++;
            Cancel();
        }
        hist(Time - arrivalTime);
    }

    void flood()
    {
        for (size_t i = 0; i < cfg.num_ports; i++)
        {
            if (i != src_port_idx)
            {
                send(i);
            }
        }
        pkt_flood_ids.push_back(pkt_id);
        flood_count++;
    }

    void send(uint port)
    {
        Enter(Buffer);
        Seize(Ports[port]);
        Leave(Buffer);
        Wait(cfg.port_delay);
        Release(Ports[port]);
    }

    void extract_mac()
    {
        Enter(Buffer);
        Seize(MacExtractor);
        Leave(Buffer);
        Wait(Uniform(10 nanoseconds, 50 nanoseconds));
        Release(MacExtractor);
    }
};

class PacketGenerator : public Event
{
    void Behavior()
    {
        size_t rnd_port_idx = size_t(Random() * cfg.num_ports);
        size_t rnd_src_mac_idx = size_t(Random() * cfg.mac_addresses);
        size_t rnd_dst_mac_idx = size_t(Random() * cfg.mac_addresses);
        // size_t rnd_src_mac_idx = size_t(rand() % mac_addresses->size())

        if (rnd_src_mac_idx == rnd_dst_mac_idx)
        {
            rnd_dst_mac_idx = (rnd_dst_mac_idx + 1) % (cfg.mac_addresses);
        }
        std::string src = mac_addresses[rnd_src_mac_idx];
        std::string dst = mac_addresses[rnd_dst_mac_idx];
        // fprintf(stderr, "Packet from %lu to %lu\n", rnd_src_mac_idx, rnd_dst_mac_idx);
        (new Packet(src, dst, rnd_port_idx))->Activate();
        Activate(Time + Exponential(cfg.pkt_arrival_rate));
    }
};

void print_help()
{
    printf("Usage: model [OPTIONS]\n");
    printf("Options:\n");
    printf("  -n <num_ports>          Number of ports\n");
    printf("  -r <pkt_arrival_rate>   Packet arrival rate\n");
    printf("  -t <sim_duration>       Duration of the Simulation\n");
    printf("  -b <buffer_capacity>    Buffer capacity\n");
    printf("  -m <mac_table_size>     MAC table size\n");
    printf("  -d <port_delay>         Port delay\n");
    printf("  -c <mac_count>          Number of mac addr to generate\n");
    printf("  -o <out_file>           Output file\n");
}

Config parse_args(int argc, char *argv[])
{
    Config config;
    int c;
    while ((c = getopt(argc, argv, "n:r:t:b:m:d:c:p:")) != -1)
    {
        switch (c)
        {
        case 'n':
            config.num_ports = atoi(optarg);
            break;
        case 'r':
            config.pkt_arrival_rate = atof(optarg);
            break;
        case 't':
            config.sim_duration = atof(optarg);
            break;
        case 'b':
            config.buffer_capacity = atoi(optarg);
            break;
        case 'm':
            config.mac_table_size = atoi(optarg);
            break;
        case 'd':
            config.port_delay = atof(optarg);
            break;
        case 'o':
            config.out_file = std::string(optarg);
            break;
        case 'c':
            config.mac_addresses = atoi(optarg);
            break;
        case 'p':
            config.packets_to_send = atoi(optarg);
            break;
        default:
            print_help();
            exit(1);
        }
    }
    return config;
}

int main(int argc, char *argv[])
{
    cfg = parse_args(argc, argv);
    mac_addresses = generateMacAddresses(cfg.mac_addresses);

    Facility ports[cfg.num_ports];
    Ports = ports;

    if (Ports == nullptr)
    {
        fprintf(stderr, "Error: Ports not initialized\n");
        return 1;
    }

    SetOutput(cfg.out_file.c_str());
    Init(0, cfg.sim_duration);

    RandomSeed(time(NULL));
    table.SetName("Cam Table");
    (new PacketGenerator())->Activate();
    Run();
    for (size_t i = 0; i < cfg.num_ports; i++)
    {
        Ports[i].Output();
    }
    Buffer.Output();
    MacExtractor.Output();
    hist.Output();
    table.Output();
    Print("Total Packets: ");
    Print(pkt_count);
    Print("\n");

    std::string dropped_msg = "Dropped Unavailable: " + std::to_string(dropped_unavailable_count) + "\n" + "Dropped Tail Drop: " + std::to_string(dropped_tail_drop_count) + "\n" + "Dropped Processed: " + std::to_string(dropped_processed_count) + "\n";
    Print(dropped_msg.c_str());

    size_t total_dropped = dropped_unavailable_count + dropped_tail_drop_count + dropped_processed_count;
    std::string dropped_total = "Dropped Total: " + std::to_string(total_dropped) + "\n";
    std::string dropped_percent = "Dropped %: " + std::to_string((double)total_dropped / pkt_count * 100) + "\n";
    Print(dropped_total.c_str());
    Print(dropped_percent.c_str());

    if (FailOpenState)
    {
        Print("Fail Open State\n");
    }

    Print("Flood Count: ");
    Print(flood_count);
    Print("\n");

    // Print("Flooded Indices: ");
    // for (size_t i = 0; i < pkt_flood_ids.size(); i++)
    // {
    //     Print(pkt_flood_ids[i]);
    //     Print(" ");
    // }
    // Print("\n");

    Print("Cam Table Size: ");
    Print(table.size());
    Print("\n");

    Print("Random Macs size: ");
    Print(mac_addresses.size());
    Print("\n");

    // Print("Cam Table Contents: \n");
    // for (auto const &pair : table.table)
    // {
    //     Print(pair.first.c_str());
    //     Print(" ");
    //     Print(pair.second);
    //     Print("\n");
    // }
    return 0;
}
