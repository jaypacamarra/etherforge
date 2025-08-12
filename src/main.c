#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>

#include "daemon.h"
#include "logging.h"
#include "config.h"

static daemon_context_t g_daemon_ctx;
static volatile sig_atomic_t g_shutdown = 0;

static void signal_handler(int signum) {
    switch (signum) {
        case SIGINT:
        case SIGTERM:
            LOG_INFO("Received signal %d, shutting down", signum);
            g_shutdown = 1;
            g_daemon_ctx.shutdown_requested = true;
            break;
        case SIGHUP:
            LOG_INFO("Received SIGHUP, ignoring");
            break;
        default:
            LOG_WARN("Received unexpected signal %d", signum);
            break;
    }
}

static void setup_signal_handlers(void) {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    
    signal(SIGPIPE, SIG_IGN);
}

static void print_usage(const char *program_name) {
    printf("EtherForged - EtherCAT Development Platform\n");
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\nOptions:\n");
    printf("  -c, --config FILE    Configuration file path (default: /etc/etherforged/etherforged.yaml)\n");
    printf("  -i, --interface IF   Network interface name (overrides config)\n");
    printf("  -p, --port PORT      UDP port number (overrides config)\n");
    printf("  -v, --verbose        Enable verbose logging\n");
    printf("  -h, --help           Show this help message\n");
    printf("  --version            Show version information\n");
    printf("\nExamples:\n");
    printf("  %s --interface eth1                    # Use eth1 interface\n", program_name);
    printf("  %s --config /opt/etherforged.yaml     # Use custom config file\n", program_name);
    printf("  %s --verbose --interface eth1         # Verbose logging\n", program_name);
    printf("  nohup %s -i eth1 &                    # Run in background\n", program_name);
    printf("\nFor more information, visit: https://github.com/etherforge/etherforged\n");
}

static void print_version(void) {
    printf("EtherForged v1.0.0\n");
    printf("EtherCAT Development Platform\n");
    printf("Built: %s %s\n", __DATE__, __TIME__);
    
#ifdef HAVE_SOEM
    printf("EtherCAT Master: SOEM (enabled)\n");
#else
    printf("EtherCAT Master: Stub implementation (SOEM not available)\n");
#endif
    
    printf("Protocol Version: 1.0\n");
    printf("License: GPL v3 (core daemon), MIT (client libraries)\n");
}

static int drop_privileges(void) {
    struct passwd *pw = getpwnam("etherforged");
    if (!pw) {
        LOG_WARN("User 'etherforged' not found, running as current user");
        return 0;
    }
    
    if (setgid(pw->pw_gid) != 0) {
        LOG_ERROR("Failed to set group ID: %s", strerror(errno));
        return -1;
    }
    
    if (setuid(pw->pw_uid) != 0) {
        LOG_ERROR("Failed to set user ID: %s", strerror(errno));
        return -1;
    }
    
    LOG_INFO("Dropped privileges to user 'etherforged'");
    return 0;
}


int main(int argc, char *argv[]) {
    const char *config_file = "/etc/etherforged/etherforged.yaml";
    const char *interface_override = NULL;
    int port_override = -1;
    bool verbose = false;
    
    static struct option long_options[] = {
        {"config",    required_argument, 0, 'c'},
        {"interface", required_argument, 0, 'i'},
        {"port",      required_argument, 0, 'p'},
        {"verbose",   no_argument,       0, 'v'},
        {"help",      no_argument,       0, 'h'},
        {"version",   no_argument,       0, 'V'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "c:i:p:vhV", long_options, NULL)) != -1) {
        switch (c) {
            case 'c':
                config_file = optarg;
                break;
            case 'i':
                interface_override = optarg;
                break;
            case 'p':
                port_override = atoi(optarg);
                if (port_override <= 0 || port_override > 65535) {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    return EXIT_FAILURE;
                }
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'V':
                print_version();
                return EXIT_SUCCESS;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }
    
    if (logging_init("console", verbose ? "debug" : "info") < 0) {
        fprintf(stderr, "Failed to initialize logging\n");
        return EXIT_FAILURE;
    }
    
    LOG_INFO("EtherForged starting");
    print_version();
    
    if (daemon_init(&g_daemon_ctx, config_file) < 0) {
        LOG_ERROR("Failed to initialize daemon");
        return EXIT_FAILURE;
    }
    
    if (interface_override) {
        strncpy(g_daemon_ctx.config.network.interface, interface_override, 
                sizeof(g_daemon_ctx.config.network.interface) - 1);
        LOG_INFO("Interface override: %s", interface_override);
        
        // Re-initialize EtherCAT with correct interface
        ethercat_cleanup(&g_daemon_ctx.ec_ctx);
        if (ethercat_init(&g_daemon_ctx.ec_ctx, g_daemon_ctx.config.network.interface) < 0) {
            LOG_ERROR("Failed to re-initialize EtherCAT master with override interface");
            return EXIT_FAILURE;
        }
    }
    
    if (port_override > 0) {
        g_daemon_ctx.config.security.port = port_override;
        LOG_INFO("Port override: %d", port_override);
    }
    
    if (geteuid() == 0) {
        LOG_WARN("Running as root - this may be required for EtherCAT access");
    }
    
    setup_signal_handlers();
    
    if (daemon_start(&g_daemon_ctx) < 0) {
        LOG_ERROR("Failed to start service");
        daemon_cleanup(&g_daemon_ctx);
        return EXIT_FAILURE;
    }
    
    LOG_INFO("EtherForged service running - press Ctrl+C to stop");
    
    while (!g_shutdown) {
        sleep(1);
    }
    
    LOG_INFO("Shutting down service...");
    daemon_stop(&g_daemon_ctx);
    daemon_cleanup(&g_daemon_ctx);
    
    logging_cleanup();
    
    LOG_INFO("EtherForged stopped");
    return EXIT_SUCCESS;
}