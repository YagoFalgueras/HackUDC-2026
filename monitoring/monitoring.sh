#!/bin/bash
# DOOM Satellite Network Monitoring Management Script
# Gestiona entorno virtual Python, dependencias, Docker stack y monitoreo

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
VENV_DIR="venv"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MONITOR_SCRIPT="monitor.py"
INTERFACE="${MONITOR_INTERFACE:-lo}"

# Detect docker compose command
get_compose_cmd() {
    if docker compose version &> /dev/null 2>&1; then
        echo "docker compose"
    elif command -v docker-compose &> /dev/null; then
        echo "docker-compose"
    else
        return 1
    fi
}

COMPOSE_CMD=$(get_compose_cmd)

# Helper functions
print_success() {
    echo -e "${GREEN}[+]${NC} $1"
}

print_error() {
    echo -e "${RED}[!]${NC} $1"
}

print_info() {
    echo -e "${BLUE}[*]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

print_header() {
    echo -e "\n${BLUE}=========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}=========================================${NC}\n"
}

# Check if running in monitoring directory
check_directory() {
    if [ ! -f "$SCRIPT_DIR/requirements.txt" ]; then
        print_error "Must be run from the monitoring directory!"
        exit 1
    fi
}

# Setup Python virtual environment
setup_venv() {
    print_header "Setting up Python Virtual Environment"

    if [ -d "$VENV_DIR" ]; then
        print_info "Virtual environment already exists: $VENV_DIR"
    else
        print_info "Creating virtual environment..."
        python3 -m venv "$VENV_DIR"
        print_success "Virtual environment created: $VENV_DIR"
    fi
}

# Activate virtual environment
activate_venv() {
    if [ -f "$VENV_DIR/bin/activate" ]; then
        source "$VENV_DIR/bin/activate"
        print_success "Virtual environment activated"
    else
        print_error "Virtual environment not found. Run: $0 setup"
        exit 1
    fi
}

# Install Python dependencies
install_deps() {
    print_header "Installing Python Dependencies"

    activate_venv

    print_info "Upgrading pip..."
    pip install --upgrade pip

    print_info "Installing dependencies from requirements.txt..."
    pip install -r requirements.txt

    print_success "All dependencies installed successfully"

    print_info "\nInstalled packages:"
    pip list | grep -E "(influxdb|scapy|psutil)"
}

# Start Docker stack
start_docker() {
    print_header "Starting Docker Stack (Grafana + InfluxDB)"

    if ! command -v docker &> /dev/null; then
        print_error "Docker not found. Please install Docker first."
        exit 1
    fi

    # Check if docker compose is available
    if [ -z "$COMPOSE_CMD" ]; then
        print_error "Docker Compose not found. Please install Docker Compose."
        print_info "Install: https://docs.docker.com/compose/install/"
        exit 1
    fi

    print_info "Starting containers..."
    $COMPOSE_CMD up -d

    print_info "Waiting for services to be ready..."
    sleep 5

    print_success "Docker stack started successfully\n"

    $COMPOSE_CMD ps

    echo ""
    print_info "Access points:"
    echo "  Grafana:  http://localhost:3000 (admin/admin)"
    echo "  InfluxDB: http://localhost:8086"
}

# Stop Docker stack
stop_docker() {
    print_header "Stopping Docker Stack"

    print_info "Stopping containers..."
    $COMPOSE_CMD down

    print_success "Docker stack stopped"
}

# Run monitoring script
run_monitor() {
    print_header "Running Network Traffic Monitor"

    activate_venv

    if [ "$EUID" -ne 0 ]; then
        print_warning "Packet capture requires root privileges"
        print_info "Restarting with sudo...\n"
        sudo -E env PATH="$PATH" "$VENV_DIR/bin/python3" "$MONITOR_SCRIPT" -i "$INTERFACE" "$@"
    else
        python3 "$MONITOR_SCRIPT" -i "$INTERFACE" "$@"
    fi
}

# Check status of services
check_status() {
    print_header "Service Status"

    echo "Docker Containers:"
    $COMPOSE_CMD ps
    echo ""

    print_info "Checking InfluxDB..."
    if curl -s http://localhost:8086/ping > /dev/null 2>&1; then
        print_success "InfluxDB is running (port 8086)"
    else
        print_error "InfluxDB is not responding"
    fi

    print_info "Checking Grafana..."
    if curl -s http://localhost:3000/api/health 2>&1 | grep -q "ok"; then
        print_success "Grafana is running (port 3000)"
    else
        print_error "Grafana is not responding"
    fi

    echo ""
    if [ -d "$VENV_DIR" ]; then
        print_success "Python venv exists: $VENV_DIR"
        if [ -f "$VENV_DIR/bin/python3" ]; then
            VENV_PYTHON_VERSION=$("$VENV_DIR/bin/python3" --version)
            print_info "Python version: $VENV_PYTHON_VERSION"
        fi
    else
        print_warning "Python venv not created. Run: $0 setup"
    fi
}

# Show container logs
show_logs() {
    print_header "Docker Container Logs"
    $COMPOSE_CMD logs -f
}

# Clean all data
clean_data() {
    print_header "Cleaning Monitoring Data"

    print_warning "This will delete all monitoring data and volumes!"
    read -p "Are you sure? [y/N] " -n 1 -r
    echo

    if [[ $REPLY =~ ^[Yy]$ ]]; then
        print_info "Stopping containers and removing volumes..."
        $COMPOSE_CMD down -v
        print_success "All data cleaned"
    else
        print_info "Cancelled"
    fi
}

# Clean venv
clean_venv() {
    print_header "Cleaning Virtual Environment"

    if [ -d "$VENV_DIR" ]; then
        print_info "Removing virtual environment: $VENV_DIR"
        rm -rf "$VENV_DIR"
        print_success "Virtual environment removed"
    else
        print_info "No virtual environment found"
    fi
}

# Full setup (venv + deps + docker)
full_setup() {
    print_header "DOOM Satellite Monitoring - Full Setup"

    setup_venv
    install_deps
    start_docker

    print_success "\nSetup complete!"
    echo ""
    print_info "Next steps:"
    echo "  1. Run monitor:  $0 run"
    echo "  2. Open Grafana: $0 dashboard"
    echo "  3. Check status: $0 status"
}

# Open Grafana dashboard
open_dashboard() {
    print_info "Opening Grafana dashboard..."

    if command -v xdg-open &> /dev/null; then
        xdg-open http://localhost:3000
    elif command -v open &> /dev/null; then
        open http://localhost:3000
    else
        print_info "Please open http://localhost:3000 in your browser"
        print_info "Login: admin / admin"
    fi
}

# Show help
show_help() {
    cat << EOF
${BLUE}=========================================${NC}
${BLUE}DOOM Satellite - Network Monitoring${NC}
${BLUE}=========================================${NC}

Usage: $0 <command> [options]

${GREEN}Setup Commands:${NC}
  setup            - Complete setup (venv + deps + docker)
  venv             - Create Python virtual environment only
  install          - Install Python dependencies
  start            - Start Docker stack (Grafana + InfluxDB)

${GREEN}Runtime Commands:${NC}
  run              - Run network traffic monitor (requires sudo)
  stop             - Stop Docker stack
  status           - Show service status
  logs             - Show Docker container logs
  dashboard        - Open Grafana in browser

${GREEN}Cleanup Commands:${NC}
  clean            - Clean monitoring data and volumes
  clean-venv       - Remove Python virtual environment
  clean-all        - Clean everything (data + venv)

${GREEN}Environment Variables:${NC}
  MONITOR_INTERFACE - Network interface to monitor (default: lo)

${GREEN}Examples:${NC}
  $0 setup                    # Complete setup
  $0 run                      # Run monitor on default interface (lo)
  MONITOR_INTERFACE=eth0 $0 run  # Run monitor on eth0
  $0 status                   # Check if services are running
  $0 dashboard                # Open Grafana

${GREEN}Quick Start:${NC}
  1. $0 setup         # First time setup
  2. $0 run           # Start monitoring (in new terminal)
  3. $0 dashboard     # Open Grafana dashboard

${GREEN}Access Points:${NC}
  Grafana:  http://localhost:3000 (admin/admin)
  InfluxDB: http://localhost:8086

EOF
}

# Main command handler
main() {
    cd "$SCRIPT_DIR"

    case "${1:-help}" in
        setup)
            full_setup
            ;;
        venv)
            setup_venv
            ;;
        install)
            setup_venv
            install_deps
            ;;
        start)
            start_docker
            ;;
        stop)
            stop_docker
            ;;
        run)
            shift
            run_monitor "$@"
            ;;
        status)
            check_status
            ;;
        logs)
            show_logs
            ;;
        clean)
            clean_data
            ;;
        clean-venv)
            clean_venv
            ;;
        clean-all)
            clean_data
            clean_venv
            ;;
        dashboard)
            open_dashboard
            ;;
        help|--help|-h)
            show_help
            ;;
        *)
            print_error "Unknown command: $1"
            echo ""
            show_help
            exit 1
            ;;
    esac
}

# Run main
main "$@"
