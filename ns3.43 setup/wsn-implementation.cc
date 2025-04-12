#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/energy-module.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TrafficWSN");

// Custom header for emergency indication
class EmergencyHeader : public Header {
public:
    EmergencyHeader() : m_isEmergency(false) {}
    void SetEmergency(bool flag) { m_isEmergency = flag; }
    bool IsEmergency() const { return m_isEmergency; }

    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("EmergencyHeader")
            .SetParent<Header>()
            .AddConstructor<EmergencyHeader>();
        return tid;
    }

    virtual TypeId GetInstanceTypeId(void) const { return GetTypeId(); }
    virtual void Serialize(Buffer::Iterator start) const {
        start.WriteU8(m_isEmergency ? 1 : 0);
    }
    virtual uint32_t GetSerializedSize(void) const { return 1; }
    virtual uint32_t Deserialize(Buffer::Iterator start) {
        uint8_t flag = start.ReadU8();
        m_isEmergency = (flag == 1);
        return GetSerializedSize();
    }
    virtual void Print(std::ostream &os) const { os << "Emergency: " << m_isEmergency; }
private:
    bool m_isEmergency;
};

// Global variables for data collection
std::map<uint32_t, uint32_t> nodeDataCount;
std::ofstream trafficDataFile;

// Renamed custom distance function to avoid ambiguity.
double MyCalculateDistance(Vector a, Vector b) {
    double dx = b.x - a.x;
    double dy = b.y - a.y;
    double dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

class TrafficSensorApplication : public Application {
public:
    TrafficSensorApplication();
    virtual ~TrafficSensorApplication();

    static TypeId GetTypeId(void);
    void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets,
               DataRate dataRate, bool isRFD, bool simulateEmergency = false);

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void SendPacket(void);
    void SendEmergencyPacket(void);
    void ScheduleTx(void);
    void SleepCycle(void);

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    DataRate m_dataRate;
    EventId m_sendEvent;
    bool m_running;
    uint32_t m_packetsSent;
    bool m_isRFD;
    bool m_simulateEmergency;
};

TrafficSensorApplication::TrafficSensorApplication()
    : m_socket(nullptr),
      m_peer(),
      m_packetSize(0),
      m_nPackets(0),
      m_dataRate(0),
      m_running(false),
      m_packetsSent(0),
      m_isRFD(false),
      m_simulateEmergency(false) {
}

TrafficSensorApplication::~TrafficSensorApplication() {
    m_socket = nullptr;
}

TypeId TrafficSensorApplication::GetTypeId(void) {
    static TypeId tid = TypeId("TrafficSensorApplication")
        .SetParent<Application>()
        .SetGroupName("WSN")
        .AddConstructor<TrafficSensorApplication>();
    return tid;
}

void TrafficSensorApplication::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize,
                                     uint32_t nPackets, DataRate dataRate, bool isRFD, bool simulateEmergency) {
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_dataRate = dataRate;
    m_isRFD = isRFD;
    m_simulateEmergency = simulateEmergency;
}

void TrafficSensorApplication::StartApplication(void) {
    m_running = true;
    m_packetsSent = 0;

    if (m_isRFD) {
        Simulator::Schedule(Seconds(0.5), &TrafficSensorApplication::SleepCycle, this);
    } else {
        ScheduleTx();
    }
}

void TrafficSensorApplication::SleepCycle(void) {
    if (!m_running) return;
    NS_LOG_INFO("Node " << GetNode()->GetId() << " entering sleep mode for energy saving");
    Simulator::Schedule(Seconds(1.0), &TrafficSensorApplication::ScheduleTx, this);
}

void TrafficSensorApplication::StopApplication(void) {
    m_running = false;
    if (m_sendEvent.IsPending())
        Simulator::Cancel(m_sendEvent);

    if (m_socket)
        m_socket->Close();
}

