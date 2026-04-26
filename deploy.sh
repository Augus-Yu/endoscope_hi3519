#!/bin/bash
#================================================================================
# Hi3519AV100 Endoscope UI Deployment Script
#================================================================================
# This script deploys the compiled binary to the Hi3519AV100 target board
# using SCP (Secure Copy Protocol).
#
# IMPORTANT: Configure the TARGET_IP and TARGET_USER variables below
# to match your target board's network settings.
#================================================================================

set -e

#--------------------------------------------------------------------------------
# Configuration - MODIFY THESE FOR YOUR TARGET
#--------------------------------------------------------------------------------

# Target board network configuration
TARGET_IP="192.168.1.100"           # IP address of the Hi3519 board
TARGET_USER="root"                   # Username for SSH/SCP
TARGET_PASSWORD=""                   # Leave empty to use SSH key authentication

# Target paths
TARGET_BINARY_PATH="/usr/bin/endoscope_ui"    # Where to place the binary on target
TARGET_LIB_PATH="/usr/lib"                    # Where MPP libraries are located
TARGET_WORK_DIR="/tmp/endoscope_ui"           # Working directory for resources

# Local paths (override via environment: HI3519_MPP_LIBS_DIR)
LOCAL_BINARY="bin/endoscope_ui"
LOCAL_MPP_LIBS="${HI3519_MPP_LIBS_DIR:-/home/ydy/Hi3519AV100_SDK_V2.0.2.0/smp/a53_linux/mpp/lib}"

# Resource files to deploy (fonts, lang, images)
RESOURCE_DIRS="lang fonts images"

#--------------------------------------------------------------------------------
# Script Configuration
#--------------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

#--------------------------------------------------------------------------------
# Helper Functions
#--------------------------------------------------------------------------------
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

#--------------------------------------------------------------------------------
# Pre-Deployment Checks
#--------------------------------------------------------------------------------
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    # Check if binary exists
    if [ ! -f "$LOCAL_BINARY" ]; then
        log_error "Binary not found: $LOCAL_BINARY"
        log_info "Please run ./build.sh first to build the project."
        exit 1
    fi
    
    log_success "Binary found: $LOCAL_BINARY"
    
    # Check binary type
    if command -v file &> /dev/null; then
        local file_type=$(file "$LOCAL_BINARY")
        if [[ ! "$file_type" == *"ARM"* ]]; then
            log_warn "Binary may not be ARM executable. Type: $file_type"
        else
            log_success "Binary is ARM executable"
        fi
    fi
    
    # Check for SSH/SCP
    if ! command -v scp &> /dev/null; then
        log_error "scp command not found. Please install OpenSSH client."
        exit 1
    fi
    
    if ! command -v ssh &> /dev/null; then
        log_error "ssh command not found. Please install OpenSSH client."
        exit 1
    fi
    
    log_success "Prerequisites check passed"
}

#--------------------------------------------------------------------------------
# Target Connection Test
#--------------------------------------------------------------------------------
test_connection() {
    log_info "Testing connection to target $TARGET_IP..."
    
    if ! ping -c 1 -W 2 "$TARGET_IP" > /dev/null 2>&1; then
        log_error "Cannot reach target at $TARGET_IP"
        log_info "Please check:"
        log_info "  1. Target board is powered on"
        log_info "  2. Network cable is connected"
        log_info "  3. IP address is correct (current: $TARGET_IP)"
        exit 1
    fi
    
    log_success "Target is reachable"
    
    # Test SSH connection
    log_info "Testing SSH connection..."
    if ! ssh -o ConnectTimeout=5 -o BatchMode=yes "$TARGET_USER@$TARGET_IP" "echo 'SSH OK'" > /dev/null 2>&1; then
        log_warn "SSH connection test failed"
        log_info "You may need to:"
        log_info "  1. Set up SSH key authentication"
        log_info "  2. Enter password when prompted"
        log_info "  3. Check that target has SSH server running"
    else
        log_success "SSH connection successful"
    fi
}

#--------------------------------------------------------------------------------
# Deployment Functions
#--------------------------------------------------------------------------------
deploy_binary() {
    log_info "Deploying binary to target..."
    
    # Create target directory if needed
    ssh "$TARGET_USER@$TARGET_IP" "mkdir -p $(dirname $TARGET_BINARY_PATH)"
    
    # Copy binary
    log_info "Copying $LOCAL_BINARY to $TARGET_USER@$TARGET_IP:$TARGET_BINARY_PATH"
    scp "$LOCAL_BINARY" "$TARGET_USER@$TARGET_IP:$TARGET_BINARY_PATH"
    
    # Make executable
    ssh "$TARGET_USER@$TARGET_IP" "chmod +x $TARGET_BINARY_PATH"
    
    log_success "Binary deployed successfully"
}

deploy_libraries() {
    log_info "Checking MPP libraries on target..."
    
    # Check which MPP libraries need to be deployed
    local libs_to_deploy=""
    for lib in mpi hifb securec; do
        local lib_file="lib${lib}.so"
        # Check if library exists on target
        if ! ssh "$TARGET_USER@$TARGET_IP" "ls ${TARGET_LIB_PATH}/${lib_file}*" > /dev/null 2>&1; then
            log_warn "Library $lib_file not found on target"
            if [ -f "${LOCAL_MPP_LIBS}/${lib_file}" ]; then
                libs_to_deploy="$libs_to_deploy ${LOCAL_MPP_LIBS}/${lib_file}"
            fi
        fi
    done
    
    if [ -n "$libs_to_deploy" ]; then
        log_info "Deploying MPP libraries..."
        for lib in $libs_to_deploy; do
            log_info "Copying $(basename $lib)..."
            scp "$lib" "$TARGET_USER@$TARGET_IP:$TARGET_LIB_PATH/"
        done
        
        # Update library cache
        ssh "$TARGET_USER@$TARGET_IP" "ldconfig" &> /dev/null || true
        
        log_success "Libraries deployed"
    else
        log_info "All required libraries already present on target"
    fi
}

