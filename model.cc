/**
 * @file model.cc
 * @brief Network switch model to simulate identification of mac flooding
 */

#include "model.h"

Config cfg;

size_t pkt_count = 0;
size_t dropped_unavailable_count = 0;
size_t dropped_tail_drop_count = 0;
size_t dropped_processed_count = 0;
size_t flood_count = 0;
bool FailOpenState = false;

// GLOBALS
Facility *ingress_ports = nullptr;
Facility *egress_ports = nullptr;
Facility MacExtractor("MAC");
Facility Encapsulator("ENC");

Store Buffer("buffer", cfg.buffer_capacity);

Histogram hist("hist", 1.4, 0.05, 50);

CamTable table;

std::vector<std::string> mac_addresses;
std::vector<size_t> pkt_flood_ids;

/**
 * @brief Helper fn to generate random mac addresses
 * @param n Number of mac addresses to generate
 * @return vector<string> of mac addresses
 */
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

void CamTable::add_mac(std::string mac, size_t port)
{
    table[mac] = port;
}

int CamTable::get_port(std::string mac)
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

/**
 * @brief Main process of each packet in the system
 */
void Packet::Behavior()
{
    if(clone)
    {
        send(dst_port_idx);
        Cancel();
        return;
    }

    t0 = Time;
    pkt_id = ++pkt_count;

    // try to get one of the ports if open, otherwise drop this packet
    if (ingress_ports[src_port_idx].Busy())
    {
        Print("ingerss Port");
        Print(src_port_idx);
        Print("busy at time: ");
        Print(Time);
        Print("\n");
        dropped_unavailable_count++;
        Cancel();
    }
    Seize(ingress_ports[src_port_idx], 1);
    Print("ingerss Port");
    Print(src_port_idx);
    Print("seized at time: ");
    Print(Time);
    Print("\n");
    // Process packet from port to buffer
    Wait(cfg.port_delay);
    Release(ingress_ports[src_port_idx]);
    Print("ingerss Port");
    Print(src_port_idx);
    Print("released at time: ");
    Print(Time);
    Print("\n");
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
    Wait(Exponential(20 nanoseconds));
    int dst_port = table.get_port(dst_mac);
    Release(table);

    Seize(Encapsulator);
    Wait(Uniform(400 nanoseconds, 500 nanoseconds));
    Release(Encapsulator);

    // Lookup dst mac
    if (dst_port == -1)
    {
        // Flood
        // flood();
        to_flood = true;
    }
    else
    {
        dst_port_idx = dst_port;
    }

    if (FailOpenState || to_flood)
    {
        flood();
    }
    else
    {
        send(dst_port_idx);
    }
    hist(Time - t0);
}

void Packet::flood()
{
    for (size_t i = 0; i < cfg.num_ports; i++)
    {
        if (i != src_port_idx)
        {
            (new Packet(this,i))->Activate();
        }
    }
    pkt_flood_ids.push_back(pkt_id);
    flood_count++;
    Cancel();
}


void Packet::send(uint port)
{
    if (Buffer.Full())
    {
        dropped_processed_count++;
        Cancel();
    }
    Enter(Buffer);
    Seize(egress_ports[port]);
    Print("egress Port");
    Print(port);
    Print("seized at time: ");
    Print(Time);
    Print("\n");
    Leave(Buffer);
    Wait(cfg.port_delay);
    Release(egress_ports[port]);
    Print("egress Port");
    Print(port);
    Print("released at time: ");
    Print(Time);
    Print("\n");
}

void Packet::extract_mac()
{
    Enter(Buffer);
    Seize(MacExtractor);
    Leave(Buffer);
    Wait(Uniform(200 nanoseconds, 300 nanoseconds));
    Release(MacExtractor);
}


void PacketGenerator::Behavior()
{
    size_t rnd_port_idx = size_t(Random() * cfg.num_ports);
    size_t rnd_src_mac_idx = size_t(Random() * cfg.mac_addresses);
    size_t rnd_dst_mac_idx = size_t(Random() * cfg.mac_addresses);

    // size_t rnd_src_mac_idx = size_t(rand() % mac_addresses->size())

    if (rnd_src_mac_idx == rnd_dst_mac_idx)
    {
        rnd_dst_mac_idx = (rnd_dst_mac_idx + 1) % (cfg.mac_addresses);
    }
    if (int(rnd_port_idx) == prev_port)
    {
        rnd_port_idx = (rnd_port_idx + 1) % cfg.num_ports;
    }
    prev_port = rnd_port_idx;
    std::string src = mac_addresses[rnd_src_mac_idx];
    std::string dst = mac_addresses[rnd_dst_mac_idx];
    // fprintf(stderr, "Packet from %lu to %lu\n", rnd_src_mac_idx, rnd_dst_mac_idx);
    (new Packet(src, dst, rnd_port_idx))->Activate();
    Activate(Time + Exponential(cfg.pkt_arrival_rate));
}

void print_help()
{
    printf("Usage: model [OPTIONS]\n");
    printf("Options:\n");
    printf("  -n <num_ports>               Number of ports\n");
    printf("  -r <pkt_arrival_rate USEC>   Packet arrival rate\n");
    printf("  -t <sim_duration SEC>        Duration of the Simulation\n");
    printf("  -b <buffer_capacity>         Buffer capacity\n");
    printf("  -m <mac_table_size>          MAC table size\n");
    printf("  -d <port_delay USEC>         Port delay\n");
    printf("  -c <mac_count>               Number of mac addr to generate\n");
    printf("  -o <out_file>                Output file\n");
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
            config.pkt_arrival_rate = atof(optarg) microseconds;
            break;
        case 't':
            config.sim_duration = atof(optarg) seconds;
            break;
        case 'b':
            config.buffer_capacity = atoi(optarg);
            break;
        case 'm':
            config.mac_table_size = atoi(optarg);
            break;
        case 'd':
            config.port_delay = atof(optarg) microseconds;
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
    Facility iports[cfg.num_ports];
    ingress_ports = iports;

    Facility eports[cfg.num_ports];
    egress_ports = eports;

    if (ingress_ports == nullptr)
    {
        fprintf(stderr, "Error: Ports not initialized\n");
        return 1;
    }

    SetOutput(cfg.out_file.c_str());
    Init(0, cfg.sim_duration);

    RandomSeed(time(NULL));
    table.SetName("Cam Table");

    for (size_t i = 0; i < cfg.num_ports; i++)
    {
        ingress_ports[i].SetName("Ingress Port " + std::to_string(i));
    }

    for (size_t i = 0; i < cfg.num_ports; i++)
    {
        egress_ports[i].SetName("Egress Port " + std::to_string(i));
    }

    (new PacketGenerator())->Activate();
    Run();
    for (size_t i = 0; i < cfg.num_ports; i++)
    {
        ingress_ports[i].Output();
    }

    for (size_t i = 0; i < cfg.num_ports; i++)
    {
        egress_ports[i].Output();
    }

    Buffer.Output();
    MacExtractor.Output();
    Encapsulator.Output();
    table.Output();
    hist.Output();

    Print("\nTotal Packets: ");
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
