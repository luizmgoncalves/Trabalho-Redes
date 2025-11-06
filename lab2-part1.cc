/*
 * Copyright (c) 2013 ResiliNets, ITTC, University of Kansas
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Justin P. Rohrer, Truc Anh N. Nguyen <annguyen@ittc.ku.edu>, Siddharth Gangadhar
 * <siddharth@ittc.ku.edu>
 *
 * James P.G. Sterbenz <jpgs@ittc.ku.edu>, director
 * ResiliNets Research Group  https://resilinets.org/
 * Information and Telecommunication Technology Center (ITTC)
 * and Department of Electrical Engineering and Computer Science
 * The University of Kansas Lawrence, KS USA.
 *
 * Work supported in part by NSF FIND (Future Internet Design) Program
 * under grant CNS-0626918 (Postmodern Internet Architecture),
 * NSF grant CNS-1050226 (Multilayer Network Resilience Analysis and Experimentation on GENI),
 * US Department of Defense (DoD), and ITTC at The University of Kansas.
 *
 * "TCP Westwood(+) Protocol Implementation in ns-3"
 * Siddharth Gangadhar, Trúc Anh Ngọc Nguyễn , Greeshma Umapathi, and James P.G. Sterbenz,
 * ICST SIMUTools Workshop on ns-3 (WNS3), Cannes, France, March 2013
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/enum.h"
#include "ns3/error-model.h"
#include "ns3/event-id.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-header.h"
#include "ns3/traffic-control-module.h"
#include "ns3/udp-header.h"

#include <fstream>
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpVariantsComparison");

static std::map<uint32_t, bool> firstCwnd;                      //!< First congestion window.
static std::map<uint32_t, bool> firstSshThr;                    //!< First SlowStart threshold.
static std::map<uint32_t, bool> firstRtt;                       //!< First RTT.
static std::map<uint32_t, bool> firstRto;                       //!< First RTO.
static std::map<uint32_t, Ptr<OutputStreamWrapper>> cWndStream; //!< Congstion window output stream.
static std::map<uint32_t, Ptr<OutputStreamWrapper>>
    ssThreshStream; //!< SlowStart threshold output stream.
static std::map<uint32_t, Ptr<OutputStreamWrapper>> rttStream;      //!< RTT output stream.
static std::map<uint32_t, Ptr<OutputStreamWrapper>> rtoStream;      //!< RTO output stream.
static std::map<uint32_t, Ptr<OutputStreamWrapper>> nextTxStream;   //!< Next TX output stream.
static std::map<uint32_t, Ptr<OutputStreamWrapper>> nextRxStream;   //!< Next RX output stream.
static std::map<uint32_t, Ptr<OutputStreamWrapper>> inFlightStream; //!< In flight output stream.
static std::map<uint32_t, uint32_t> cWndValue;                      //!< congestion window value.
static std::map<uint32_t, uint32_t> ssThreshValue;                  //!< SlowStart threshold value.

/**
 * Get the Node Id From Context.
 *
 * @param context The context.
 * @return the node ID.
 */
static uint32_t
GetNodeIdFromContext(std::string context)
{
    const std::size_t n1 = context.find_first_of('/', 1);
    const std::size_t n2 = context.find_first_of('/', n1 + 1);
    return std::stoul(context.substr(n1 + 1, n2 - n1 - 1));
}

/**
 * Congestion window tracer.
 *
 * @param context The context.
 * @param oldval Old value.
 * @param newval New value.
 */
static void
CwndTracer(std::string context, uint32_t oldval, uint32_t newval)
{
    uint32_t nodeId = GetNodeIdFromContext(context);

    if (firstCwnd[nodeId])
    {
        *cWndStream[nodeId]->GetStream() << "0.0 " << oldval << std::endl;
        firstCwnd[nodeId] = false;
    }
    *cWndStream[nodeId]->GetStream() << Simulator::Now().GetSeconds() << " " << newval << std::endl;
    cWndValue[nodeId] = newval;

    if (!firstSshThr[nodeId])
    {
        *ssThreshStream[nodeId]->GetStream()
            << Simulator::Now().GetSeconds() << " " << ssThreshValue[nodeId] << std::endl;
    }
}

/**
 * Slow start threshold tracer.
 *
 * @param context The context.
 * @param oldval Old value.
 * @param newval New value.
 */
