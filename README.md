
![alt text](https://github.com/VijayKagupati/Traffic-Simulation-with-Wireless-Sensor-Networks/blob/main/project-banner.svg)
# Traffic Simulation with Wireless Sensor Networks

This project integrates the Network Simulator 3 (NS-3) with SUMO (Simulation of Urban Mobility) to create an intelligent traffic management system using wireless sensor networks (WSNs). The system simulates traffic sensors that collect vehicle data, detect emergencies, and adaptively control traffic lights to optimize traffic flow.

## Table of Contents
* [Overview](#overview)
* [Features](#features)
* [Project Structure](#project-structure)
* [Prerequisites](#prerequisites)
* [Installation](#installation)
* [Usage](#usage)
* [Components](#components)
* [Simulation Workflow](#simulation-workflow)
* [Customization](#customization)
* [Troubleshooting](#troubleshooting)

## Overview
![diagram-export-12-4-2025-7_21_28-am](https://github.com/user-attachments/assets/8be977b9-9fb5-4fb3-89bb-4559f951419a)

This project demonstrates intelligent traffic management by:
1. Simulating wireless sensor networks that collect traffic data using NS-3
2. Using the collected data to dynamically adjust traffic signals in SUMO
3. Visualizing traffic patterns with a color-coded interface
4. Supporting emergency vehicle prioritization

## Features
* **Traffic Data Collection**: Wireless sensors detect vehicle counts and emergencies
* **Adaptive Traffic Control**: Traffic lights adjust based on real-time traffic conditions
* **Emergency Vehicle Priority**: Special handling for emergency situations
* **Dynamic Visualization**: Color-coded visual representation of traffic conditions
* **Configurable Simulation**: Adjustable speed and parameters for different scenarios

## Project Structure
```
TS&A/
├── traffic_sensor_data.csv        # Generated traffic data
├── visualize_traffic_enhanced.py  # Enhanced visualization script
├── visualize_traffic.py           # Basic visualization script
├── ns3.43 setup/                  # NS-3 implementation files
│   ├── run-simulation.sh          # Main script to run the simulation
│   ├── sumo-ns3-integration.cc    # Integration between NS-3 and SUMO
│   └── wsn-implementation.cc      # Wireless sensor network implementation
└── sumo setup/                    # SUMO configuration files
    ├── example.sumocfg           # SUMO configuration
    ├── example.net.xml           # Road network definition
    └── various other SUMO files  # Additional configuration files
```

## Prerequisites
* Ubuntu Linux environment
* NS-3.43 installed and configured
* SUMO Traffic Simulator (latest version recommended)
* Python 3.x with TraCI package
* C++ development tools

## Installation
1. Clone this repository:
```bash
git clone https://github.com/yourusername/traffic-wsn-simulation.git
cd traffic-wsn-simulation
```

2. Install NS-3.43:
```bash
# Download and extract NS-3.43
wget https://www.nsnam.org/releases/ns-allinone-3.43.tar.bz2
tar xjf ns-allinone-3.43.tar.bz2
cd ns-allinone-3.43/ns-3.43
./waf configure --enable-examples
./waf build
```

3. Install SUMO:
```bash
sudo add-apt-repository ppa:sumo/stable
sudo apt update
sudo apt install sumo sumo-tools sumo-doc
```

4. Install Python dependencies:
```bash
pip install traci matplotlib numpy pandas
```

5. Copy project files to the correct locations:
```bash
# Copy NS-3 integration files
cp ../../ns3.43\ setup/* scratch/

# Set up SUMO configuration
export SUMO_HOME=/usr/share/sumo
```

6. Ensure NS-3.43 is installed:
```bash
cd ns-allinone-3.43/ns-3.43
./waf configure
./waf build
```

7. Ensure SUMO is installed:
```bash
sudo apt install sumo sumo-tools sumo-doc
echo 'export SUMO_HOME="/usr/share/sumo"' >> ~/.bashrc
source ~/.bashrc
```

## Usage
Run the complete simulation with:
```bash
cd /path/to/TS\&A/ns3.43\ setup/
./run-simulation.sh
```

Options:
* Control simulation speed: `run-simulation.sh --speed=normal`
   * Available speeds: slow, normal, fast, very-fast

You can also:
1. Start the simulation:
```bash
cd ns-allinone-3.43/ns-3.43
./scratch/run-simulation.sh
```

2. Visualize the traffic data:
```bash
python visualize_traffic_enhanced.py
```

3. To run with custom parameters:
```bash
./scratch/run-simulation.sh --duration=3600 --sensors=24 --emergency-freq=0.05
```

## Components
### 1. Wireless Sensor Network (WSN)
The system uses three types of nodes:
   * **RFD (Reduced Function Devices)**: Traffic sensors placed near roads
   * **FFD (Full Function Devices)**: Routers that collect data from RFDs
   * **FPC (Full PAN Coordinator)**: Central node that processes all data

These nodes communicate using LR-WPAN (IEEE 802.15.4) and collect:
   * Vehicle counts at junctions
   * Emergency vehicle presence
   * Traffic density information

The WSN component simulates sensor nodes deployed at traffic intersections. These sensors:
- Collect data on vehicle counts, speeds, and density
- Detect emergency vehicles through priority signaling
- Communicate wirelessly with central traffic management system
- Operate on limited power with energy conservation protocols

### 2. SUMO Traffic Simulation
![image](https://github.com/user-attachments/assets/203829df-ad08-43ca-bf67-91ad68fad3dc)

The traffic simulation includes:
   * Road networks with multiple junctions
   * Traffic lights with configurable timing
   * Vehicle routes and traffic patterns
   * TraCI interface for external control

### 3. Integration Layer
The NS-3 and SUMO simulators are integrated through:
   * TraCI (Traffic Control Interface) for SUMO control
   * Data exchange between simulations
   * Adaptive traffic light control algorithms

### 4. Traffic Control System
- Processes data from sensor networks in real-time
- Implements adaptive traffic light control algorithms
- Prioritizes emergency vehicle routing
- Optimizes overall traffic flow based on current conditions

### 5. Visualization Module
- Displays traffic density with color-coded representations:
   * **Green**: Normal traffic conditions
   * **Cyan**: Light traffic
   * **Orange**: Heavy traffic
   * **Red**: Emergency situation
- Shows sensor node status and network connectivity
- Provides real-time statistics on traffic flow efficiency
- Compares adaptive vs. static traffic light performance

## Simulation Workflow
1. **WSN Simulation**:
   * NS-3 simulates sensor nodes collecting traffic data
   * Data is saved to `traffic_sensor_data.csv`
2. **Traffic Control Integration**:
   * Traffic data is read from CSV
   * Traffic light timings are adjusted based on vehicle counts and emergency flags
3. **SUMO Visualization**:
   * SUMO GUI displays the traffic simulation
   * Junction colors indicate traffic conditions
   * Traffic lights adapt to the changing conditions

The complete workflow includes:
1. **Initialization**: Set up NS-3 network topology and SUMO traffic environment
2. **Sensor Deployment**: Place wireless sensors at key intersections
3. **Data Collection**: Sensors gather traffic metrics and transmit via wireless network
4. **Decision Making**: Central system analyzes data and makes traffic control decisions
5. **Traffic Management**: Signals are adjusted through TraCI commands to SUMO
6. **Visualization**: Results displayed through the visualization interface
7. **Data Analysis**: Performance metrics are collected and analyzed

## Customization
The simulation framework supports various customization options:

### Modify Sensor Placement
Edit the mobility model in wsn-implementation.cc to change sensor positions.

### Adjust Traffic Control Logic
Modify the AdjustTrafficLights() function in sumo-ns3-integration.cc to implement different control strategies.

### Change Simulation Parameters
Edit the simulation parameters in run-simulation.sh to test different scenarios:
   * Simulation time
   * Data collection intervals
   * Traffic density thresholds

### Additional Customization Options
- Modify `example.net.xml` to change road network layout
- Adjust sensor parameters in `wsn-implementation.cc`
- Change traffic patterns in SUMO configuration files
- Implement new traffic control algorithms by modifying `sumo-ns3-integration.cc`
- Customize visualization by editing Python visualization scripts

## Troubleshooting
### Common Issues
- **NS-3 Build Errors**: Ensure all dependencies are installed with `apt install build-essential libsqlite3-dev python3-dev python3-pip`
- **SUMO Connection Failures**: Check SUMO is running and `SUMO_HOME` environment variable is correctly set
- **Visualization Issues**: Verify Python dependencies and check CSV data format
- **Performance Problems**: Adjust simulation step time or reduce network complexity
- **NS-3 Compilation Errors**: Ensure all required modules are installed and properly configured
- **SUMO Connection Issues**: Verify that SUMO is correctly installed and that the configuration files exist
- **TraCI Errors**: Check that the TraCI port (8813) is not in use by another application
- **Visualization Problems**: Ensure the paths to SUMO configuration files are correct

### Debug Tips
- Use NS-3 logging system with `NS_LOG="WSNTraffic=level_all"` environment variable
- Check generated traffic data file for anomalies
- Examine SUMO GUI for traffic flow visualization during simulation
- For wireless connectivity issues, check signal propagation parameters

For more details, refer to the [NS-3 documentation](https://www.nsnam.org/docs/release/3.43/tutorial/html/index.html) and [SUMO documentation](https://sumo.dlr.de/docs/).

---
