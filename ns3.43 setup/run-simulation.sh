#!/bin/bash

# Script to run the integrated SUMO-NS3 simulation with visualization

# Configuration
NS3_DIR="/home/$USER/ns-allinone-3.43/ns-allinone-3.43/ns-3.43"  # Path to NS-3 directory
NS3_SETUP_DIR="/home/$USER/TS&A/ns3.43 setup"  # Path to your NS3 code
SUMO_CONFIG_DIR="/home/$USER/TS&A/sumo setup"  # Path to SUMO config directory
OUTPUT_DIR="/home/$USER/TS&A"  # Directory for all output files
SIM_SPEED="normal"  # Can be "slow", "normal", "fast", "very-fast"

# Process command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --speed=*)
      SIM_SPEED="${1#*=}"
      shift
      ;;
    *)
      shift
      ;;
  esac
done

# First, generate traffic sensor data from WSN simulation
cd $NS3_DIR || { echo "Failed to change to NS-3 directory. Check the path."; exit 1; }
echo "Running WSN simulation to generate traffic data..."

# Copy the implementation files to the NS3 scratch directory
cp "$NS3_SETUP_DIR/wsn-implementation.cc" "$NS3_DIR/scratch/"
echo "Copied simulation files to NS3 scratch directory"

# Build the NS3 WSN project
if [ -f "./ns3" ]; then
  ./ns3 build scratch/wsn-implementation
elif [ -f "./waf" ]; then
  ./waf build scratch/wsn-implementation
fi

# Run the WSN simulation (this part works)
if [ -f "./ns3" ]; then
  ./ns3 run "scratch/wsn-implementation --simTime=100"
elif [ -f "./waf" ]; then
  ./waf --run "scratch/wsn-implementation --simTime=100"
else
  echo "Could not find NS-3 build system. Please check your NS-3 installation."
  exit 1
fi

# Move the traffic_sensor_data.csv file to the OUTPUT_DIR if it's not already there
if [ -f "$NS3_DIR/traffic_sensor_data.csv" ] && [ "$NS3_DIR/traffic_sensor_data.csv" != "$OUTPUT_DIR/traffic_sensor_data.csv" ]; then
  mv "$NS3_DIR/traffic_sensor_data.csv" "$OUTPUT_DIR/"
  echo "Moved traffic sensor data to $OUTPUT_DIR"
fi

# Create an enhanced visualization script if it doesn't exist
cat > "$OUTPUT_DIR/visualize_traffic_enhanced.py" << 'EOF'
#!/usr/bin/env python3

import os
import sys
import time
import traci
import subprocess
import csv
import random
import argparse

def read_traffic_data(file_path):
    data = {}
    with open(file_path, 'r') as f:
        next(f)  # Skip header
        for line in f:
            parts = line.strip().split(',')
            if len(parts) >= 3:
                time_stamp = float(parts[0])
                node_id = int(parts[1])
                count = int(parts[2])
                emergency = False
                if len(parts) >= 4 and parts[3].strip():
                    emergency = True
                
                if time_stamp not in data:
                    data[time_stamp] = {}
                data[time_stamp][f'J{node_id}'] = {'count': count, 'emergency': emergency}
    return data

