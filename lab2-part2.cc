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
#include <vector>

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
 * In flight tracer.
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
TraceCwnd(std::string cwnd_tr_file_name, uint32_t nodeId, uint32_t socketIndex)
{
    AsciiTraceHelper ascii;
    cWndStream[nodeId] = ascii.CreateFileStream(cwnd_tr_file_name);
    Config::Connect("/NodeList/" + std::to_string(nodeId) +
                        "/$ns3::TcpL4Protocol/SocketList/" + std::to_string(socketIndex) + "/CongestionWindow",
                    MakeCallback(&CwndTracer));
}

/**
 * Slow start threshold trace connection.
 *
 * @param ssthresh_tr_file_name Slow start threshold trace file name.
 * @param nodeId Node ID.
 */
static void
TraceSsThresh(std::string ssthresh_tr_file_name, uint32_t nodeId, uint32_t socketIndex)
{
    AsciiTraceHelper ascii;
    ssThreshStream[nodeId] = ascii.CreateFileStream(ssthresh_tr_file_name);
    Config::Connect("/NodeList/" + std::to_string(nodeId) +
                        "/$ns3::TcpL4Protocol/SocketList/" + std::to_string(socketIndex) + "/SlowStartThreshold",
                    MakeCallback(&SsThreshTracer));
}

/**
 * RTT trace connection.
 *
 * @param rtt_tr_file_name RTT trace file name.
 * @param nodeId Node ID.
 */
static void
TraceRtt(std::string rtt_tr_file_name, uint32_t nodeId, uint32_t socketIndex)
{
    AsciiTraceHelper ascii;
    rttStream[nodeId] = ascii.CreateFileStream(rtt_tr_file_name);
    Config::Connect("/NodeList/" + std::to_string(nodeId) + "/$ns3::TcpL4Protocol/SocketList/" + std::to_string(socketIndex) + "/RTT",
                    MakeCallback(&RttTracer));
}

/**
 * RTO trace connection.
 *
 * @param rto_tr_file_name RTO trace file name.
 * @param nodeId Node ID.
 */
static void
TraceRto(std::string rto_tr_file_name, uint32_t nodeId, uint32_t socketIndex)
{
    AsciiTraceHelper ascii;
    rtoStream[nodeId] = ascii.CreateFileStream(rto_tr_file_name);
    Config::Connect("/NodeList/" + std::to_string(nodeId) + "/$ns3::TcpL4Protocol/SocketList/" + std::to_string(socketIndex) + "/RTO",
                    MakeCallback(&RtoTracer));
}

/**
 * Next TX trace connection.
 *
 * @param next_tx_seq_file_name Next TX trace file name.
 * @param nodeId Node ID.
 */
static void
TraceNextTx(std::string& next_tx_seq_file_name, uint32_t nodeId, uint32_t socketIndex)
{
    AsciiTraceHelper ascii;
    nextTxStream[nodeId] = ascii.CreateFileStream(next_tx_seq_file_name);
    Config::Connect("/NodeList/" + std::to_string(nodeId) +
                        "/$ns3::TcpL4Protocol/SocketList/" + std::to_string(socketIndex) + "/NextTxSequence",
                    MakeCallback(&NextTxTracer));
}

/**
 * In flight trace connection.
 *
 * @param in_flight_file_name In flight trace file name.
 * @param nodeId Node ID.
 */
static void
TraceInFlight(std::string& in_flight_file_name, uint32_t nodeId, uint32_t socketIndex)
{
    AsciiTraceHelper ascii;
    inFlightStream[nodeId] = ascii.CreateFileStream(in_flight_file_name);
    Config::Connect("/NodeList/" + std::to_string(nodeId) +
                        "/$ns3::TcpL4Protocol/SocketList/" + std::to_string(socketIndex) + "/BytesInFlight",
                    MakeCallback(&InFlightTracer));
}

/**
 * Next RX trace connection.
 *
 * @param next_rx_seq_file_name Next RX trace file name.
 * @param nodeId Node ID.
 */
static void
TraceNextRx(std::string& next_rx_seq_file_name, uint32_t nodeId, uint32_t socketIndex)
{
    AsciiTraceHelper ascii;
    nextRxStream[nodeId] = ascii.CreateFileStream(next_rx_seq_file_name);
    Config::Connect("/NodeList/" + std::to_string(nodeId) +
                        "/$ns3::TcpL4Protocol/SocketList/" + std::to_string(socketIndex) + "/RxBuffer/NextRxSequence",
                    MakeCallback(&NextRxTracer));
}

