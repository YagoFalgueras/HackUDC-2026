#!/usr/bin/env python3
"""
DOOM Satellite Network Traffic Monitor
Captures UDP traffic on ports 5000 (uplink) and 5001 (downlink)
and exports metrics to InfluxDB for Grafana visualization.
"""

import time
import signal
import sys
from datetime import datetime
from collections import defaultdict
from scapy.all import sniff, UDP, IP
from influxdb import InfluxDBClient
import threading
import argparse

# Configuration
UPLINK_PORT = 5000      # Input commands: Ground -> Satellite
DOWNLINK_PORT = 5001    # Video stream: Satellite -> Ground
INFLUXDB_HOST = 'localhost'
INFLUXDB_PORT = 8086
INFLUXDB_DB = 'doom_satellite'
SAMPLE_INTERVAL = 1.0   # seconds

# Global stats
class TrafficStats:
    def __init__(self):
        self.lock = threading.Lock()
        self.reset()

    def reset(self):
        with self.lock:
            self.uplink_bytes = 0
            self.uplink_packets = 0
            self.downlink_bytes = 0
            self.downlink_packets = 0
            self.uplink_total_bytes = 0
            self.uplink_total_packets = 0
            self.downlink_total_bytes = 0
            self.downlink_total_packets = 0

    def add_uplink(self, bytes_count):
        with self.lock:
            self.uplink_bytes += bytes_count
            self.uplink_packets += 1
            self.uplink_total_bytes += bytes_count
            self.uplink_total_packets += 1

    def add_downlink(self, bytes_count):
        with self.lock:
            self.downlink_bytes += bytes_count
            self.downlink_packets += 1
            self.downlink_total_bytes += bytes_count
            self.downlink_total_packets += 1

    def get_and_reset_interval(self):
        with self.lock:
            data = {
                'uplink_bytes': self.uplink_bytes,
                'uplink_packets': self.uplink_packets,
                'downlink_bytes': self.downlink_bytes,
                'downlink_packets': self.downlink_packets,
                'uplink_total_bytes': self.uplink_total_bytes,
                'uplink_total_packets': self.uplink_total_packets,
                'downlink_total_bytes': self.downlink_total_bytes,
                'downlink_total_packets': self.downlink_total_packets,
            }
            # Reset interval counters but keep totals
            self.uplink_bytes = 0
            self.uplink_packets = 0
            self.downlink_bytes = 0
            self.downlink_packets = 0
            return data

stats = TrafficStats()
running = True

def signal_handler(sig, frame):
    """Handle Ctrl+C gracefully"""
    global running
    print("\n[!] Stopping monitor...")
    running = False

def packet_callback(packet):
    """Process captured packets"""
    if not packet.haslayer(UDP) or not packet.haslayer(IP):
        return

    udp_layer = packet[UDP]
    ip_layer = packet[IP]

    # Total packet size (Ethernet frame size if available, else IP packet size)
    packet_size = len(packet)

    # Classify by direction:
    # - Uplink (Input): packets TO port 5000 (ground -> satellite)
    # - Downlink (Video): packets FROM port 5001 (satellite -> ground)
    if udp_layer.dport == UPLINK_PORT:
        stats.add_uplink(packet_size)
    elif udp_layer.sport == DOWNLINK_PORT:
        stats.add_downlink(packet_size)

def capture_traffic(interface):
    """Capture packets on specified interface"""
    print(f"[*] Starting packet capture on interface: {interface}")
    print(f"[*] Monitoring ports: {UPLINK_PORT} (uplink), {DOWNLINK_PORT} (downlink)")

    # BPF filter for both ports
    bpf_filter = f"udp and (port {UPLINK_PORT} or port {DOWNLINK_PORT})"

    try:
        sniff(
            iface=interface,
            filter=bpf_filter,
            prn=packet_callback,
            store=False,
            stop_filter=lambda x: not running
        )
    except PermissionError:
        print("[!] Error: Packet capture requires root privileges. Run with sudo.")
        sys.exit(1)
    except Exception as e:
        print(f"[!] Error during packet capture: {e}")
        sys.exit(1)

