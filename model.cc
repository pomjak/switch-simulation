/**
 * @file model.cc
 * @brief Network switch model to simulate identification of mac flooding
 */

#include "simlib.h"

#define SIM_END_TIME 1000
#define NUM_PORTS 5
#define PKT_ARRIVAL_RATE 1
#define OUT_FILE "model.out"
#define BUFFER_CAPACITY 2000
#define MAC_EXTRACT_TIME 0.001

// GLOBALS
Facility Ports[NUM_PORTS];
Facility MacExtractor("MAC");
Store Buffer(BUFFER_CAPACITY);

class Packet : public Process
{
    double arrivalTime;

    void Behavior()
    {
        arrivalTime = Time;
        int rnd_port_idx = int(Random() * 100) % NUM_PORTS;
        // try to get one of the ports if open, otherwise drop this packet
        if (Ports[rnd_port_idx].Busy())
        {
            return;
        }
        Seize(Ports[rnd_port_idx]);
        // TODO
        Wait(0.001);
        Release(Ports[rnd_port_idx]);
        // try to get buffer space, otherwise drop this packet
        if (!Buffer.Full())
        {
            Enter(Buffer);
            Seize(MacExtractor);
            Leave(Buffer);
            Wait(MAC_EXTRACT_TIME);
            Release(MacExtractor);
        }
        else
        {
            return;
        }
    }
};

class PacketGenerator : public Event
{
    void Behavior()
    {
        Packet *packet = new Packet();
        packet->Activate();
        Activate(Time + Exponential(PKT_ARRIVAL_RATE));
    }
};

int main(int argc, char *argv[])
{
    SetOutput(OUT_FILE);
    Init(0, SIM_END_TIME);
    PacketGenerator *packetGenerator = new PacketGenerator();
    packetGenerator->Activate();
    Run();
    return 0;
}