int
main(int argc, char* argv[])
{
    
    std::string dataRate = "1Mbps"; 
    std::string delay = "20ms";     
    double errorRate = 0.00001;
    uint32_t nFlows = 4; 
    std::string transport_prot = "TcpCubic";
    uint32_t seed = 123456789; 

    CommandLine cmd(__FILE__);
    cmd.AddValue("transport_prot", "Transport protocol to use: TcpCubic or TcpNewReno", transport_prot);
    cmd.AddValue("errorRate", "Bottleneck link error rate", errorRate);
    cmd.AddValue("delay", "Bottleneck delay", delay);
    cmd.AddValue("dataRate", "Bottleneck data Rate", dataRate);
    cmd.AddValue("nFlows", "Number of flows (must be even)", nFlows);
    cmd.AddValue("seed", "Seed for simulation", seed);
    cmd.Parse(argc, argv);

    
    if (nFlows % 2 != 0 || nFlows < 0)
    {
        NS_FATAL_ERROR("nFlows precisa ser um número par maior que 0.");
    }
    uint32_t flows_per_dest = nFlows / 2;

    std::string prefix_file_name = "lab2-part2-" + transport_prot + "-" + std::to_string(nFlows);
    uint64_t data_mbytes = 0;
    uint32_t mtu_bytes = 400;
    double duration = 20.0;
    double start_time = 1.0; 
    double stop_time = start_time + duration;

    bool tracing = true;
    bool flow_monitor = true;
    
    
    SeedManager::SetSeed(seed);
    SeedManager::SetRun(1); 

    
    if (transport_prot.compare("TcpCubic") == 0){
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpCubic"));
    } else if (transport_prot.compare("TcpNewReno") == 0){
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
    } else {
        NS_LOG_ERROR("Protocolo de transporte inválido.");
        return 1;
    }

    
    Header* temp_header = new Ipv4Header();
    uint32_t ip_header = temp_header->GetSerializedSize();
    delete temp_header;
    temp_header = new TcpHeader();
    uint32_t tcp_header = temp_header->GetSerializedSize();
    delete temp_header;
    uint32_t tcp_adu_size = mtu_bytes - 20 - (ip_header + tcp_header);

    
    NodeContainer nodes;
    nodes.Create(5);
    Ptr<Node> fonte = nodes.Get(0);
    Ptr<Node> n1 = nodes.Get(1); 
    Ptr<Node> n2 = nodes.Get(2); 
    Ptr<Node> dest1 = nodes.Get(3); 
    Ptr<Node> dest2 = nodes.Get(4); 

    NodeContainer link_s_n1(fonte, n1);
    NodeContainer link_n1_n2(n1, n2);
    NodeContainer link_n2_d1(n2, dest1);
    NodeContainer link_n2_d2(n2, dest2);
    
    
    PointToPointHelper p2p_fast;
    p2p_fast.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p_fast.SetChannelAttribute("Delay", StringValue("0.01ms"));
    
    NetDeviceContainer dev_s_n1 = p2p_fast.Install(link_s_n1);

    
    Ptr<RateErrorModel> error_model = CreateObject<RateErrorModel>();
    error_model->SetAttribute("ErrorRate", DoubleValue(errorRate));

    PointToPointHelper p2p_bottleneck;
    p2p_bottleneck.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p_bottleneck.SetChannelAttribute("Delay", StringValue(delay));
    p2p_bottleneck.SetDeviceAttribute("ReceiveErrorModel", PointerValue(error_model));
    
    NetDeviceContainer dev_n1_n2 = p2p_bottleneck.Install(link_n1_n2);
    
    
    NetDeviceContainer dev_n2_d1 = p2p_fast.Install(link_n2_d1);

    
    PointToPointHelper p2p_d2_slow;
    p2p_d2_slow.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p_d2_slow.SetChannelAttribute("Delay", StringValue("50ms"));
    NetDeviceContainer dev_n2_d2 = p2p_d2_slow.Install(link_n2_d2);
    
    
    InternetStackHelper stack;
    stack.Install(nodes);
    
    Ipv4AddressHelper address;

    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer i_s_n1 = address.Assign(dev_s_n1);

    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n1_n2 = address.Assign(dev_n1_n2);

    address.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n2_d1 = address.Assign(dev_n2_d1);

    address.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n2_d2 = address.Assign(dev_n2_d2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 8080;
    
    ApplicationContainer sink_apps_dest1;
    ApplicationContainer sink_apps_dest2;

    for (uint32_t i = 0; i < flows_per_dest; i++)
    {
        Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port + i));
        PacketSinkHelper sink("ns3::TcpSocketFactory", serverAddress);
        sink_apps_dest1.Add(sink.Install(dest1)); 
    }

    for (uint32_t i = 0; i < flows_per_dest; i++)
    {
        Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port + flows_per_dest + i));
        PacketSinkHelper sink("ns3::TcpSocketFactory", serverAddress);
        sink_apps_dest2.Add(sink.Install(dest2)); 
    }

    sink_apps_dest1.Start(Seconds(0.0));
    sink_apps_dest1.Stop(Seconds(stop_time));
    sink_apps_dest2.Start(Seconds(0.0));
    sink_apps_dest2.Stop(Seconds(stop_time));


    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(tcp_adu_size));
    
    for (uint32_t i = 0; i < flows_per_dest; i++)
    {
        AddressValue remoteAddress(InetSocketAddress(i_n2_d1.GetAddress(1, 0), port + i));
        BulkSendHelper ftp("ns3::TcpSocketFactory", Address());
        ftp.SetAttribute("Remote", remoteAddress);
        ftp.SetAttribute("SendSize", UintegerValue(tcp_adu_size));
        ftp.SetAttribute("MaxBytes", UintegerValue(data_mbytes * 1000000));

        ApplicationContainer fonteApp = ftp.Install(fonte);
        fonteApp.Start(Seconds(start_time)); 
        fonteApp.Stop(Seconds(stop_time));
    }

    
    for (uint32_t i = 0; i < flows_per_dest; i++)
    {
        AddressValue remoteAddress(InetSocketAddress(i_n2_d2.GetAddress(1, 0), port + flows_per_dest + i));
        BulkSendHelper ftp("ns3::TcpSocketFactory", Address());
        ftp.SetAttribute("Remote", remoteAddress);
        ftp.SetAttribute("SendSize", UintegerValue(tcp_adu_size));
        ftp.SetAttribute("MaxBytes", UintegerValue(data_mbytes * 1000000));

        ApplicationContainer fonteApp = ftp.Install(fonte);
        fonteApp.Start(Seconds(start_time)); 
        fonteApp.Stop(Seconds(stop_time));
    }

    
    if (tracing)
    {
        firstCwnd[0] = true;
        firstSshThr[0] = true;
        firstRtt[0] = true;
        firstRto[0] = true;

        Simulator::Schedule(Seconds(start_time + 0.00001),
                            &TraceCwnd,
                            prefix_file_name + "-n0-cwnd.data",
                            0, 0);
        Simulator::Schedule(Seconds(start_time + 0.00001),
                            &TraceRtt,
                            prefix_file_name + "-n0-rtt.data",
                            0, 0);
    }
    
    
    FlowMonitorHelper flowHelper;
    if (flow_monitor)
    {
        flowHelper.Install(nodes);
    }

    Simulator::Stop(Seconds(stop_time));
    Simulator::Run();

    
    double flowDuration = duration; 
    uint64_t totalRxBytesDest1 = 0;
    uint64_t totalRxBytesDest2 = 0;

    
    for (uint32_t i = 0; i < flows_per_dest; ++i)
    {
        Ptr<Application> genericApp = sink_apps_dest1.Get(i);
        Ptr<PacketSink> sinkApp = DynamicCast<PacketSink>(genericApp);
        if (sinkApp)
        {
            totalRxBytesDest1 += sinkApp->GetTotalRx();
        }
    }

    
    for (uint32_t i = 0; i < flows_per_dest; ++i)
    {
        Ptr<Application> genericApp = sink_apps_dest2.Get(i);
        Ptr<PacketSink> sinkApp = DynamicCast<PacketSink>(genericApp);
        if (sinkApp)
        {
            totalRxBytesDest2 += sinkApp->GetTotalRx();
        }
    }
    
    double aggregateGoodputDest1 = (totalRxBytesDest1 * 8.0) / flowDuration;
    double aggregateGoodputDest2 = (totalRxBytesDest2 * 8.0) / flowDuration;
    
    double avgGoodputDest1 = aggregateGoodputDest1 / flows_per_dest;
    double avgGoodputDest2 = aggregateGoodputDest2 / flows_per_dest;
    
    double totalAggregateGoodput = aggregateGoodputDest1 + aggregateGoodputDest2;

    std::cout << "\n--- Resultados de Goodput (Parte 2) ---" << std::endl;
    std::cout << "Protocol: " << transport_prot << std::endl;
    std::cout << "Total Flows: " << nFlows << " (Flows/Dest: " << flows_per_dest << ")" << std::endl;
    std::cout << "Flow Duration: " << flowDuration << " seconds" << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    
    std::cout << "Dest 1 (Fast RTT) | Total Rx Bytes: " << totalRxBytesDest1 << std::endl;
    std::cout << "Dest 1 (Fast RTT) | Aggregate Goodput: " << aggregateGoodputDest1 << " bps" << std::endl;
    std::cout << "Dest 1 (Fast RTT) | Average Per-Flow Goodput: " << avgGoodputDest1 << " bps" << std::endl;
    
    std::cout << "------------------------------------------" << std::endl;
    
    std::cout << "Dest 2 (Slow RTT) | Total Rx Bytes: " << totalRxBytesDest2 << std::endl;
    std::cout << "Dest 2 (Slow RTT) | Aggregate Goodput: " << aggregateGoodputDest2 << " bps" << std::endl;
    std::cout << "Dest 2 (Slow RTT) | Average Per-Flow Goodput: " << avgGoodputDest2 << " bps" << std::endl;
    
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "Total Aggregate Goodput: " << totalAggregateGoodput << " bps" << std::endl;


    if (flow_monitor)
    {
        flowHelper.SerializeToXmlFile(prefix_file_name + ".flowmonitor", true, true);
    }

    Simulator::Destroy();
    return 0;
}