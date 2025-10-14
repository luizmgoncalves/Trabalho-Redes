/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

// Default Network Topology
//
//       10.1.1.0
// n0 -------------- n1
//    point-to-point
//

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FirstScriptExample");

int
main(int argc, char* argv[])
{
    uint32_t nPackets = 1;
    uint32_t nClients = 1;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nClients", "Numero de clientes", nClients);
    cmd.AddValue("nPackets", "Numero de pacotes enviados pelos clientes", nPackets);

    cmd.Parse(argc, argv);

    // Filtro
    if (nPackets < 0 || nPackets > 5)
    {
        nPackets = 1;
    }
    if (nClients < 0 || nClients > 5){
        nClients = 1;
    }


    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer server;
    server.Create(1);
    NodeContainer clients;
    clients.Create(nClients);

    NodeContainer nodes;

    nodes.Add(server);
    nodes.Add(clients);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces;
    NetDeviceContainer devices;

    for (uint32_t i=0; i < nClients; i++){
        devices = pointToPoint.Install(server.Get(0), clients.Get(i));
        interfaces = address.Assign(devices);
        address.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    UdpEchoServerHelper echoServer(9);
    echoServer.SetAttribute("Port", UintegerValue(15));

    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(1));
    serverApps.Stop(Seconds(20));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 15);
    echoClient.SetAttribute("MaxPackets", UintegerValue(nPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(2)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    for (uint32_t i=0; i<nClients; i++ ){
        Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
        uint32_t rand_n = x->GetInteger() % 5 + 2; // Gerando numero entre 7 e 2

        ApplicationContainer clientApps = echoClient.Install(clients.Get(i));
        clientApps.Start(Seconds(rand_n));
        clientApps.Stop(Seconds(20));
    }
    
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
