#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/traci-module.h"  // Use actual TraCI module
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <sstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SUMONs3Integration");

// Data structure including an emergency flag.
struct TrafficData {
  uint32_t vehicleCount;
  bool emergency;
};

std::map<std::string, TrafficData> junctionTrafficData;
std::ifstream trafficSensorFile;
std::string sumoConfig;
bool usingGui = true;
double stepLength = 0.1;  // SUMO step length in seconds

void ReadTrafficSensorData()
{
  std::string line;
  std::map<uint32_t, TrafficData> latestTrafficData;

  // Skip header
  std::getline(trafficSensorFile, line);

  while (std::getline(trafficSensorFile, line)) {
    // Parse CSV data: Time,NodeID,VehicleCount,EmergencyFlag
    uint32_t nodeId;
    uint32_t vehicleCount;
    int emergencyFlag = 0;

    std::stringstream ss(line);
    std::string token;

    std::getline(ss, token, ','); // Time field (ignored)

    std::getline(ss, token, ',');
    nodeId = std::stoi(token);

    std::getline(ss, token, ',');
    vehicleCount = std::stoi(token);

    if (std::getline(ss, token, ',')) {
      emergencyFlag = std::stoi(token);
    }

    latestTrafficData[nodeId] = {vehicleCount, (emergencyFlag != 0)};
  }

  // Map node IDs to junction IDs (assuming 1:1 mapping)
  for (const auto &pair : latestTrafficData) {
    std::string junctionId = "J" + std::to_string(pair.first);
    junctionTrafficData[junctionId] = pair.second;
  }
}

void AdjustTrafficLights(Ptr<TraciClient> client)
{
  NS_LOG_INFO("Adjusting traffic light timings based on sensor data at time " 
              << Simulator::Now().GetSeconds());

  std::vector<std::string> tlsIds = client->TrafficLightGetIDList();

  for (const std::string &tlsId : tlsIds) {
    std::vector<std::string> controlledJunctions = client->TrafficLightGetControlledJunctions(tlsId);

    int totalVehicles = 0;
    bool emergencyDetected = false;
    for (const std::string &junction : controlledJunctions) {
      if (junctionTrafficData.find(junction) != junctionTrafficData.end()) {
        totalVehicles += junctionTrafficData[junction].vehicleCount;
        if (junctionTrafficData[junction].emergency) {
          emergencyDetected = true;
        }
      }
    }

    // Apply traffic light program based on traffic conditions
    if (emergencyDetected) {
      NS_LOG_INFO("Emergency detected at " << tlsId << ", setting emergency mode");
      client->TrafficLightSetProgram(tlsId, "emergency");
    } else if (totalVehicles > 8) {
      NS_LOG_INFO("Heavy traffic at " << tlsId << ", total vehicles: " << totalVehicles);
      client->TrafficLightSetProgram(tlsId, "heavy_traffic");
    } else if (totalVehicles < 3) {
      NS_LOG_INFO("Light traffic at " << tlsId << ", total vehicles: " << totalVehicles);
      client->TrafficLightSetProgram(tlsId, "light_traffic");
    } else {
      NS_LOG_INFO("Normal traffic at " << tlsId << ", total vehicles: " << totalVehicles);
      client->TrafficLightSetProgram(tlsId, "normal");
    }
  }

  // Collect vehicle data from SUMO
  std::map<std::string, Ptr<ConstantPositionMobilityModel>> mobilityModels;
  std::vector<std::string> vehicles = client->VehicleGetIDList();
  
  NS_LOG_INFO("Number of vehicles in simulation: " << vehicles.size());
}

// TraCI client callback - executed for each SUMO time step
void TraCIClientCallback(Ptr<TraciClient> client)
{
  static uint32_t count = 0;
  
  // Only process sensor data every 50 steps (5 seconds with 0.1s steps)
  if (count++ % 50 == 0) {
    trafficSensorFile.clear();
    trafficSensorFile.seekg(0);
    ReadTrafficSensorData();
    AdjustTrafficLights(client);
  }
}

int main(int argc, char *argv[])
{
  // Default values
  sumoConfig = "sumo-config.xml";
  std::string trafficDataFile = "traffic_sensor_data.csv";
  double simTime = 100.0;
  std::string outputDir = ".";  // Default to current directory

  CommandLine cmd;
  cmd.AddValue("sumoConfig", "SUMO configuration file", sumoConfig);
  cmd.AddValue("trafficData", "Traffic sensor data file", trafficDataFile);
  cmd.AddValue("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue("gui", "Use SUMO GUI", usingGui);
  cmd.AddValue("stepLength", "SUMO simulation step length", stepLength);
  cmd.AddValue("outputDir", "Directory for output files", outputDir);
  cmd.Parse(argc, argv);

  // Enable logging
  LogComponentEnable("SUMONs3Integration", LOG_LEVEL_INFO);
  LogComponentEnable("TraciClient", LOG_LEVEL_WARN);

  // Open traffic sensor data file
  trafficSensorFile.open(trafficDataFile);
  if (!trafficSensorFile.is_open()) {
    NS_LOG_ERROR("Failed to open traffic data file: " << trafficDataFile);
    return 1;
  }

  // Generate run_integrated_simulation.sh in the specified directory
  std::string scriptPath = outputDir + "/run_integrated_simulation.sh";
  std::ofstream scriptFile(scriptPath.c_str());

  // Write contents to the scriptFile
  scriptFile << "#!/bin/bash\n\n"
             << "# This script runs SUMO with the traffic light control based on sensor data\n\n"
             << "# Path to SUMO configuration\n"
             << "SUMO_CONFIG=\"" << sumoConfig << "\"\n\n"
             << "# Start SUMO with GUI\n"
             << "sumo-gui -c \"$SUMO_CONFIG\" --remote-port 8813 --start &\n\n"
             << "# Wait for SUMO to initialize\n"
             << "sleep 2\n\n"
             << "# Read traffic sensor data and apply traffic light control\n"
             << "cd \"" << outputDir << "\"\n"  // Change directory to the output dir
             << "python3 <<EOF\n"
             // rest of Python script with proper path to traffic_sensor_data.csv
             << "EOF\n";
  scriptFile.close();

  // Make script executable
  std::string chmodCmd = "chmod +x " + scriptPath;
  system(chmodCmd.c_str());

  // Create and configure the TraCI client
  Ptr<TraciClient> client = CreateObject<TraciClient>();
  client->SetAttribute("SumoConfigPath", StringValue(sumoConfig));
  client->SetAttribute("SumoBinaryPath", StringValue(usingGui ? "sumo-gui" : "sumo"));
  client->SetAttribute("SumoWaitForConnection", BooleanValue(true));
  client->SetAttribute("SynchInterval", TimeValue(Seconds(stepLength)));
  client->SetAttribute("StartTime", TimeValue(Seconds(0.0)));
  client->SetAttribute("SumoGUI", BooleanValue(usingGui));
  
  // Register the client callback to be executed at each time step
  client->TraceConnectWithoutContext("SumoSimulationStep", MakeCallback(&TraCIClientCallback));

  // Initialize and start TraCI
  client->Init();

  // Run the simulation
  NS_LOG_INFO("Starting simulation for " << simTime << " seconds");
  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();

  // Clean up
  trafficSensorFile.close();

  NS_LOG_INFO("Simulation completed successfully.");
  return 0;
}