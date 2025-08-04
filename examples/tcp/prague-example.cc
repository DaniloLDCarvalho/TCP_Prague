/* Exemplo completo: TCP Prague com FQ-CoDel */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/csma-module.h"

#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

using namespace ns3;

// Globais para coleta de estatísticas
std::ofstream rxS1R1Throughput;
std::ofstream rxS2R2Throughput;
std::ofstream rxS3R1Throughput;
std::ofstream fairnessIndex;
std::ofstream t1QueueLength;
std::ofstream t2QueueLength;
std::vector<uint64_t> rxS1R1Bytes(10, 0);
std::vector<uint64_t> rxS2R2Bytes(20, 0);
std::vector<uint64_t> rxS3R1Bytes(10, 0);

void TraceS1R1Sink(std::size_t index, Ptr<const Packet> p, const Address& a) { rxS1R1Bytes[index] += p->GetSize(); }
void TraceS2R2Sink(std::size_t index, Ptr<const Packet> p, const Address& a) { rxS2R2Bytes[index] += p->GetSize(); }
void TraceS3R1Sink(std::size_t index, Ptr<const Packet> p, const Address& a) { rxS3R1Bytes[index] += p->GetSize(); }

void InitializeCounters() {
    for (std::size_t i = 0; i < 10; i++) rxS1R1Bytes[i] = 0;
    for (std::size_t i = 0; i < 20; i++) rxS2R2Bytes[i] = 0;
    for (std::size_t i = 0; i < 10; i++) rxS3R1Bytes[i] = 0;
}

void
PrintProgress(Time interval)
{
    std::cout << "Progress to " << std::fixed << std::setprecision(1)
              << Simulator::Now().GetSeconds() << " seconds simulation time" << std::endl;
    Simulator::Schedule(interval, &PrintProgress, interval);
}

void PrintThroughput(Time measurementWindow) {
    for (std::size_t i = 0; i < 10; i++)
        rxS1R1Throughput << measurementWindow.GetSeconds() << "s " << i << " "
            << (rxS1R1Bytes[i] * 8) / (measurementWindow.GetSeconds()) / 1e6 << std::endl;
    for (std::size_t i = 0; i < 20; i++)
        rxS2R2Throughput << measurementWindow.GetSeconds() << "s " << i << " "
            << (rxS2R2Bytes[i] * 8) / (measurementWindow.GetSeconds()) / 1e6 << std::endl;
    for (std::size_t i = 0; i < 10; i++)
        rxS3R1Throughput << measurementWindow.GetSeconds() << "s " << i << " "
            << (rxS3R1Bytes[i] * 8) / (measurementWindow.GetSeconds()) / 1e6 << std::endl;
}

void PrintFairness(Time measurementWindow) {
    auto calcFairness = [measurementWindow](const std::vector<uint64_t>& v, int n, const std::string& label) {
        double avg = 0, fairness = 0;
        uint64_t sum = 0, sumSq = 0;
        for (int i = 0; i < n; i++) {
            sum += v[i];
            sumSq += v[i] * v[i];
        }
        avg = ((sum / n) * 8 / measurementWindow.GetSeconds()) / 1e6;
        if (sumSq > 0)
            fairness = static_cast<double>(sum * sum) / (n * sumSq);
        fairnessIndex << "Average throughput for " << label << ": "
            << std::fixed << std::setprecision(2) << avg << " Mbps; fairness: "
            << std::fixed << std::setprecision(3) << fairness << std::endl;
    };
    calcFairness(rxS1R1Bytes, 10, "S1-R1 flows");
    calcFairness(rxS2R2Bytes, 20, "S2-R2 flows");
    calcFairness(rxS3R1Bytes, 10, "S3-R1 flows");

    // Aggregate throughput
    uint64_t sumT1 = 0, sumR1 = 0;
    for (int i = 0; i < 10; i++) sumT1 += rxS1R1Bytes[i];
    for (int i = 0; i < 20; i++) sumT1 += rxS2R2Bytes[i];
    for (int i = 0; i < 10; i++) sumR1 += rxS3R1Bytes[i];
    for (int i = 0; i < 10; i++) sumR1 += rxS1R1Bytes[i];
    fairnessIndex << "Aggregate user-level throughput for flows through T1: "
        << static_cast<double>(sumT1 * 8) / 1e9 << " Gbps" << std::endl;
    fairnessIndex << "Aggregate user-level throughput for flows to R1: "
        << static_cast<double>(sumR1 * 8) / 1e9 << " Gbps" << std::endl;
}