def export_to_influxdb(client, sample_interval):
    """Periodically export metrics to InfluxDB"""
    print(f"[*] Starting InfluxDB export (interval: {sample_interval}s)")

    while running:
        time.sleep(sample_interval)

        data = stats.get_and_reset_interval()

        # Calculate rates (per second)
        uplink_bps = data['uplink_bytes'] * 8 / sample_interval  # bits per second
        uplink_kbps = uplink_bps / 1000
        downlink_bps = data['downlink_bytes'] * 8 / sample_interval
        downlink_kbps = downlink_bps / 1000

        # Prepare InfluxDB data points
        json_body = [
            {
                "measurement": "bandwidth",
                "tags": {
                    "direction": "uplink",
                    "port": str(UPLINK_PORT)
                },
                "time": datetime.utcnow().isoformat(),
                "fields": {
                    "bytes_per_sec": float(data['uplink_bytes'] / sample_interval),
                    "kbps": float(uplink_kbps),
                    "packets_per_sec": float(data['uplink_packets'] / sample_interval),
                    "total_bytes": float(data['uplink_total_bytes']),
                    "total_packets": int(data['uplink_total_packets'])
                }
            },
            {
                "measurement": "bandwidth",
                "tags": {
                    "direction": "downlink",
                    "port": str(DOWNLINK_PORT)
                },
                "time": datetime.utcnow().isoformat(),
                "fields": {
                    "bytes_per_sec": float(data['downlink_bytes'] / sample_interval),
                    "kbps": float(downlink_kbps),
                    "packets_per_sec": float(data['downlink_packets'] / sample_interval),
                    "total_bytes": float(data['downlink_total_bytes']),
                    "total_packets": int(data['downlink_total_packets'])
                }
            }
        ]

        # Write to InfluxDB
        try:
            client.write_points(json_body)

            # Console output
            print(f"\r[{datetime.now().strftime('%H:%M:%S')}] "
                  f"↑ {uplink_kbps:6.2f} kbps ({data['uplink_packets']:3d} pkt/s) | "
                  f"↓ {downlink_kbps:7.2f} kbps ({data['downlink_packets']:4d} pkt/s)",
                  end='', flush=True)
        except Exception as e:
            print(f"\n[!] Error writing to InfluxDB: {e}")

def main():
    parser = argparse.ArgumentParser(description='DOOM Satellite Network Monitor')
    parser.add_argument('-i', '--interface', default='lo',
                        help='Network interface to monitor (default: lo)')
    parser.add_argument('--influxdb-host', default=INFLUXDB_HOST,
                        help=f'InfluxDB host (default: {INFLUXDB_HOST})')
    parser.add_argument('--influxdb-port', type=int, default=INFLUXDB_PORT,
                        help=f'InfluxDB port (default: {INFLUXDB_PORT})')
    parser.add_argument('--interval', type=float, default=SAMPLE_INTERVAL,
                        help=f'Sampling interval in seconds (default: {SAMPLE_INTERVAL})')

    args = parser.parse_args()

    # Setup signal handler
    signal.signal(signal.SIGINT, signal_handler)

    # Connect to InfluxDB
    print(f"[*] Connecting to InfluxDB at {args.influxdb_host}:{args.influxdb_port}")
    try:
        client = InfluxDBClient(
            host=args.influxdb_host,
            port=args.influxdb_port,
            database=INFLUXDB_DB
        )
        # Test connection and create database if needed
        client.create_database(INFLUXDB_DB)
        print(f"[+] Connected to InfluxDB (database: {INFLUXDB_DB})")
    except Exception as e:
        print(f"[!] Error connecting to InfluxDB: {e}")
        print("[!] Make sure InfluxDB is running (docker-compose up -d)")
        sys.exit(1)

    # Start InfluxDB export thread
    export_thread = threading.Thread(target=export_to_influxdb, args=(client, args.interval), daemon=True)
    export_thread.start()

    # Start packet capture (blocking)
    capture_traffic(args.interface)

    # Wait for export thread to finish
    export_thread.join(timeout=2)

    # Final stats
    final_stats = stats.get_and_reset_interval()
    print("\n\n[*] Final Statistics:")
    print(f"    Uplink (port {UPLINK_PORT}):")
    print(f"      Total bytes:   {final_stats['uplink_total_bytes']:,}")
    print(f"      Total packets: {final_stats['uplink_total_packets']:,}")
    print(f"    Downlink (port {DOWNLINK_PORT}):")
    print(f"      Total bytes:   {final_stats['downlink_total_bytes']:,}")
    print(f"      Total packets: {final_stats['downlink_total_packets']:,}")
    print("\n[+] Monitor stopped.")

if __name__ == '__main__':
    main()
