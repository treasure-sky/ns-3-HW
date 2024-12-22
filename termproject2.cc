/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/qos-txop.h"
#include "ns3/ssid.h"
#include "ns3/wifi-mac.h"
#include "ns3/yans-wifi-helper.h"

// Default Network Topology
//
//   Wifi 10.1.3.0
//
//  *    *    *    *
//  |    |    |    |    10.1.1.0
// wn1  wn2  wn3  AP --------------- R   fn1  fn2  fn3
//                   point-to-point  |    |    |    |
//                                   ================
//                                     LAN 10.1.2.0

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TermProject#2");

int
main(int argc, char* argv[])
{
    bool verbose = true;
    bool tracing = false;

    // fn 개수
    const uint32_t nCsma = 3;

    // wn 개수 (시나리오1에서 해당 값 변경)
    // 노드가 4개이면 wn 3개, AP 1개로 구성
    const uint32_t nWifi = 4;

    // CWmin (시나리오2에서 해당 값 변경)
    const uint32_t CWmin = 63;

    // 패킷 사이즈 (시나리오 3에서 해당 값 변경)
    const uint32_t packetSize = 1500;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    // p2p 노드 생성
    NodeContainer p2pNodes;
    p2pNodes.Create(2);

    // p2p 연결 설정
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // p2p 노드에 설정 적용
    NetDeviceContainer p2pDevices;
    p2pDevices = pointToPoint.Install(p2pNodes);

    // p2p 노드와 유선 노드 묶음
    NodeContainer csmaNodes;
    csmaNodes.Add(p2pNodes.Get(1)); // AP와 연결될 R 라우터 node 생성
    csmaNodes.Create(nCsma);        // fn 노드 생성

    // 유선 노드 연결 설정
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(6)));

    // 유선 노드에 설정 적용
    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install(csmaNodes);

    // wn과 ap 노드가 각각 담긴 컨테이너 생성
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nWifi);
    NodeContainer wifiApNode = p2pNodes.Get(0);

    // YANS wifi 모델 사용 (attenuation, interference, noise 등 현실 요소 반영 모델)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    // wn에 ssid, wifi모델 설정 및 active probing 비활성화
    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac",
                "Ssid",
                SsidValue(ssid),
                "ActiveProbing",
                BooleanValue(false),
                "QosSupported",
                BooleanValue(false));

    // wifi 네트워크에 노드 연결
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // ap 노드에 ssid, wifi모델 설정 및 wifi 네트워크에 노드 연결
    NetDeviceContainer apDevices;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "QosSupported", BooleanValue(false));

    // wifi 네트워크에 노드 연결
    apDevices = wifi.Install(phy, mac, wifiApNode);

    // STA NetDevice 가져오기
    Ptr<NetDevice> dev = staDevices.Get(0);
    Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(dev);
    Ptr<WifiMac> wifiMac = wifiDev->GetMac();

    // Txop Attribute 추출
    PointerValue ptr;
    wifiMac->GetAttribute("Txop", ptr);
    Ptr<Txop> txop = ptr.Get<Txop>();

    // CW 설정
    txop->SetMinCw(CWmin);

    // 모든 wn 위치 고정
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(0.0),
                                  "MinY",
                                  DoubleValue(0.0),
                                  "DeltaX",
                                  DoubleValue(5.0),
                                  "DeltaY",
                                  DoubleValue(10.0),
                                  "GridWidth",
                                  UintegerValue(3),
                                  "LayoutType",
                                  StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // 인터넷 프로토콜 스택을 모든 노드에 설치
    InternetStackHelper stack;
    stack.Install(csmaNodes);
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;

    // p2p IP 주소 등록
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces;
    p2pInterfaces = address.Assign(p2pDevices);

    // LAN에 IP 주소 등록
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces;
    csmaInterfaces = address.Assign(csmaDevices);

    // wn과 ap에 IP주소 등록
    address.SetBase("10.1.3.0", "255.255.255.0");
    address.Assign(staDevices);
    address.Assign(apDevices);

    // udp server 설정
    UdpServerHelper udpServer(9);
    ApplicationContainer serverApps = udpServer.Install(csmaNodes.Get(nCsma));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(31.0)); // 30초 동안 동작 (udp client가 데이터를 전부 수신하기 위함)

    // udp client 설정
    UdpClientHelper udpClient(csmaInterfaces.GetAddress(nCsma), 9);
    udpClient.SetAttribute("MaxPackets", UintegerValue(0));

    // 10Mbps로 전송하기 위한 주기 계산
    double packetPeriod = static_cast<double>(packetSize * 8) / 10000000.0;
    udpClient.SetAttribute("Interval",
                           TimeValue(Seconds(packetPeriod))); // 10Mbps로 전송
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    // 모든 wn에 UDP 클라이언트 설치
    ApplicationContainer clientApps;
    for (int i = 0; i < nWifi; ++i)
    {
        clientApps.Add(udpClient.Install(wifiStaNodes.Get(i)));
    }

    clientApps.Start(Seconds(2.0)); // sever가 시작된 후에 전송 시작
    clientApps.Stop(Seconds(22.0)); // 20초 동안 동작

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    Simulator::Stop(Seconds(31.0));

    if (tracing)
    {
        phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        pointToPoint.EnablePcapAll("third");
        phy.EnablePcap("third", apDevices.Get(0));
        csma.EnablePcap("third", csmaDevices.Get(0), true);
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
