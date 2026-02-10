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
    unsigned int current_phase_secs;
} PmonState;

static const char* PHASE_NAME[] = {
    [PMON_WORK]    = "Work",
    [PMON_L_BREAK] = "Long Break",
    [PMON_S_BREAK] = "Short Break"
};

static PmonState state = {0};

void log_time(const PmonConf *conf, int mins, int secs, int total) {
    FILE *out = conf->log_file ? conf->log_file : stdout;

    if (conf->log_file) {
        fseek(out, 0, SEEK_SET);
        fprintf(out, "%s: [%02d:%02d/%d]\n\n", PHASE_NAME[state.phase], mins, secs, total);
    } else {
        fprintf(out, "\r%s: [%02d:%02d/%d]       ", PHASE_NAME[state.phase], mins, secs, total);
        fprintf(out, "\r");
    }

    fflush(out);
}

int get_phase_minutes(const PmonConf *conf) {
    if (state.phase == PMON_WORK) return conf->work_mins;
    if (state.phase == PMON_L_BREAK) return conf->long_break_mins;
    return conf->short_break_mins;
}

void update_tracked_time(const PmonConf *conf) {
    if (state.phase == PMON_WORK) {
        state.time_worked += conf->work_mins * SECONDS_IN_MINUTE;
    } else {
        int mins = (state.phase == PMON_L_BREAK) ? conf->long_break_mins : conf->short_break_mins;
        state.time_on_break += mins * SECONDS_IN_MINUTE;
    }
}

PmonPhase get_next_phase(const PmonConf *conf) {
    if (state.phase == PMON_WORK) {
        state.cycle_count++;
        return (state.cycle_count >= conf->cycles) ? PMON_L_BREAK : PMON_S_BREAK;
    }

    if (state.phase == PMON_L_BREAK) state.cycle_count = 0;

    return PMON_WORK;
}

void run_phase(const PmonConf *conf) {
    int phase_minutes = get_phase_minutes(conf);
    time_t end_time = time(NULL) + (phase_minutes * SECONDS_IN_MINUTE);

    while (time(NULL) < end_time) {
        int left = end_time - time(NULL);
        state.current_phase_secs = (phase_minutes * SECONDS_IN_MINUTE) - left;
        log_time(conf, left / 60, left % 60, phase_minutes);
        sleep(1);
    }

    update_tracked_time(conf);
    state.current_phase_secs = 0;
}

void print_final_stats(int status, void *ptr) {
    (void) status; (void) ptr;
    int work_secs = state.time_worked + (state.phase == PMON_WORK ? state.current_phase_secs : 0);
    int break_secs = state.time_on_break + (state.phase != PMON_WORK ? state.current_phase_secs : 0);

    printf("\n\nTime Studying: %d hrs %d mins %d secs\n",
        work_secs / 3600, (work_secs % 3600) / 60, work_secs % 60);
    printf("Time On Break: %d hrs %d mins %d secs\n",
        break_secs / 3600, (break_secs % 3600) / 60, break_secs % 60);
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
        "  -o    Path to print timer output to\n",
        prgm, DEFAULT_CYCLES, DEFAULT_WORK_MINS, DEFAULT_LONG_BREAK_MINS, DEFAULT_SHORT_BREAK_MINS
    );
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
    on_exit(print_final_stats, NULL);

    while (1) {
        run_phase(&conf);
        state.phase = get_next_phase(&conf);
    }

    return 0;
}