void CheckT1QueueSize(Ptr<QueueDisc> queue) {
    uint32_t qSize = queue->GetNPackets();
    Time backlog = Seconds(static_cast<double>(qSize * 1500 * 8) / 1e10); // 10 Gb/s
    t1QueueLength << std::fixed << std::setprecision(2) << Simulator::Now().GetSeconds() << " "
        << qSize << " " << backlog.GetMicroSeconds() << std::endl;
    Simulator::Schedule(MilliSeconds(10), &CheckT1QueueSize, queue);
}
void CheckT2QueueSize(Ptr<QueueDisc> queue) {
    uint32_t qSize = queue->GetNPackets();
    Time backlog = Seconds(static_cast<double>(qSize * 1500 * 8) / 1e9); // 1 Gb/s
    t2QueueLength << std::fixed << std::setprecision(2) << Simulator::Now().GetSeconds() << " "
        << qSize << " " << backlog.GetMicroSeconds() << std::endl;
    Simulator::Schedule(MilliSeconds(10), &CheckT2QueueSize, queue);
}

int main(int argc, char* argv[]) {
    std::string tcpTypeId = "TcpPrague";
    //std::string tcpTypeId = "TcpDctcp";
    Time flowStartupWindow = Seconds(0.1);
    Time convergenceTime = Seconds(0.3);
    Time measurementWindow = Seconds(0.1);
    bool enableSwitchEcn = true;
    Time progressInterval = MilliSeconds(100);

    CommandLine cmd(__FILE__);
    cmd.AddValue("tcpTypeId", "ns-3 TCP TypeId", tcpTypeId);
    cmd.AddValue("flowStartupWindow", "startup time window (TCP staggered starts)", flowStartupWindow);
    cmd.AddValue("convergenceTime", "convergence time", convergenceTime);
    cmd.AddValue("measurementWindow", "measurement window", measurementWindow);
    cmd.AddValue("enableSwitchEcn", "enable ECN at switches", enableSwitchEcn);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::" + tcpTypeId));
    Config::SetDefault("ns3::FqCoDelQueueDisc::UseEcn", BooleanValue(enableSwitchEcn));

    Time startTime{0};
    Time stopTime = flowStartupWindow + convergenceTime + measurementWindow;

    NodeContainer S1, S2, S3, R2;
    Ptr<Node> T1 = CreateObject<Node>();
    Ptr<Node> T2 = CreateObject<Node>();
    Ptr<Node> R1 = CreateObject<Node>();
    S1.Create(10);
    S2.Create(20);
    S3.Create(10);
    R2.Create(20);

    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(2));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(false));

    PointToPointHelper pointToPointSR;
    pointToPointSR.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    pointToPointSR.SetChannelAttribute("Delay", StringValue("10us"));

    PointToPointHelper pointToPointT;
    pointToPointT.SetDeviceAttribute("DataRate", StringValue("20Mbps"));
    pointToPointT.SetChannelAttribute("Delay", StringValue("10us"));

    std::vector<NetDeviceContainer> S1T1(10), S2T1(20), S3T2(10), R2T2(20);
    NetDeviceContainer T1T2 = pointToPointT.Install(T1, T2);
    NetDeviceContainer R1T2 = pointToPointSR.Install(R1, T2);

    for (std::size_t i = 0; i < 10; i++)
        S1T1[i] = pointToPointSR.Install(S1.Get(i), T1);
    for (std::size_t i = 0; i < 20; i++)
        S2T1[i] = pointToPointSR.Install(S2.Get(i), T1);
    for (std::size_t i = 0; i < 10; i++)
        S3T2[i] = pointToPointSR.Install(S3.Get(i), T2);
    for (std::size_t i = 0; i < 20; i++)
        R2T2[i] = pointToPointSR.Install(R2.Get(i), T2);

    pointToPointT.EnablePcapAll("fqcodel-marking");
    InternetStackHelper stack;
    stack.InstallAll();

    TrafficControlHelper tchFqCodel10;
    tchFqCodel10.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    QueueDiscContainer queueDiscs1 = tchFqCodel10.Install(T1T2.Get(0));

    TrafficControlHelper tchFqCodel1;
    tchFqCodel1.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    QueueDiscContainer queueDiscs2 = tchFqCodel1.Install(R1T2.Get(1));

    for (std::size_t i = 0; i < 10; i++)
        tchFqCodel1.Install(S1T1[i].Get(1));
    for (std::size_t i = 0; i < 20; i++)
        tchFqCodel1.Install(S2T1[i].Get(1));
    for (std::size_t i = 0; i < 10; i++)
        tchFqCodel1.Install(S3T2[i].Get(1));
    for (std::size_t i = 0; i < 20; i++)
        tchFqCodel1.Install(R2T2[i].Get(1));

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> ipS1T1(10), ipS2T1(20), ipS3T2(10), ipR2T2(20);
    address.SetBase("172.16.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ipT1T2 = address.Assign(T1T2);
    address.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ipR1T2 = address.Assign(R1T2);
    address.SetBase("10.1.1.0", "255.255.255.0");
    for (std::size_t i = 0; i < 10; i++) {
        ipS1T1[i] = address.Assign(S1T1[i]);
        address.NewNetwork();
    }
    address.SetBase("10.2.1.0", "255.255.255.0");
    for (std::size_t i = 0; i < 20; i++) {
        ipS2T1[i] = address.Assign(S2T1[i]);
        address.NewNetwork();
    }
    address.SetBase("10.3.1.0", "255.255.255.0");
    for (std::size_t i = 0; i < 10; i++) {
        ipS3T2[i] = address.Assign(S3T2[i]);
        address.NewNetwork();
    }
    address.SetBase("10.4.1.0", "255.255.255.0");
    for (std::size_t i = 0; i < 20; i++) {
        ipR2T2[i] = address.Assign(R2T2[i]);
        address.NewNetwork();
    }
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // === Aplicações e coleta ===
    std::vector<Ptr<PacketSink>> r2Sinks(20);
    for (std::size_t i = 0; i < 20; i++) {
        uint16_t port = 50000 + i;
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(R2.Get(i));
        Ptr<PacketSink> packetSink = sinkApp.Get(0)->GetObject<PacketSink>();
        r2Sinks[i] = packetSink;
        sinkApp.Start(startTime);
        sinkApp.Stop(stopTime);

        OnOffHelper clientHelper1("ns3::TcpSocketFactory", Address());
        clientHelper1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        clientHelper1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        clientHelper1.SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
        clientHelper1.SetAttribute("PacketSize", UintegerValue(1000));
        AddressValue remoteAddress(InetSocketAddress(ipR2T2[i].GetAddress(0), port));
        clientHelper1.SetAttribute("Remote", remoteAddress);
        ApplicationContainer clientApp = clientHelper1.Install(S2.Get(i));
        clientApp.Start(i * flowStartupWindow / 20 + startTime + MilliSeconds(i * 5));
        clientApp.Stop(stopTime);
    }

    std::vector<Ptr<PacketSink>> s1r1Sinks(10), s3r1Sinks(10);
    for (std::size_t i = 0; i < 20; i++) {
        uint16_t port = 60000 + i;
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(R1);
        Ptr<PacketSink> packetSink = sinkApp.Get(0)->GetObject<PacketSink>();
        if (i < 10) s1r1Sinks[i] = packetSink;
        else s3r1Sinks[i-10] = packetSink;
        sinkApp.Start(startTime);
        sinkApp.Stop(stopTime);

        OnOffHelper clientHelper1("ns3::TcpSocketFactory", Address());
        clientHelper1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        clientHelper1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        clientHelper1.SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
        clientHelper1.SetAttribute("PacketSize", UintegerValue(1000));
        AddressValue remoteAddress(InetSocketAddress(ipR1T2.GetAddress(0), port));
        clientHelper1.SetAttribute("Remote", remoteAddress);

        ApplicationContainer clientApp;
        if (i < 10) {
            clientApp = clientHelper1.Install(S1.Get(i));
            clientApp.Start(i * flowStartupWindow / 10 + startTime + MilliSeconds(i * 5));
        } else {
            clientApp = clientHelper1.Install(S3.Get(i-10));
            clientApp.Start((i-10) * flowStartupWindow / 10 + startTime + MilliSeconds(i * 5));
        }
        clientApp.Stop(stopTime);
    }

    rxS1R1Throughput.open("prague-example-s1-r1-throughput.dat", std::ios::out);
    rxS1R1Throughput << "#Time(s) flow thruput(Mb/s)" << std::endl;
    rxS2R2Throughput.open("prague-example-s2-r2-throughput.dat", std::ios::out);
    rxS2R2Throughput << "#Time(s) flow thruput(Mb/s)" << std::endl;
    rxS3R1Throughput.open("prague-example-s3-r1-throughput.dat", std::ios::out);
    rxS3R1Throughput << "#Time(s) flow thruput(Mb/s)" << std::endl;
    fairnessIndex.open("prague-example-fairness.dat", std::ios::out);
    t1QueueLength.open("prague-example-t1-length.dat", std::ios::out);
    t1QueueLength << "#Time(s) qlen(pkts) qlen(us)" << std::endl;
    t2QueueLength.open("prague-example-t2-length.dat", std::ios::out);
    t2QueueLength << "#Time(s) qlen(pkts) qlen(us)" << std::endl;

    for (std::size_t i = 0; i < 10; i++)
        s1r1Sinks[i]->TraceConnectWithoutContext("Rx", MakeBoundCallback(&TraceS1R1Sink, i));
    for (std::size_t i = 0; i < 20; i++)
        r2Sinks[i]->TraceConnectWithoutContext("Rx", MakeBoundCallback(&TraceS2R2Sink, i));
    for (std::size_t i = 0; i < 10; i++)
        s3r1Sinks[i]->TraceConnectWithoutContext("Rx", MakeBoundCallback(&TraceS3R1Sink, i));

    Simulator::Schedule(flowStartupWindow + convergenceTime, &InitializeCounters);
    Simulator::Schedule(flowStartupWindow + convergenceTime + measurementWindow, &PrintThroughput, measurementWindow);
    Simulator::Schedule(flowStartupWindow + convergenceTime + measurementWindow, &PrintFairness, measurementWindow);
    Simulator::Schedule(flowStartupWindow + convergenceTime, &CheckT1QueueSize, queueDiscs1.Get(0));
    Simulator::Schedule(flowStartupWindow + convergenceTime, &CheckT2QueueSize, queueDiscs2.Get(0));

    Simulator::Stop(stopTime + TimeStep(1));
    Simulator::Schedule(progressInterval, &PrintProgress, progressInterval);
    LogComponentEnable("FqCoDelQueueDisc", LOG_LEVEL_INFO);
    Simulator::Run();

    rxS1R1Throughput.close();
    rxS2R2Throughput.close();
    rxS3R1Throughput.close();
    fairnessIndex.close();
    t1QueueLength.close();
    t2QueueLength.close();
    Simulator::Destroy();
    return 0;
}