static void
SsThreshTracer(std::string context, uint32_t oldval, uint32_t newval)
{
    uint32_t nodeId = GetNodeIdFromContext(context);

    if (firstSshThr[nodeId])
    {
        *ssThreshStream[nodeId]->GetStream() << "0.0 " << oldval << std::endl;
        firstSshThr[nodeId] = false;
    }
    *ssThreshStream[nodeId]->GetStream()
        << Simulator::Now().GetSeconds() << " " << newval << std::endl;
    ssThreshValue[nodeId] = newval;

    if (!firstCwnd[nodeId])
    {
        *cWndStream[nodeId]->GetStream()
            << Simulator::Now().GetSeconds() << " " << cWndValue[nodeId] << std::endl;
    }
}

/**
 * RTT tracer.
 *
 * @param context The context.
 * @param oldval Old value.
 * @param newval New value.
 */
static void
RttTracer(std::string context, Time oldval, Time newval)
{
    uint32_t nodeId = GetNodeIdFromContext(context);

    if (firstRtt[nodeId])
    {
        *rttStream[nodeId]->GetStream() << "0.0 " << oldval.GetSeconds() << std::endl;
        firstRtt[nodeId] = false;
    }
    *rttStream[nodeId]->GetStream()
        << Simulator::Now().GetSeconds() << " " << newval.GetSeconds() << std::endl;
}

/**
 * RTO tracer.
 *
 * @param context The context.
 * @param oldval Old value.
 * @param newval New value.
 */
static void
RtoTracer(std::string context, Time oldval, Time newval)
{
    uint32_t nodeId = GetNodeIdFromContext(context);

    if (firstRto[nodeId])
    {
        *rtoStream[nodeId]->GetStream() << "0.0 " << oldval.GetSeconds() << std::endl;
        firstRto[nodeId] = false;
    }
    *rtoStream[nodeId]->GetStream()
        << Simulator::Now().GetSeconds() << " " << newval.GetSeconds() << std::endl;
}

/**
 * Next TX tracer.
 *
 * @param context The context.
 * @param old Old sequence number.
 * @param nextTx Next sequence number.
 */
static void
NextTxTracer(std::string context, SequenceNumber32 old [[maybe_unused]], SequenceNumber32 nextTx)
{
    uint32_t nodeId = GetNodeIdFromContext(context);

    *nextTxStream[nodeId]->GetStream()
        << Simulator::Now().GetSeconds() << " " << nextTx << std::endl;
}

/**
 * In-flight tracer.
 *
 * @param context The context.
 * @param old Old value.
 * @param inFlight In flight value.
 */
static void
InFlightTracer(std::string context, uint32_t old [[maybe_unused]], uint32_t inFlight)
{
    uint32_t nodeId = GetNodeIdFromContext(context);

    *inFlightStream[nodeId]->GetStream()
        << Simulator::Now().GetSeconds() << " " << inFlight << std::endl;
}

/**
 * Next RX tracer.
 *
 * @param context The context.
 * @param old Old sequence number.
 * @param nextRx Next sequence number.
 */
static void
NextRxTracer(std::string context, SequenceNumber32 old [[maybe_unused]], SequenceNumber32 nextRx)
{
    uint32_t nodeId = GetNodeIdFromContext(context);

    *nextRxStream[nodeId]->GetStream()
        << Simulator::Now().GetSeconds() << " " << nextRx << std::endl;
}

/**
 * Congestion window trace connection.
 *
 * @param cwnd_tr_file_name Congestion window trace file name.
 * @param nodeId Node ID.
 */
static void
TraceCwnd(std::string cwnd_tr_file_name, uint32_t nodeId)
{
    AsciiTraceHelper ascii;
    cWndStream[nodeId] = ascii.CreateFileStream(cwnd_tr_file_name);
    Config::Connect("/NodeList/" + std::to_string(nodeId) +
                        "/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
                    MakeCallback(&CwndTracer));
}

/**
 * Slow start threshold trace connection.
 *
 * @param ssthresh_tr_file_name Slow start threshold trace file name.
 * @param nodeId Node ID.
 */
static void
TraceSsThresh(std::string ssthresh_tr_file_name, uint32_t nodeId)
{
    AsciiTraceHelper ascii;
    ssThreshStream[nodeId] = ascii.CreateFileStream(ssthresh_tr_file_name);
    Config::Connect("/NodeList/" + std::to_string(nodeId) +
                        "/$ns3::TcpL4Protocol/SocketList/0/SlowStartThreshold",
                    MakeCallback(&SsThreshTracer));
}

