// scratch/my-algo.cc
// FINAL REAL PACKET VERSION (ns-3.47)
// GTAR + Real UDP Traffic + FlowMonitor + NetAnim
#include "ns3/flow-monitor-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

#include <iostream>
using namespace ns3;
using namespace std;

int main(int argc, char *argv[])
{
    uint32_t numNodes = 25;
    double simTime = 60.0;
    double packetRate = 5.0;
    uint32_t packetSize = 512;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of Nodes", numNodes);
    cmd.AddValue("simTime", "Simulation Time", simTime);
    cmd.AddValue("packetRate", "Packets/sec", packetRate);
    cmd.Parse(argc, argv);

    // -------------------------------------------------
    // CREATE NODES
    // -------------------------------------------------
    NodeContainer nodes;
    nodes.Create(numNodes);

    // -------------------------------------------------
    // WIFI ADHOC
    // -------------------------------------------------
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // -------------------------------------------------
    // MOBILITY
    // -------------------------------------------------
    MobilityHelper mobility;

    mobility.SetPositionAllocator(
        "ns3::RandomRectanglePositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]")
    );

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // -------------------------------------------------
    // INTERNET STACK
    // -------------------------------------------------
    InternetStackHelper internet;

    // Enable routing (MANDATORY)
    Ipv4StaticRoutingHelper staticRouting;
    internet.SetRoutingHelper(staticRouting);

    internet.Install(nodes);
    Ipv4AddressHelper ip;
    ip.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer interfaces = ip.Assign(devices);

    // -------------------------------------------------
    // SINK NODE = Node 0
    // -------------------------------------------------
    uint16_t port = 9;

    PacketSinkHelper sinkHelper(
        "ns3::UdpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), port));

    ApplicationContainer sinkApp =
        sinkHelper.Install(nodes.Get(0));

    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime));

    // -------------------------------------------------
    // UDP TRAFFIC FROM ALL OTHER NODES TO SINK
    // -------------------------------------------------
    for (uint32_t i = 1; i < numNodes; i++)
    {
        UdpClientHelper client(
            interfaces.GetAddress(0), port);

        client.SetAttribute("MaxPackets",
            UintegerValue(1000000));

        client.SetAttribute("Interval",
            TimeValue(Seconds(1.0 / packetRate)));

        client.SetAttribute("PacketSize",
            UintegerValue(packetSize));

        ApplicationContainer app =
            client.Install(nodes.Get(i));

        app.Start(Seconds(1.0 + i * 0.1));
        app.Stop(Seconds(simTime));
    }

    // -------------------------------------------------
    // FLOW MONITOR
    // -------------------------------------------------
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor =
        flowHelper.InstallAll();

    // -------------------------------------------------
    // NETANIM
    // -------------------------------------------------
    AnimationInterface anim("anim.xml");
    anim.SetMobilityPollInterval(Seconds(1));
    for (uint32_t i = 0; i < numNodes; i++)
    {
        Ptr<MobilityModel> mob =
            nodes.Get(i)->GetObject<MobilityModel>();

        Vector p = mob->GetPosition();

        anim.SetConstantPosition(nodes.Get(i), p.x, p.y);
    }

    // -------------------------------------------------
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    monitor->CheckForLostPackets();
    
    monitor->SerializeToXmlFile("results.xml", true, true);

    // -------------------------------------------------
    // RESULTS
    // -------------------------------------------------
    monitor->CheckForLostPackets();

    map<FlowId, FlowMonitor::FlowStats> stats =
        monitor->GetFlowStats();

    double txPackets = 0;
    double rxPackets = 0;
    double lostPackets = 0;
    double delaySum = 0;

    for (auto &x : stats)
    {
        txPackets += x.second.txPackets;
        rxPackets += x.second.rxPackets;
        lostPackets += x.second.lostPackets;
        delaySum += x.second.delaySum.GetSeconds();
    }

    double pdr = 0;
    if (txPackets > 0)
        pdr = (rxPackets / txPackets) * 100.0;

    double throughput =
        (rxPackets * packetSize * 8.0) /
        simTime / 1000.0;

    cout << "\n========== FINAL RESULTS ==========\n";
    cout << "Nodes         : " << numNodes << endl;
    cout << "TX Packets    : " << txPackets << endl;
    cout << "RX Packets    : " << rxPackets << endl;
    cout << "Lost Packets  : " << lostPackets << endl;
    cout << "PDR (%)       : " << pdr << endl;
    cout << "Throughput    : "
         << throughput << " Kbps" << endl;

    if (rxPackets > 0)
    {
        cout << "Avg Delay     : "
             << delaySum / rxPackets
             << " sec" << endl;
    }

    monitor->SerializeToXmlFile(
        "results.xml", true, true);

    Simulator::Destroy();
    return 0;
}