void TrafficSensorApplication::SendPacket(void) {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);

    if (m_isRFD) {
        uint32_t trafficCount = rand() % 10;
        uint32_t nodeId = GetNode()->GetId();
        bool emergency = m_simulateEmergency && (trafficCount > 8);

        EmergencyHeader eh;
        eh.SetEmergency(emergency);
        packet->AddHeader(eh);

        trafficDataFile << Simulator::Now().GetSeconds() << ","
                        << nodeId << ","
                        << trafficCount << ","
                        << (emergency ? 1 : 0) << std::endl;

        NS_LOG_INFO("Node " << nodeId << " detected " << trafficCount
                            << " vehicles at time " << Simulator::Now().GetSeconds()
                            << (emergency ? " [EMERGENCY]" : ""));
    }

    m_socket->Send(packet);
    m_packetsSent++;

    if (m_packetsSent < m_nPackets) {
        ScheduleTx();
    }
}

void TrafficSensorApplication::SendEmergencyPacket(void) {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    EmergencyHeader eh;
    eh.SetEmergency(true);
    packet->AddHeader(eh);

    m_socket->Send(packet);
    NS_LOG_INFO("Node " << GetNode()->GetId() << " sent an emergency packet at time " << Simulator::Now().GetSeconds());
}

void TrafficSensorApplication::ScheduleTx(void) {
    if (m_running) {
        Time tNext(Seconds(m_packetSize * 8 / static_cast<double>(m_dataRate.GetBitRate())));
        m_sendEvent = Simulator::Schedule(tNext, &TrafficSensorApplication::SendPacket, this);
    }
}

void ReceivePacket(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;

    while ((packet = socket->RecvFrom(from))) {
        EmergencyHeader eh;
        packet->PeekHeader(eh);
        uint32_t receiverNodeId = socket->GetNode()->GetId();
        nodeDataCount[receiverNodeId]++;

        NS_LOG_INFO("Node " << receiverNodeId << " received packet"
                            << (eh.IsEmergency() ? " [EMERGENCY]" : ""));

        if (receiverNodeId == 0) {
            NS_LOG_INFO("FPC received data, total packets: " << nodeDataCount[receiverNodeId]);
        }
    }
}

