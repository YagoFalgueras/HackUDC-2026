#!/bin/bash

# Start satellite in background
./doom-sat 127.0.0.1 5000 5001 &
SAT_PID=$!

# Start ground station
./doom-ground 127.0.0.1 5001 5000

# Kill satellite when ground exits
kill $SAT_PID 2>/dev/null
