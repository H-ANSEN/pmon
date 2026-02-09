#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

#include "config.h"

#define SECONDS_IN_MINUTE 60

typedef enum {
    PMON_WORK,
    PMON_L_BREAK,
    PMON_S_BREAK,
} PmonPhase;

typedef struct {
    unsigned int cycles;
    unsigned int work_mins;
    unsigned int long_break_mins;
    unsigned int short_break_mins;
    FILE *log_file;
} PmonConf;

typedef struct {
    PmonPhase phase;
    unsigned int cycle_count;
    unsigned int time_worked;
    unsigned int time_on_break;
    unsigned int time_in_progress;
} PmonState;

static const char* PHASE_NAME[] = {
    [PMON_WORK]    = "Work",
    [PMON_L_BREAK] = "Long Break",
    [PMON_S_BREAK] = "Short Break"
};

static PmonState state = {
    .phase = PMON_WORK,
    .cycle_count = 0,
    .time_worked = 0,
    .time_on_break = 0
};

void log_time(const PmonConf *conf, int min, int secs, int total) {
    if (conf->log_file != NULL) {
        fseek(conf->log_file, 0, SEEK_SET);
        fprintf(conf->log_file, "%s: [%02d:%02d/%d]\n",
                PHASE_NAME[state.phase], min, secs, total);
        fflush(conf->log_file);
    } else {
        fprintf(stdout, "\r%s: [%02d:%02d/%d]",
                PHASE_NAME[state.phase], min, secs, total);
        fflush(stdout);
    }
}

int phase_minutes(const PmonConf *conf) {
    switch (state.phase) {
        case PMON_WORK:    return conf->work_mins;
        case PMON_L_BREAK: return conf->long_break_mins;
        case PMON_S_BREAK: return conf->short_break_mins;
    }
}

PmonPhase next_phase(PmonConf *conf) {
    if (state.phase == PMON_WORK) {
        state.cycle_count++;
        state.time_worked += conf->work_mins;
        if (state.cycle_count >= conf->cycles) return PMON_L_BREAK; 
        else                                   return PMON_S_BREAK;
    } else {

        if (state.phase == PMON_L_BREAK) {
            state.cycle_count = 0;
            state.time_on_break += conf->long_break_mins;
        }

        if (state.phase == PMON_S_BREAK) {
            state.time_on_break += conf->short_break_mins;
        }

        return PMON_WORK;
    }
}

void run_phase(const PmonConf *conf) {
    int phase_len_minutes = phase_minutes(conf);

    time_t current_time;
    time_t start_time = time(NULL);
    time_t end_time = start_time + (phase_len_minutes * SECONDS_IN_MINUTE);

    do {
        current_time = time(NULL);

        int time_left_seconds = end_time - current_time;
        int minutes_left = time_left_seconds / SECONDS_IN_MINUTE;
        int seconds_left = time_left_seconds % SECONDS_IN_MINUTE;

        state.time_in_progress = (phase_len_minutes * SECONDS_IN_MINUTE) - time_left_seconds;

        log_time(conf, minutes_left, seconds_left, phase_len_minutes);
        sleep(1);
    } while(current_time < end_time);

    state.time_in_progress = 0;
}

void print_usage(const char *prgm) {
    fprintf(stdout,
        "Usage: %s [-c cycles] [-w work_minutes] [-l long_break_minutes]\n"
        "          [-s short_break_minute] [-o log_filepath]\n"
        "  -h    Print this usage message\n"
        "  -c    Number of work sessions before a long break (default %d)\n"
        "  -w    Minutes per work session (default %d)\n"
        "  -l    Minutes per long break session (default %d)\n"
        "  -s    Minutes per short break session (default %d)\n"
        "  -o    Path to print timer output to",
        prgm, DEFAULT_CYCLES, DEFAULT_WORK_MINS, DEFAULT_LONG_BREAK_MINS, DEFAULT_SHORT_BREAK_MINS
    );
}

void exiting(int status, void *ptr) {
    int seconds_worked_total = state.time_worked + (state.phase == PMON_WORK ? state.time_in_progress : 0);
    int seconds_on_break_total = state.time_on_break + (state.phase == PMON_L_BREAK || state.phase == PMON_S_BREAK ? state.time_in_progress : 0);

    int hours_worked = seconds_worked_total / 3600;
    int minutes_worked = (seconds_worked_total % 3600) / 60;
    int seconds_worked = seconds_worked_total % 60;

    int hours_breaked = seconds_on_break_total / 3600;
    int minutes_breaked = (seconds_on_break_total % 3600) / 60;
    int seconds_breaked = seconds_on_break_total % 60;

    fprintf(stdout,
        "\n\n"
        "Time Studying: %d hrs %d mins %d secs\n"
        "Time On Break: %d hrs %d mins %d secs\n",
        hours_worked, minutes_worked, seconds_worked,
        hours_breaked, minutes_breaked, seconds_breaked);
}

void on_exit_handler(int sig) {
    exit(0);
}

int main(int argc, char **argv) {
    PmonConf conf = {
        .cycles = DEFAULT_CYCLES,
        .work_mins = DEFAULT_WORK_MINS,
        .long_break_mins = DEFAULT_LONG_BREAK_MINS,
        .short_break_mins = DEFAULT_SHORT_BREAK_MINS,
        .log_file = NULL
    };

    int opt;
    const char *log_filename = NULL;
    while ((opt = getopt(argc, argv, "c:w:l:s:o:h")) != -1) {
        switch (opt) {
            case 'c': conf.cycles = atoi(optarg); break;
            case 'w': conf.work_mins = atoi(optarg); break;
            case 'l': conf.long_break_mins = atoi(optarg); break;
            case 's': conf.short_break_mins = atoi(optarg); break;
            case 'o': log_filename = optarg; break;
            case '?':
            case 'h':
            default: print_usage(argv[0]); return 1;
        }
    }

    if (log_filename) {
        conf.log_file = fopen(log_filename, "w");
        if (!conf.log_file) {
            perror("fopen");
            return 1;
        }
    }

    signal(SIGINT, on_exit_handler);
    on_exit(exiting, NULL);

    while (1) {
        run_phase(&conf);
        state.phase = next_phase(&conf);
    }

    return 0;
}