/**
 * RTT trace connection.
 *
 * @param rtt_tr_file_name RTT trace file name.
 * @param nodeId Node ID.
 */
static void
TraceRtt(std::string rtt_tr_file_name, uint32_t nodeId)
{
    AsciiTraceHelper ascii;
    rttStream[nodeId] = ascii.CreateFileStream(rtt_tr_file_name);
    Config::Connect("/NodeList/" + std::to_string(nodeId) + "/$ns3::TcpL4Protocol/SocketList/0/RTT",
                    MakeCallback(&RttTracer));
}

/**
 * RTO trace connection.
 *
 * @param rto_tr_file_name RTO trace file name.
 * @param nodeId Node ID.
 */
static void
TraceRto(std::string rto_tr_file_name, uint32_t nodeId)
{
    AsciiTraceHelper ascii;
    rtoStream[nodeId] = ascii.CreateFileStream(rto_tr_file_name);
    Config::Connect("/NodeList/" + std::to_string(nodeId) + "/$ns3::TcpL4Protocol/SocketList/0/RTO",
                    MakeCallback(&RtoTracer));
}

/**
 * Next TX trace connection.
 *
 * @param next_tx_seq_file_name Next TX trace file name.
 * @param nodeId Node ID.
 */
static void
TraceNextTx(std::string& next_tx_seq_file_name, uint32_t nodeId)
{
    AsciiTraceHelper ascii;
    nextTxStream[nodeId] = ascii.CreateFileStream(next_tx_seq_file_name);
    Config::Connect("/NodeList/" + std::to_string(nodeId) +
                        "/$ns3::TcpL4Protocol/SocketList/0/NextTxSequence",
                    MakeCallback(&NextTxTracer));
}

/**
 * In flight trace connection.
 *
 * @param in_flight_file_name In flight trace file name.
 * @param nodeId Node ID.
 */
static void
TraceInFlight(std::string& in_flight_file_name, uint32_t nodeId)
{
    AsciiTraceHelper ascii;
    inFlightStream[nodeId] = ascii.CreateFileStream(in_flight_file_name);
    Config::Connect("/NodeList/" + std::to_string(nodeId) +
                        "/$ns3::TcpL4Protocol/SocketList/0/BytesInFlight",
                    MakeCallback(&InFlightTracer));
}

/**
 * Next RX trace connection.
 *
 * @param next_rx_seq_file_name Next RX trace file name.
 * @param nodeId Node ID.
 */
static void
TraceNextRx(std::string& next_rx_seq_file_name, uint32_t nodeId)
{
    AsciiTraceHelper ascii;
    nextRxStream[nodeId] = ascii.CreateFileStream(next_rx_seq_file_name);
    Config::Connect("/NodeList/" + std::to_string(nodeId) +
                        "/$ns3::TcpL4Protocol/SocketList/0/RxBuffer/NextRxSequence",
                    MakeCallback(&NextRxTracer));
}

