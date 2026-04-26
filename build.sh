#!/bin/bash
#================================================================================
# Hi3519AV100 Endoscope UI Build Script
#================================================================================
# This script sets up the build environment and compiles the endoscope UI
# for the Hi3519AV100 platform using the arm-himix200-linux toolchain.
#================================================================================

set -e  # Exit on error

#--------------------------------------------------------------------------------
# Configuration
#--------------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_NAME="endoscope_ui"
BUILD_JOBS=${BUILD_JOBS:-4}  # Default to 4 parallel jobs, override with env var

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

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
# Environment Check
#--------------------------------------------------------------------------------
check_environment() {
    log_info "Checking build environment..."
    
    # Check if running from correct directory
    if [ ! -f "Makefile" ]; then
        log_error "Makefile not found. Please run this script from the endoscope_hi3519 directory."
        exit 1
    fi
    
    # Check for cross-compiler
    TOOLCHAIN_BASE="/opt/hisi-linux/x86-arm/arm-himix200-linux"
    CROSS_CC="${TOOLCHAIN_BASE}/bin/arm-himix200-linux-gcc"
    
    if [ ! -x "$CROSS_CC" ]; then
        log_error "Cross-compiler not found at: $CROSS_CC"
        log_info "Please install the Hi3519 toolchain to /opt/hisi-linux/x86-arm/arm-himix200-linux/"
        exit 1
    fi
    
    log_success "Cross-compiler found: $CROSS_CC"
    
    # Check for MPP SDK
    MPP_SDK="/home/ydy/Hi3519AV100_SDK_V2.0.2.0/smp/a53_linux/mpp"
    if [ ! -d "$MPP_SDK" ]; then
        log_error "MPP SDK not found at: $MPP_SDK"
        log_info "Please install the Hi3519AV100 SDK and update the path in this script."
        exit 1
    fi
    
    log_success "MPP SDK found: $MPP_SDK"
    
    # Check for required MPP libraries
    local libs_ok=true
    for lib in mpi hifb securec; do
        if [ ! -f "${MPP_SDK}/lib/lib${lib}.so" ] && [ ! -f "${MPP_SDK}/lib/lib${lib}.a" ]; then
            log_warn "MPP library lib${lib} not found (may be linked dynamically)"
            libs_ok=false
        fi
    done
    
    if [ "$libs_ok" = true ]; then
        log_success "All required MPP libraries found"
    fi
    
    # Check for LVGL
    if [ ! -d "../lvgl" ]; then
        log_error "LVGL directory not found at ../lvgl"
        log_info "Please ensure LVGL is checked out in the parent directory."
        exit 1
    fi
    
    log_success "LVGL found at ../lvgl"
    
    # Check for UI sources
    if [ ! -d "../main/src/endoscope_ui" ]; then
        log_warn "UI source directory not found at ../main/src/endoscope_ui"
    else
        local ui_files=$(find ../main/src/endoscope_ui -name "*.c" | wc -l)
        log_success "UI source files found: $ui_files files"
    fi
    
    log_success "Environment check complete!"
    echo ""
}

#--------------------------------------------------------------------------------
# Setup Functions
#--------------------------------------------------------------------------------
setup_directories() {
    log_info "Setting up build directories..."
    
    mkdir -p obj bin
    
    log_success "Directories created"
}

setup_lv_conf() {
    log_info "Setting up LVGL configuration..."
    
    if [ ! -f "lv_conf.h" ]; then
        if [ -f "../lv_conf.h" ]; then
            log_info "Copying lv_conf.h from parent directory..."
            cp ../lv_conf.h lv_conf.h
            
            # Modify for embedded use
            log_info "Applying embedded-specific modifications..."
            
            # Disable PC-specific features
            sed -i 's/#define LV_USE_SDL[[:space:]]*1/#define LV_USE_SDL 0/' lv_conf.h || true
            sed -i 's/#define LV_USE_LINUX_FBDEV[[:space:]]*0/#define LV_USE_LINUX_FBDEV 1/' lv_conf.h || true
            
            # Set appropriate refresh period for embedded
            sed -i 's/#define LV_DEF_REFR_PERIOD[[:space:]]*[0-9]*/#define LV_DEF_REFR_PERIOD 16/' lv_conf.h || true
            
            # Reduce memory for embedded
            sed -i 's/#define LV_MEM_SIZE[[:space:]]*(.*)/#define LV_MEM_SIZE (8 * 1024 * 1024)/' lv_conf.h || true
            
            log_success "lv_conf.h created with embedded settings"
        else
            log_warn "No lv_conf.h found in parent directory. You'll need to create one manually."
        fi
    else
        log_info "lv_conf.h already exists, skipping..."
    fi
}

