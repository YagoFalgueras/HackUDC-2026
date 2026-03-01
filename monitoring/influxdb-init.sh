#!/bin/bash
# InfluxDB initialization script for DOOM Satellite monitoring

echo "Initializing InfluxDB for DOOM Satellite monitoring..."

# Wait for InfluxDB to be ready
until curl -s http://localhost:8086/ping > /dev/null 2>&1; do
    echo "Waiting for InfluxDB to be ready..."
    sleep 2
done

# Create database if it doesn't exist
influx -execute "CREATE DATABASE doom_satellite"

# Create retention policy (keep data for 7 days)
influx -execute "CREATE RETENTION POLICY doom_rp ON doom_satellite DURATION 7d REPLICATION 1 DEFAULT"

echo "InfluxDB initialization complete!"