int
main(int argc, char* argv[])
{
    std::string dataRate = "10Mbps";
    std::string delay = "20ms";
    double errorRate = 0.00001;
    uint32_t nFlows = 1;
    std::string transport_prot = "TcpCubic";
    uint32_t seed = 1;

    CommandLine cmd(__FILE__);
    cmd.AddValue("transport_prot",
                 "Transport protocol to use: TcpNewReno, TcpLinuxReno, "
                 "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                 "TcpBic, TcpYeah, TcpIllinois, TcpWestwoodPlus, TcpLedbat, "
                 "TcpLp, TcpDctcp, TcpCubic, TcpBbr",
                 transport_prot);
    cmd.AddValue("errorRate", "Packet error rate", errorRate);
    cmd.AddValue("delay", "Bottleneck delay", delay);
    cmd.AddValue("dataRate", "Data Rate", dataRate);
    cmd.AddValue("nFlows", "Number of flows", nFlows);
    cmd.AddValue("seed", "Seed for simulation", seed);
    cmd.Parse(argc, argv);

    std::string bandwidth = "2Mbps";
    std::string access_bandwidth = "10Mbps";
    std::string access_delay = "45ms";
    bool tracing = true;
    std::string prefix_file_name = "scratch/resultados/Congestion_Control";
    uint64_t data_mbytes = 0;
    uint32_t mtu_bytes = 400;
    double duration = 20.0;
    uint32_t run = 0;
    bool flow_monitor = true;
    bool pcap = false;
    std::string queue_disc_type = "ns3::PfifoFastQueueDisc";
    std::string recovery = "ns3::TcpClassicRecovery";

    SeedManager::SetSeed(seed);
    SeedManager::SetRun(run);

    // User may find it convenient to enable logging
    // LogComponentEnable("TcpVariantsComparison", LOG_LEVEL_ALL);
    // LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("PfifoFastQueueDisc", LOG_LEVEL_ALL);

    // Calculate the ADU size
    Header* temp_header = new Ipv4Header();
    uint32_t ip_header = temp_header->GetSerializedSize();
    NS_LOG_LOGIC("IP Header size is: " << ip_header);
    delete temp_header;
    temp_header = new TcpHeader();
    uint32_t tcp_header = temp_header->GetSerializedSize();
    NS_LOG_LOGIC("TCP Header size is: " << tcp_header);
    delete temp_header;
    uint32_t tcp_adu_size = mtu_bytes - 20 - (ip_header + tcp_header);
    NS_LOG_LOGIC("TCP ADU size is: " << tcp_adu_size);

    // Set the simulation start and stop time
    double start_time = 1;
    double stop_time = start_time + duration;


    if (transport_prot.compare("TcpCubic") == 0){
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpCubic"));
    } else if (transport_prot.compare("TcpNewReno") == 0){
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
    } else {
        NS_LOG_ERROR("Protocolo passado incorreto.");
    }

    // Criando nós
    NodeContainer todos, fonte_no2, no2_no3, no3_destino;
    todos.Create(4);
    fonte_no2.Add(todos.Get(0));
    fonte_no2.Add(todos.Get(1));

    no2_no3.Add(todos.Get(1));
    no2_no3.Add(todos.Get(2));

    no3_destino.Add(todos.Get(2));
    no3_destino.Add(todos.Get(3));

    Ptr<RateErrorModel> error_model = CreateObject<RateErrorModel>();
    error_model->SetAttribute("ErrorRate", DoubleValue(errorRate));

    PointToPointHelper links_normais;
    links_normais.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    links_normais.SetChannelAttribute("Delay", StringValue("0.01ms"));

    NetDeviceContainer dev0_dev1 = links_normais.Install(fonte_no2);
    NetDeviceContainer dev2_dev3 = links_normais.Install(no3_destino);

    PointToPointHelper link_bottleneck;
    link_bottleneck.SetDeviceAttribute("DataRate", StringValue(dataRate));
    link_bottleneck.SetChannelAttribute("Delay", StringValue("100ms"));
    link_bottleneck.SetDeviceAttribute("ReceiveErrorModel", PointerValue(error_model));

    NetDeviceContainer bottleneck_dev = link_bottleneck.Install(no2_no3);

    InternetStackHelper stack;
    stack.InstallAll(); // Possivel troca stack.Install(nodes)


    TrafficControlHelper tchPfifo;
    tchPfifo.SetRootQueueDisc("ns3::PfifoFastQueueDisc");

    TrafficControlHelper tchCoDel;
    tchCoDel.SetRootQueueDisc("ns3::CoDelQueueDisc");


    // Configurando IPs de cada ligação p2p
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer i01 = address.Assign(dev0_dev1);

    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ibtl = address.Assign(bottleneck_dev);

    address.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i23 = address.Assign(dev2_dev3);

    // COnfigura servidor para responder da porta 8080 em diante
    uint16_t port = 8080;
    for (uint32_t i = 0; i < nFlows; i++)
    {
        Address enderecos_servidor(InetSocketAddress(Ipv4Address::GetAny(), port+i));
        PacketSinkHelper servidor("ns3::TcpSocketFactory", enderecos_servidor); 
        ApplicationContainer app_servidor = servidor.Install(todos.Get(3)); 
        app_servidor.Start(Seconds(0.0));
        app_servidor.Stop(Seconds(stop_time));
    }

    NS_LOG_INFO("Initialize Global Routing.");
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configura aplicativos cliente para requisitar na porta 8080 em diante do servidor
    port = 8080;
    for (uint32_t i = 0; i < nFlows; i++)
    {
        AddressValue remoteAddress(InetSocketAddress(i23.GetAddress(1, 0), port+i));
        Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(tcp_adu_size));
        BulkSendHelper ftp("ns3::TcpSocketFactory", Address());
        ftp.SetAttribute("Remote", remoteAddress);
        ftp.SetAttribute("SendSize", UintegerValue(tcp_adu_size));
        ftp.SetAttribute("MaxBytes", UintegerValue(data_mbytes * 1000000));

        ApplicationContainer sourceApp = ftp.Install(todos.Get(0));
        sourceApp.Start(Seconds(0.0));
        sourceApp.Stop(Seconds(stop_time));
    }

    // Set up tracing if enabled
    if (tracing)
    {
        std::ofstream ascii;
        Ptr<OutputStreamWrapper> ascii_wrap;
        ascii.open(prefix_file_name + "-ascii");
        ascii_wrap = new OutputStreamWrapper(prefix_file_name + "-ascii", std::ios::out);
        stack.EnableAsciiIpv4All(ascii_wrap);

        for (uint16_t index = 0; index < nFlows; index++)
        {
            std::string flowString;
            if (nFlows > 1)
            {
                flowString = "-flow" + std::to_string(index);
            }

            firstCwnd[0] = true;
            firstSshThr[0] = true;
            firstRtt[0] = true;
            firstRto[0] = true;

            firstCwnd[3] = true;
            firstSshThr[3] = true;
            firstRtt[3] = true;
            firstRto[3] = true;

            Simulator::Schedule(Seconds(start_time + 0.00001),
                                &TraceCwnd,
                                prefix_file_name + flowString + "-cwnd.data",
                                0);
            Simulator::Schedule(Seconds(start_time + 0.00001),
                                &TraceSsThresh,
                                prefix_file_name + flowString + "-ssth.data",
                                0);
            Simulator::Schedule(Seconds(start_time + 0.00001),
                                &TraceRtt,
                                prefix_file_name + flowString + "-rtt.data",
                                0);
            Simulator::Schedule(Seconds(start_time + 0.00001),
                                &TraceRto,
                                prefix_file_name + flowString + "-rto.data",
                                0);
            Simulator::Schedule(Seconds(start_time + 0.00001),
                                &TraceNextTx,
                                prefix_file_name + flowString + "-next-tx.data",
                                0);
            Simulator::Schedule(Seconds(start_time + 0.00001),
                                &TraceInFlight,
                                prefix_file_name + flowString + "-inflight.data",
                                0);
            Simulator::Schedule(Seconds(start_time + 0.1),
                                &TraceNextRx,
                                prefix_file_name + flowString + "-next-rx.data",
                                3);
        }
    }

    if (pcap)
    {
        links_normais.EnablePcapAll(prefix_file_name, true);
        link_bottleneck.EnablePcapAll(prefix_file_name, true);
    }

    // Flow monitor
    FlowMonitorHelper flowHelper;
    if (flow_monitor)
    {
        flowHelper.InstallAll();
    }

    Simulator::Stop(Seconds(stop_time));
    Simulator::Run();

    double flowDuration = duration - start_time; 
    uint64_t totalRxBytes = 0; 

    Ptr<Node> destNode = todos.Get(3);
    
    std::cout << "\n--- Resultados de Goodput por Fluxo ---" << std::endl;

    for (uint32_t flowIndex = 0; flowIndex < nFlows; ++flowIndex)
    {
        Ptr<Application> genericApp = destNode->GetApplication(flowIndex);
        
        Ptr<PacketSink> sinkApp = DynamicCast<PacketSink>(genericApp);

        if (sinkApp)
        {
            uint64_t currentRxBytes = sinkApp->GetTotalRx();
            totalRxBytes += currentRxBytes;
            
            double goodputBps = (currentRxBytes * 8.0) / flowDuration; 
            
            std::cout << "Flow numero " << flowIndex + 1 
                      << " | Goodput: " << goodputBps << " bps" 
                      << " (Recebido: " << currentRxBytes << " bytes)" 
                      << std::endl;
        }
        else
        {
            std::cerr << "Erro: Aplicação no índice " << flowIndex << " não é do tipo PacketSink." << std::endl;
        }
    }
    

    double aggregateGoodput = (totalRxBytes * 8) / flowDuration;
    std::cout << "---" << std::endl;
    std::cout << "Goodput Agregado Total: " << aggregateGoodput << " bps" << std::endl;

    if (flow_monitor)
    {
        flowHelper.SerializeToXmlFile(prefix_file_name + ".flowmonitor", true, true);
    }

    Simulator::Destroy();
    return 0;
}