def main():
    # Parse command line arguments
    parser = argparse.ArgumentParser(description='SUMO Traffic Visualization with TraCI')
    parser.add_argument('--sumo-config', default="/home/vijay/TS&A/sumo setup/example.sumocfg", 
                        help='Path to SUMO config file')
    parser.add_argument('--traffic-data', default="/home/vijay/TS&A/traffic_sensor_data.csv",
                        help='Path to traffic sensor data file')
    parser.add_argument('--speed', choices=['slow', 'normal', 'fast', 'very-fast'], default='normal',
                        help='Simulation speed (default: normal)')
    parser.add_argument('--verbose', action='store_true',
                        help='Show detailed output')
    args = parser.parse_args()
    
    # Set delay based on speed setting
    if args.speed == 'slow':
        step_delay = 0.5
    elif args.speed == 'normal':
        step_delay = 0.2
    elif args.speed == 'fast':
        step_delay = 0.05
    elif args.speed == 'very-fast':
        step_delay = 0.01
    
    # Start SUMO with GUI
    sumo_cmd = ["sumo-gui", "-c", args.sumo_config, "--start", "--remote-port", "8813"]
    if args.speed == 'very-fast':
        sumo_cmd.append("--step-length")
        sumo_cmd.append("0.05")  # Faster SUMO steps for very fast mode
    
    print(f"Starting SUMO: {' '.join(sumo_cmd)}")
    
    try:
        sumo_process = subprocess.Popen(sumo_cmd)
        time.sleep(2)  # Give SUMO time to start
        
        # Connect to SUMO via TraCI
        traci.init(port=8813)
        print("Connected to SUMO via TraCI")
        
        # Read traffic data
        traffic_data = read_traffic_data(args.traffic_data)
        time_points = sorted(traffic_data.keys())
        
        print(f"Loaded {len(time_points)} time points of traffic data")
        print(f"Simulation speed: {args.speed} (delay: {step_delay:.3f}s per step)")
        
        # Set up colors for visualization
        colors = {
            'NORMAL': (0, 255, 0),   # Green
            'HEAVY': (255, 165, 0),   # Orange
            'LIGHT': (0, 255, 255),  # Cyan
            'EMERGENCY': (255, 0, 0)  # Red
        }
        
        # Add vehicles to represent traffic state
        vehicles_added = []
        
        # Process each time point
        for t in time_points:
            # Delay based on selected speed
            time.sleep(step_delay)
            
            # Get traffic light IDs
            tls_ids = traci.trafficlight.getIDList()
            
            for tls_id in tls_ids:
                # Map traffic light to junction
                junction = tls_id.replace('TL', 'J')
                
                # Get vehicle count and emergency status for this junction
                junction_data = traffic_data[t].get(junction, {'count': 0, 'emergency': False})
                vehicle_count = junction_data['count']
                is_emergency = junction_data['emergency']
                
                # Print status if verbose mode enabled
                status_text = ""
                if is_emergency:
                    status_text = "[EMERGENCY]"
                elif vehicle_count > 8:
                    status_text = "[HEAVY TRAFFIC]"
                elif vehicle_count < 3:
                    status_text = "[LIGHT TRAFFIC]"
                else:
                    status_text = "[NORMAL TRAFFIC]"
                
                if args.verbose:
                    print(f"Time {t:.1f}: Junction {junction} has {vehicle_count} vehicles {status_text}")
                
                # Adjust traffic lights
                if is_emergency:
                    if args.verbose:
                        print(f"  EMERGENCY at {tls_id}, setting emergency program")
                    try:
                        # Rapid phase changes for emergency response
                        traci.trafficlight.setPhase(tls_id, 0)  # Set to first phase
                        traci.trafficlight.setPhaseDuration(tls_id, 3)  # Short duration
                        
                        # Visual indicator - change junction color to red
                        for edge_id in traci.trafficlight.getControlledLinks(tls_id):
                            if edge_id and len(edge_id) > 0 and edge_id[0]:
                                for lane_id in traci.edge.getLaneIDs(edge_id[0][0]):
                                    traci.lane.setColor(lane_id, colors['EMERGENCY'])
                    except Exception as e:
                        if args.verbose:
                            print(f"Error adjusting {tls_id}: {e}")
                
                elif vehicle_count > 8:
                    # Heavy traffic - extend green phase
                    if args.verbose:
                        print(f"  Heavy traffic at {tls_id}, extending green phase")
                    try:
                        current_phase = traci.trafficlight.getPhase(tls_id)
                        if current_phase % 2 == 0:  # Assuming even phases are green
                            traci.trafficlight.setPhaseDuration(tls_id, 10)
                            
                        # Visual indicator - change junction color to orange
                        for edge_id in traci.trafficlight.getControlledLinks(tls_id):
                            if edge_id and len(edge_id) > 0 and edge_id[0]:
                                for lane_id in traci.edge.getLaneIDs(edge_id[0][0]):
                                    traci.lane.setColor(lane_id, colors['HEAVY'])
                    except Exception as e:
                        if args.verbose:
                            print(f"Error adjusting {tls_id}: {e}")
                        
                elif vehicle_count < 3:
                    # Light traffic - shorter green phase
                    if args.verbose:
                        print(f"  Light traffic at {tls_id}, shortening green phase")
                    try:
                        current_phase = traci.trafficlight.getPhase(tls_id)
                        if current_phase % 2 == 0:  # Assuming even phases are green
                            traci.trafficlight.setPhaseDuration(tls_id, 3)
                            
                        # Visual indicator - change junction color to cyan
                        for edge_id in traci.trafficlight.getControlledLinks(tls_id):
                            if edge_id and len(edge_id) > 0 and edge_id[0]:
                                for lane_id in traci.edge.getLaneIDs(edge_id[0][0]):
                                    traci.lane.setColor(lane_id, colors['LIGHT'])
                    except Exception as e:
                        if args.verbose:
                            print(f"Error adjusting {tls_id}: {e}")
                else:
                    # Normal traffic
                    try:
                        # Visual indicator - change junction color to green
                        for edge_id in traci.trafficlight.getControlledLinks(tls_id):
                            if edge_id and len(edge_id) > 0 and edge_id[0]:
                                for lane_id in traci.edge.getLaneIDs(edge_id[0][0]):
                                    traci.lane.setColor(lane_id, colors['NORMAL'])
                    except Exception as e:
                        if args.verbose:
                            print(f"Error setting colors: {e}")
                
                # Dynamically add/remove vehicles to visualize traffic density
                try:
                    if args.speed != 'very-fast':  # Skip vehicle management in very-fast mode
                        # Add vehicles proportional to the count
                        edge_ids = []
                        for edge_id in traci.trafficlight.getControlledLinks(tls_id):
                            if edge_id and len(edge_id) > 0 and edge_id[0]:
                                edge_ids.append(edge_id[0][0])
                        
                        if edge_ids:
                            # Add vehicles for visualization
                            vehicles_to_add = max(0, vehicle_count - len(vehicles_added))
                            for i in range(vehicles_to_add):
                                veh_id = f"veh_{len(vehicles_added)}"
                                edge = random.choice(edge_ids)
                                try:
                                    traci.vehicle.add(veh_id, "route_0", typeID="passenger", depart=t)
                                    vehicles_added.append(veh_id)
                                except:
                                    pass
                except Exception as e:
                    if args.verbose:
                        print(f"Error managing vehicles: {e}")
            
            traci.simulationStep()
            
            # Print progress indicator (without flooding the console)
            if int(t) % 10 == 0:
                print(f"Simulation time: {t:.1f}s", end="\r")
        
        print("\nVisualization complete")
        if args.speed != 'very-fast':
            time.sleep(3)  # Keep SUMO open briefly to see the final state
        traci.close()
        
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        try:
            sumo_process.terminate()
        except:
            pass
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
EOF

chmod +x "$OUTPUT_DIR/visualize_traffic_enhanced.py"
echo "Created enhanced visualization script"

# Run the visualization automatically with the specified speed
echo "Starting traffic visualization with speed: $SIM_SPEED..."
cd "$OUTPUT_DIR"
python3 "$OUTPUT_DIR/visualize_traffic_enhanced.py" --sumo-config="$SUMO_CONFIG_DIR/example.sumocfg" --traffic-data="$OUTPUT_DIR/traffic_sensor_data.csv" --speed="$SIM_SPEED"

echo "Simulation and visualization completed."