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
    PmonPhase phase;
    unsigned int cycles;
    unsigned int cycle_count;
    unsigned int work_mins;
    unsigned int long_break_mins;
    unsigned int short_break_mins;
} PmonConf;

static const char* PHASE_NAME[] = {
    [PMON_WORK]    = "Work",
    [PMON_L_BREAK] = "Long Break",
    [PMON_S_BREAK] = "Short Break"
};

int phase_minutes(const PmonConf *conf) {
    switch (conf->phase) {
        case PMON_WORK:    return conf->work_mins;
        case PMON_L_BREAK: return conf->long_break_mins;
        case PMON_S_BREAK: return conf->short_break_mins;
    }
}

PmonPhase next_phase(PmonConf *conf) {
    if (conf->phase == PMON_WORK) {
        conf->cycle_count++;
        if (conf->cycle_count >= conf->cycles) return PMON_L_BREAK; 
        else                                   return PMON_S_BREAK;
    } else {
        if (conf->phase == PMON_L_BREAK) {
            conf->cycle_count = 0;
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

        fprintf(stderr, "\r%s: [%02d:%02d/%d]", 
                PHASE_NAME[conf->phase], minutes_left, 
                seconds_left, phase_len_minutes);
        sleep(1);
    } while(current_time < end_time);
}

void print_usage(const char *prgm) {
    fprintf(stdout,
        "Usage: %s [-c cycles] [-w work_minutes] [-l long_break_minutes] [-s short_break_minute]\n"
        "  -h    Print this usage message\n"
        "  -c    Number of work sessions before a long break (default %d)\n"
        "  -w    Minutes per work session (default %d)\n"
        "  -l    Minutes per long break session (default %d)\n"
        "  -s    Minutes per short break session (default %d)\n",
        prgm, DEFAULT_CYCLES, DEFAULT_WORK_MINS, DEFAULT_LONG_BREAK_MINS, DEFAULT_SHORT_BREAK_MINS
    );
}

int main(int argc, char **argv) {
    PmonConf conf = {
        .phase = PMON_WORK,
        .cycles = DEFAULT_CYCLES,
        .cycle_count = 0,
        .work_mins = DEFAULT_WORK_MINS,
        .long_break_mins = DEFAULT_LONG_BREAK_MINS,
        .short_break_mins = DEFAULT_SHORT_BREAK_MINS,
    };

    int opt;
    while ((opt = getopt(argc, argv, "c:w:l:s:h")) != -1) {
        switch (opt) {
            case 'c': conf.cycles = atoi(optarg); break;
            case 'w': conf.work_mins = atoi(optarg); break;
            case 'l': conf.long_break_mins = atoi(optarg); break;
            case 's': conf.short_break_mins = atoi(optarg); break;
            case '?':
            case 'h':
            default: print_usage(argv[0]); return 1;
        }
    }

    while (1) {
        run_phase(&conf);
        conf.phase = next_phase(&conf);
    }

    return 0;
}