deploy_resources() {
    if [ -z "$RESOURCE_DIRS" ]; then
        return
    fi
    
    log_info "Deploying resource files..."
    
    # Create working directory
    ssh "$TARGET_USER@$TARGET_IP" "mkdir -p $TARGET_WORK_DIR"
    
    # Copy resources
    for dir in $RESOURCE_DIRS; do
        if [ -d "$dir" ]; then
            log_info "Copying $dir..."
            scp -r "$dir" "$TARGET_USER@$TARGET_IP:$TARGET_WORK_DIR/"
        fi
    done
    
    log_success "Resources deployed"
}

#--------------------------------------------------------------------------------
# Post-Deployment
#--------------------------------------------------------------------------------
verify_deployment() {
    log_info "Verifying deployment..."
    
    # Check binary on target
    if ssh "$TARGET_USER@$TARGET_IP" "test -x $TARGET_BINARY_PATH"; then
        log_success "Binary is executable on target"
    else
        log_error "Binary verification failed"
        exit 1
    fi
    
    # Get binary size on target
    local target_size=$(ssh "$TARGET_USER@$TARGET_IP" "du -h $TARGET_BINARY_PATH | cut -f1")
    log_info "Binary size on target: $target_size"
}

#--------------------------------------------------------------------------------
# Run on Target
#--------------------------------------------------------------------------------
run_on_target() {
    echo ""
    log_info "Starting endoscope_ui on target..."
    echo "============================================================"
    
    # Kill any existing instance
    ssh "$TARGET_USER@$TARGET_IP" "pkill -9 endoscope_ui" &> /dev/null || true
    sleep 1
    
    # Set up environment and run
    local cmd="cd $TARGET_WORK_DIR && export LD_LIBRARY_PATH=$TARGET_LIB_PATH:\$LD_LIBRARY_PATH && $TARGET_BINARY_PATH"
    
    log_info "Running: $cmd"
    echo "------------------------------------------------------------"
    
    # Run with output
    ssh -t "$TARGET_USER@$TARGET_IP" "$cmd"
}

#--------------------------------------------------------------------------------
# Configuration Helper
#--------------------------------------------------------------------------------
edit_config() {
    echo "Current configuration:"
    echo "  TARGET_IP: $TARGET_IP"
    echo "  TARGET_USER: $TARGET_USER"
    echo "  TARGET_BINARY_PATH: $TARGET_BINARY_PATH"
    echo ""
    echo "To change configuration, edit this script or set environment variables:"
    echo "  export HI3519_TARGET_IP=192.168.1.xxx"
    echo "  export HI3519_TARGET_USER=root"
}

#--------------------------------------------------------------------------------
# Main Script
#--------------------------------------------------------------------------------
main() {
    echo "============================================================"
    echo "  Hi3519AV100 Endoscope UI Deployment Script"
    echo "============================================================"
    echo ""
    
    # Override config with environment variables if set
    TARGET_IP="${HI3519_TARGET_IP:-$TARGET_IP}"
    TARGET_USER="${HI3519_TARGET_USER:-$TARGET_USER}"
    
    # Parse arguments
    local run_after_deploy=false
    local skip_libs=false
    local config_only=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            -r|--run)
                run_after_deploy=true
                shift
                ;;
            --skip-libs)
                skip_libs=true
                shift
                ;;
            --config)
                config_only=true
                shift
                ;;
            -h|--help)
                echo "Usage: $0 [options]"
                echo ""
                echo "Options:"
                echo "  -r, --run       Run the application after deployment"
                echo "  --skip-libs     Skip library deployment"
                echo "  --config        Show current configuration"
                echo "  -h, --help      Show this help message"
                echo ""
                echo "Environment variables:"
                echo "  HI3519_TARGET_IP     Target board IP address"
                echo "  HI3519_TARGET_USER   Target board username"
                echo ""
                echo "Example:"
                echo "  HI3519_TARGET_IP=192.168.1.50 ./deploy.sh -r"
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                exit 1
                ;;
        esac
    done
    
    if [ "$config_only" = true ]; then
        edit_config
        exit 0
    fi
    
    log_info "Target: $TARGET_USER@$TARGET_IP"
    
    # Run deployment steps
    check_prerequisites
    test_connection
    deploy_binary
    
    if [ "$skip_libs" = false ]; then
        deploy_libraries
    fi
    
    deploy_resources
    verify_deployment
    
    echo ""
    log_success "Deployment complete!"
    
    if [ "$run_after_deploy" = true ]; then
        run_on_target
    else
        echo ""
        echo "To run on target:"
        echo "  ssh $TARGET_USER@$TARGET_IP $TARGET_BINARY_PATH"
        echo ""
        echo "Or run this script with -r flag:"
        echo "  ./deploy.sh -r"
    fi
}

# Run main function
main "$@"
