/**
 * @file model.cc
 * @brief Network switch model to simulate identification of mac flooding
 */

#include "model.h"

// GLOBALS
Config cfg;
Facility *ingress_ports = nullptr;
Facility *egress_ports = nullptr;
Facility MacExtractor("MAC");
Facility Encapsulator("ENC");
Store Buffer("Shared Buffer", cfg.buffer_capacity);
Histogram hist("Packet Lifetime", 1.1, 0.025, 50);
Histogram hist2("Port Delay", 0, 0.025, 100);
Histogram PacketSizes("Packet Sizes", 64, 50, 50);

CamTable table;
std::vector<std::string> mac_addresses;
std::vector<size_t> pkt_flood_ids;

size_t pkt_count = 0;
size_t dropped_unavailable_count = 0;
size_t dropped_tail_drop_count = 0;
size_t dropped_processed_count = 0;
size_t flood_count = 0;
bool FailOpenState = false;

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

    if (clone)
    {
        if (egress_ports[dst_port_idx].Busy())
        {
            if (Buffer.Full())
            {
                dropped_tail_drop_count++;
                Cancel();
            }

            Priority = 1;
            Enter(Buffer, pkt_size);

            Seize(egress_ports[dst_port_idx]);
            Leave(Buffer, pkt_size);
            Priority = 0;
            Wait(port_delay);
            Release(egress_ports[dst_port_idx]);
        }
        else
        {
            send(dst_port_idx);
        }
        Cancel();
    }

    t0 = Time;
    pkt_id = ++pkt_count;

    hist2(port_delay);

    // try to get one of the ports if open, otherwise drop this packet
    if (ingress_ports[src_port_idx].Busy())
    {
        dropped_unavailable_count++;
        Cancel();
    }
    Seize(ingress_ports[src_port_idx], 1);
    Wait(port_delay);

    Release(ingress_ports[src_port_idx]);

    if (MacExtractor.Busy())
    {
        if (Buffer.Full())
        {
            dropped_tail_drop_count++;
            Cancel();
        }
        Enter(Buffer, pkt_size);
        Seize(MacExtractor);
        Leave(Buffer, pkt_size);
        Wait(Uniform(300 nanoseconds, 400 nanoseconds));
        Release(MacExtractor);
    }
    else
    {
        Seize(MacExtractor);
        Wait(Uniform(300 nanoseconds, 400 nanoseconds));
        Release(MacExtractor);
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
    Wait(Uniform(20 nanoseconds, 40 nanoseconds));
    int dst_port = table.get_port(dst_mac);
    Release(table);

    Seize(Encapsulator);
    Wait(Uniform(400 nanoseconds, 500 nanoseconds));
    Release(Encapsulator);

    // Lookup dst mac
    if (dst_port == -1 || FailOpenState)
    {
        // to_flood = true;
        flood();
    }
    else
    {
        dst_port_idx = dst_port;
        if (egress_ports[dst_port_idx].Busy())
        {
            if (Buffer.Full())
            {
                dropped_tail_drop_count++;
                Cancel();
            }
            Priority = 1;
            Enter(Buffer, pkt_size);

            Seize(egress_ports[dst_port_idx]);
            Leave(Buffer, pkt_size);
            Priority = 0;
            Wait(port_delay);
            Release(egress_ports[dst_port_idx]);
        }
        else
        {

            send(dst_port_idx);
        }
    }

    hist(Time - t0);
}

void Packet::flood()
{
    for (size_t i = 0; i < cfg.num_ports; i++)
    {
        if (i != src_port_idx)
        {
            (new Packet(this, i))->Activate();
        }
    }
    pkt_flood_ids.push_back(pkt_id);
    flood_count++;
}

void Packet::send(uint port)
{
    Seize(egress_ports[port]);
    Wait(port_delay);
    Release(egress_ports[port]);
}

void Packet::extract_mac()
{
    Seize(MacExtractor);
    Leave(Buffer, pkt_size);
    Wait(Uniform(300 nanoseconds, 400 nanoseconds));
    Release(MacExtractor);
}

void PacketGenerator::Behavior()
{
    size_t rnd_src_mac_idx = size_t(Random() * cfg.mac_addresses);
    size_t rnd_dst_mac_idx = size_t(Random() * cfg.mac_addresses);

    size_t pkt_size = cfg.packet_size;
    if (pkt_size == 0)
    {
        double rand = Random();
        if (rand < 0.5)
        {
            pkt_size = 64; // Min Ethernet frame
        }
        else if (rand < 0.9)
        {
            pkt_size = size_t(Uniform(65, 500));
        }
        else
        {
            pkt_size = size_t(Uniform(501, 1518)); // Max standard Ethernet frame
        }
    }

    PacketSizes(pkt_size);

    if (rnd_src_mac_idx == rnd_dst_mac_idx)
    {
        rnd_dst_mac_idx = (rnd_dst_mac_idx + 1) % (cfg.mac_addresses);
    }

    std::string src = mac_addresses[rnd_src_mac_idx];
    std::string dst = mac_addresses[rnd_dst_mac_idx];

    // Create new packet and schedule next arrival
    (new Packet(src, dst, port, pkt_size))->Activate();

    double next_arrival = ((pkt_size * 8) / 1.0e3) / (cfg.load);
    Activate(Time + next_arrival);
}

void print_help()
{
    printf("Usage: model [OPTIONS]\n");
    printf("Options:\n");
    printf("  -n <num_ports>               Number of ports\n");
    // printf("  -p <packets per sec>         Pps to send\n");
    printf("  -t <sim_duration SEC>        Duration of the Simulation\n");
    printf("  -b <buffer_capacity>         Buffer capacity\n");
    printf("  -m <mac_table_size>          MAC table size\n");
    printf("  -c <mac_count>               Number of mac addr to generate\n");
    printf("  -s <packet_size>             Packet size\n");
    printf("  -l <load percent>            Load percent\n");
    printf("  -o <out_file>                Output file\n");
}

Config parse_args(int argc, char *argv[])
{
    Config config;
    int c;
    while ((c = getopt(argc, argv, "n:t:b:m:c:s:l:g:")) != -1)
    {
        switch (c)
        {
        case 'n':
            config.num_ports = atoi(optarg);
            break;
        case 't':
            config.sim_duration = atof(optarg) seconds;
            break;
        case 'b':
            config.buffer_capacity = atoll(optarg);
            break;
        case 'm':
            config.mac_table_size = atoi(optarg);
            break;
        case 'o':
            config.out_file = std::string(optarg);
            break;
        case 'c':
            config.mac_addresses = atoi(optarg);
            break;
        case 'l':
            config.load = atof(optarg);
            break;
        case 's':
            config.packet_size = atoi(optarg);
            break;
        case 'g':
            config.num_gens = atoi(optarg);
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
    if (egress_ports == nullptr)
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

    for (size_t i = 0; i < cfg.num_gens; i++)
    {
        auto p = new PacketGenerator(i);
        p->Activate();
    }

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
    hist2.Output();
    PacketSizes.Output();

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


    Print("Cam Table Size: ");
    Print(table.size());
    Print("\n");

    Print("Random Macs size: ");
    Print(mac_addresses.size());
    Print("\n");

    return 0;
}
