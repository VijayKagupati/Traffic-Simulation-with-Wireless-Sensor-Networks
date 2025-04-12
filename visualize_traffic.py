#!/usr/bin/env python3

import os
import sys
import time
import traci
import subprocess
import csv

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
                if time_stamp not in data:
                    data[time_stamp] = {}
                data[time_stamp][f'J{node_id}'] = count
    return data

def main():
    traffic_data_file = "/home/vijay/TS&A/traffic_sensor_data.csv"
    sumo_config = "/home/vijay/TS&A/sumo setup/example.sumocfg"
    
    # Start SUMO with GUI
    sumo_cmd = ["sumo-gui", "-c", sumo_config, "--start"]
    print(f"Starting SUMO: {' '.join(sumo_cmd)}")
    
    try:
        sumo_process = subprocess.Popen(sumo_cmd)
        time.sleep(2)  # Give SUMO time to start
        
        # Connect to SUMO via TraCI
        traci.init(port=8813)
        print("Connected to SUMO via TraCI")
        
        # Read traffic data
        traffic_data = read_traffic_data(traffic_data_file)
        time_points = sorted(traffic_data.keys())
        
        print(f"Loaded {len(time_points)} time points of traffic data")
        
        # Process each time point
        for t in time_points:
            time.sleep(0.5)  # Slow down visualization
            
            # Get traffic light IDs
            tls_ids = traci.trafficlight.getIDList()
            
            for tls_id in tls_ids:
                # Map traffic light to junction (simplistic mapping)
                junction = tls_id.replace('TL', 'J')
                
                # Get vehicle count for this junction
                vehicle_count = traffic_data[t].get(junction, 0)
                print(f"Time {t:.1f}: Junction {junction} has {vehicle_count} vehicles")
                
                if vehicle_count > 8:
                    # Heavy traffic - extend green phase
                    print(f"  Heavy traffic at {tls_id}, extending green phase")
                    try:
                        current_phase = traci.trafficlight.getPhase(tls_id)
                        if current_phase % 2 == 0:  # Assuming even phases are green
                            traci.trafficlight.setPhaseDuration(tls_id, 10)
                    except Exception as e:
                        print(f"Error adjusting {tls_id}: {e}")
                elif vehicle_count < 3:
                    # Light traffic - shorter green phase
                    print(f"  Light traffic at {tls_id}, shortening green phase")
                    try:
                        current_phase = traci.trafficlight.getPhase(tls_id)
                        if current_phase % 2 == 0:
                            traci.trafficlight.setPhaseDuration(tls_id, 3)
                    except Exception as e:
                        print(f"Error adjusting {tls_id}: {e}")
            
            traci.simulationStep()
        
        print("Visualization complete")
        traci.close()
        
    except Exception as e:
        print(f"Error: {e}")
    finally:
        try:
            sumo_process.terminate()
        except:
            pass
    
    return 0

if __name__ == "__main__":
    sys.exit(main())