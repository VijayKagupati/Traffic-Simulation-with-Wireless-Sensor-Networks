#!/bin/bash

# Create the network file from nodes, edges, and connections
netconvert --node-files=nodes.xml --edge-files=edges.xml --connection-files=connections.xml --output-file=example.net.xml --tls.discard-simple --tllogic-files=tll.xml

echo "Network file created successfully!"
echo "To run the simulation, use the command:"
echo "sumo-gui -c example.sumocfg"