int main(int argc, char *argv[]) {
    uint32_t numRFD = 10;
    uint32_t numFFD = 3;
    uint32_t numFPC = 1;
    double simTime = 100.0;
    std::string outputDir = ".";  // Default to current directory

    CommandLine cmd;
    cmd.AddValue("numRFD", "Number of RFD nodes", numRFD);
    cmd.AddValue("numFFD", "Number of FFD nodes", numFFD);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("outputDir", "Directory for output files", outputDir);
    cmd.Parse(argc, argv);

    LogComponentEnable("TrafficWSN", LOG_LEVEL_INFO);

    NodeContainer fpcNodes, ffdNodes, rfdNodes;
    fpcNodes.Create(numFPC);
    ffdNodes.Create(numFFD);
    rfdNodes.Create(numRFD);

    NodeContainer allNodes;
    allNodes.Add(fpcNodes);
    allNodes.Add(ffdNodes);
    allNodes.Add(rfdNodes);

    LrWpanHelper lrWpanHelper;
    NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(allNodes);

    // Assign unique short addresses to avoid IPv4 address collisions.
    for (uint32_t i = 0; i < lrwpanDevices.GetN(); i++) {
        Ptr<NetDevice> dev = lrwpanDevices.Get(i);
        Ptr<ns3::lrwpan::LrWpanNetDevice> lrwpanDev = DynamicCast<ns3::lrwpan::LrWpanNetDevice>(dev);
        if (lrwpanDev != nullptr) {
            Ptr<ns3::lrwpan::LrWpanMac> mac = lrwpanDev->GetMac();
            if (mac != nullptr) {
                mac->SetShortAddress(i + 1);
            }
        }
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(50.0),
                                  "MinY", DoubleValue(50.0),
                                  "DeltaX", DoubleValue(0.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(fpcNodes);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(30.0),
                                  "MinY", DoubleValue(30.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.Install(ffdNodes);

    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.Install(rfdNodes);

    InternetStackHelper internet;
    internet.Install(allNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(lrwpanDevices);

    // Debugging: Print the number of interfaces assigned
    NS_LOG_INFO("Number of interfaces assigned: " << interfaces.GetN());

    std::string trafficDataPath = outputDir + "/traffic_sensor_data.csv";
    trafficDataFile.open(trafficDataPath.c_str());
    trafficDataFile << "Time,NodeID,VehicleCount,Emergency" << std::endl;

    uint16_t port = 9;
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

    Ptr<Socket> fpcRxSocket = Socket::CreateSocket(fpcNodes.Get(0), tid);
    InetSocketAddress fpcRxSocketAddr = InetSocketAddress(Ipv4Address::GetAny(), port);
    fpcRxSocket->Bind(fpcRxSocketAddr);
    fpcRxSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    std::vector<Ptr<Socket>> ffdRxSockets;
    for (uint32_t i = 0; i < numFFD; i++) {
        Ptr<Socket> socket = Socket::CreateSocket(ffdNodes.Get(i), tid);
        InetSocketAddress socketAddr = InetSocketAddress(Ipv4Address::GetAny(), port);
        socket->Bind(socketAddr);
        socket->SetRecvCallback(MakeCallback(&ReceivePacket));
        ffdRxSockets.push_back(socket);

        Ptr<Socket> ffdTxSocket = Socket::CreateSocket(ffdNodes.Get(i), tid);
        InetSocketAddress fpcAddr = InetSocketAddress(interfaces.GetAddress(0), port);

        Ptr<TrafficSensorApplication> app = CreateObject<TrafficSensorApplication>();
        app->Setup(ffdTxSocket, fpcAddr, 1024, 1000, DataRate("1kbps"), false, false);
        ffdNodes.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(1.0));
        app->SetStopTime(Seconds(simTime));
    }

    for (uint32_t i = 0; i < numRFD; i++) {
        uint32_t closestFFD = 0;
        double minDistance = std::numeric_limits<double>::max();

        for (uint32_t j = 0; j < numFFD; j++) {
            Vector ffdPos = ffdNodes.Get(j)->GetObject<MobilityModel>()->GetPosition();
            Vector rfdPos = rfdNodes.Get(i)->GetObject<MobilityModel>()->GetPosition();
            double distance = MyCalculateDistance(ffdPos, rfdPos);
            if (distance < minDistance) {
                minDistance = distance;
                closestFFD = j;
            }
        }

        Ptr<Socket> socket = Socket::CreateSocket(rfdNodes.Get(i), tid);
        uint32_t interfaceIndex = numFPC + closestFFD;
        NS_LOG_INFO("Assigning interface for RFD " << i << " to FFD " << closestFFD << " with interface index " << interfaceIndex);
        if (interfaceIndex < interfaces.GetN()) {
            InetSocketAddress ffdAddr = InetSocketAddress(interfaces.GetAddress(interfaceIndex), port);

            Ptr<TrafficSensorApplication> app = CreateObject<TrafficSensorApplication>();
            app->Setup(socket, ffdAddr, 512, 1000, DataRate("1kbps"), true, true);
            rfdNodes.Get(i)->AddApplication(app);
            app->SetStartTime(Seconds(1.0 + (0.1 * i)));
            app->SetStopTime(Seconds(simTime));
        } else {
            NS_LOG_ERROR("Interface index out of bounds: " << interfaceIndex);
        }
    }

    AnimationInterface anim("traffic-wsn-animation.xml");
    for (uint32_t i = 0; i < numFPC; i++) {
        anim.UpdateNodeDescription(fpcNodes.Get(i), "FPC");
        anim.UpdateNodeColor(fpcNodes.Get(i), 255, 0, 0);
    }
    for (uint32_t i = 0; i < numFFD; i++) {
        anim.UpdateNodeDescription(ffdNodes.Get(i), "FFD");
        anim.UpdateNodeColor(ffdNodes.Get(i), 0, 255, 0);
    }
    for (uint32_t i = 0; i < numRFD; i++) {
        anim.UpdateNodeDescription(rfdNodes.Get(i), "RFD");
        anim.UpdateNodeColor(rfdNodes.Get(i), 0, 0, 255);
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    trafficDataFile.close();

    std::cout << "Simulation completed. Data stored in traffic_sensor_data.csv" << std::endl;

    return 0;
}