#--------------------------------------------------------------------------------
# Build Functions
#--------------------------------------------------------------------------------
build_project() {
    log_info "Starting build..."
    log_info "Build jobs: $BUILD_JOBS"
    
    # Run make with specified number of jobs
    make -j$BUILD_JOBS "$@"
    
    log_success "Build completed successfully!"
}

#--------------------------------------------------------------------------------
# Post-Build Functions
#--------------------------------------------------------------------------------
show_results() {
    echo ""
    log_info "Build Results:"
    echo "============================================================"
    
    if [ -f "bin/${PROJECT_NAME}" ]; then
        local file_size=$(du -h "bin/${PROJECT_NAME}" | cut -f1)
        log_success "Binary created: bin/${PROJECT_NAME} ($file_size)"
        
        # Show file type information
        if command -v file &> /dev/null; then
            echo ""
            echo "File information:"
            file "bin/${PROJECT_NAME}"
        fi
        
        # Show binary size breakdown if size command available
        if command -v arm-himix200-linux-size &> /dev/null; then
            echo ""
            echo "Section sizes:"
            arm-himix200-linux-size "bin/${PROJECT_NAME}"
        fi
    else
        log_error "Binary not found: bin/${PROJECT_NAME}"
        exit 1
    fi
    
    echo "============================================================"
}

#--------------------------------------------------------------------------------
# Main Script
#--------------------------------------------------------------------------------
main() {
    echo "============================================================"
    echo "  Hi3519AV100 Endoscope UI Build Script"
    echo "============================================================"
    echo ""
    
    # Parse arguments
    local clean_first=false
    local verbose=false
    local check_only=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            -c|--clean)
                clean_first=true
                shift
                ;;
            -v|--verbose)
                verbose=true
                shift
                ;;
            -j|--jobs)
                BUILD_JOBS="$2"
                shift 2
                ;;
            --check)
                check_only=true
                shift
                ;;
            -h|--help)
                echo "Usage: $0 [options]"
                echo ""
                echo "Options:"
                echo "  -c, --clean      Clean before building"
                echo "  -v, --verbose    Verbose output"
                echo "  -j, --jobs N     Use N parallel jobs (default: 4)"
                echo "  --check          Only check environment, don't build"
                echo "  -h, --help       Show this help message"
                echo ""
                echo "Environment variables:"
                echo "  BUILD_JOBS       Number of parallel build jobs"
                exit 0
                ;;
            *)
                # Pass remaining arguments to make
                break
                ;;
        esac
    done
    
    # Change to script directory
    cd "$SCRIPT_DIR"
    
    # Run environment check
    check_environment
    
    if [ "$check_only" = true ]; then
        log_info "Environment check complete. Exiting."
        exit 0
    fi
    
    # Clean if requested
    if [ "$clean_first" = true ]; then
        log_info "Cleaning previous build..."
        make clean
    fi
    
    # Setup
    setup_directories
    setup_lv_conf
    
    # Build
    echo ""
    if [ "$verbose" = true ]; then
        build_project V=1 "$@"
    else
        build_project "$@"
    fi
    
    # Show results
    show_results
    
    echo ""
    log_success "All done! Binary is ready at: bin/${PROJECT_NAME}"
    echo ""
    echo "Next steps:"
    echo "  1. Strip binary:    make strip"
    echo "  2. Deploy:          ./deploy.sh"
    echo "  3. Run on target:   ./endoscope_ui"
}

# Run main function
main "$@"
