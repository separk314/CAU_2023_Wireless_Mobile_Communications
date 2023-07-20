#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/wifi-module.h"
#include "ns3/dsr-module.h"
#include "ns3/aodv-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/dsr-helper.h"
#include <fstream>

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("TermProject");

void PrintCourseChange (std::string context, Ptr<const MobilityModel> model) {
    Vector position = model->GetPosition();
    NS_LOG_UNCOND(context << " x = " << position.x << ", y = " << position.y);
}

int main(int argc, char *argv[]) {
    int mobilityType = 1;   // 0:fast, 1:slow
    int protocol = 2;   // 0:aodv, 1:dsr, 2:dsdv
    uint32_t nCsma = 3;
    uint32_t nWifi = 3;

    CommandLine cmd (__FILE__);
    cmd.AddValue ("protocol", "0:aodv, 1:dsr, 2:dsdv", protocol);
    cmd.AddValue("mobilityType", "Mobility: 0(fast) or 1(slow)", mobilityType);
    cmd.AddValue ("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
    cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
    cmd.Parse (argc, argv);

    LogComponentEnable("TermProject", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer p2pNodes;
    p2pNodes.Create (2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer p2pDevices;
    p2pDevices = pointToPoint.Install (p2pNodes);

    NodeContainer csmaNodes;
    csmaNodes.Add (p2pNodes.Get (1));
    csmaNodes.Create (nCsma);

    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install (csmaNodes);

    // YansWifi 설정
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create (nWifi);
    NodeContainer wifiApNode = p2pNodes.Get (0);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy;
    phy.SetChannel (channel.Create ());

    WifiHelper wifi;
    wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

    // SSID 설정
    WifiMacHelper mac;
    Ssid ssid = Ssid ("ns-3-ssid");
    mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
               
    // YansWifi - Sta: wifi interface를 가지는 단말로서 AP를 통해 데이터를 보냄
    NetDeviceContainer staDevices;
    staDevices = wifi.Install (phy, mac, wifiStaNodes);

    mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

    // YansWifi - AP: Access Point는 공유기 모드로 사용하여 STA가 보낸 데이터를
    // 무선으로 받아 Ethernet 포트를 통해 유선망으로 보낸다.
    NetDeviceContainer apDevices;
    apDevices = wifi.Install (phy, mac, wifiApNode);

    // Mobility setting
    MobilityHelper mobility;

    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

    if (mobilityType == 0) {
        // Node moves fast
        NS_LOG_INFO("Node moves fast: 24m/s (Car)");
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)), "Speed", StringValue("ns3::ConstantRandomVariable[Constant=28]"));
    } else if (mobilityType == 1) {
        // Node moves slow
        NS_LOG_INFO("Node moves slow: 1.4m/s (Human)");
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)), "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.4]"));
    }
    mobility.Install(wifiStaNodes);

    // AP는 움직이면 안 되므로 Constant poisition을 가진다
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiApNode);


    // Internet stack, protocol
    InternetStackHelper stack;

    if (protocol == 0) {
        AodvHelper aodv;
        stack.SetRoutingHelper(aodv);
        NS_LOG_INFO("AODV protocol");
    } else if (protocol == 1) {
        DsrMainHelper dsrMain;
        DsrHelper dsr;
        dsrMain.Install(dsr, p2pNodes);
        dsrMain.Install(dsr, csmaNodes);
        if (apDevices.GetN() > 0) {
            dsrMain.Install(dsr, wifiApNode); 
            dsrMain.Install(dsr, wifiStaNodes);
        }
        NS_LOG_INFO("DSR protocol");
    } else if (protocol == 2) {
        DsdvHelper dsdv;
        stack.SetRoutingHelper(dsdv);
        NS_LOG_INFO("DSDV protocol");

    } else {
        return 0;
    }

    if (protocol != 1) {
        stack.Install (csmaNodes);
        stack.Install (wifiApNode);
        stack.Install (wifiStaNodes);
    }

    Ipv4AddressHelper address;

    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces;
    p2pInterfaces = address.Assign (p2pDevices);

    address.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces;
    csmaInterfaces = address.Assign (csmaDevices);

    address.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiStaInterfaces = address.Assign (staDevices);
    Ipv4InterfaceContainer wifiApInterface = address.Assign (apDevices);

    UdpEchoServerHelper echoServer (9);

    ApplicationContainer serverApps = echoServer.Install (csmaNodes.Get (nCsma));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));

    UdpEchoClientHelper echoClient (csmaInterfaces.GetAddress (nCsma), 9);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

    // Config::Connet 함수를 통해 Callback 함수와 Trace source를 연결
    std::ostringstream path;
    path << "/NodeList/" << wifiStaNodes.Get(nWifi-1)->GetId() << "/$ns3::MobilityModel/CourseChange";
    Config::Connect(path.str(), MakeCallback(&PrintCourseChange));

    ApplicationContainer clientApps = echoClient.Install (wifiStaNodes.Get (nWifi - 1));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));

    // 성능 측정을 위한 FlowMonitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    Simulator::Stop (Seconds (20.0));

    NS_LOG_INFO("Starting Simulation");
    Simulator::Run ();
    NS_LOG_INFO("Ended Simulation");

    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    double endTime = 0;

    for(std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); i++) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        NS_LOG_INFO("Flow " << i->first << " (" << t.sourceAddress << "->" << t.destinationAddress << ")\n");
        NS_LOG_INFO("  Lost Packets: " << i->second.lostPackets << "\n");
        NS_LOG_INFO("  Delay Sum: " << i->second.delaySum.GetSeconds() << " s\n");
        NS_LOG_INFO("  Default Delay: " << i->second.delaySum.GetSeconds()/i->second.rxPackets << " s\n");
        NS_LOG_INFO("  throughput : " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1000000 << " Mbps" << std::endl);

        // std::cout << "  Tx Bytes: " << i->second.txBytes << "\n";
        // std::cout << "  Rx Bytes: " << i->second.rxBytes << "\n";

        if (endTime < i->second.timeLastRxPacket.GetSeconds()) {
            endTime = i->second.timeLastRxPacket.GetSeconds();
        }
    }

    Simulator::Destroy();
    return 0;
}