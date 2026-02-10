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
    PmonPhase phase;                 // the current phase
    unsigned int cycle_count;        // current cycle number
    unsigned int work_secs;          // total work phases completed in seconds
    unsigned int break_secs;         // total break phases completed in seconds
    unsigned int current_phase_secs; // time spent in current phase in seconds

    FILE *log_file;                  // file to log to or NULL if none
    const char *log_filepath;        // path to log file or NULL if none

    const unsigned int cycles;       // number of work sessions to a long break
    const unsigned int work_time;    // length of work sessions in seconds
    const unsigned int lbreak_time;  // length of long breaks in seconds
    const unsigned int sbreak_time;  // length of short breaks in seconds
} PmonConf;

static const char* PHASE_NAME[] = {
    [PMON_WORK]    = "Work",
    [PMON_L_BREAK] = "Long Break",
    [PMON_S_BREAK] = "Short Break"
};

static volatile sig_atomic_t paused = 0;

void clear_log_file(PmonConf *conf) {
    if (conf->log_file != NULL && conf->log_filepath != NULL) {
        freopen(conf->log_filepath, "w", conf->log_file);
    }
}

void log_time(const PmonConf *conf, int mins, int secs, int total) {
    FILE *out = conf->log_file ? conf->log_file : stdout;
    const char *state = paused ? "(PAUSED) " : "";

    if (conf->log_file) {
        fseek(out, 0, SEEK_SET);
        fprintf(out, "%s%s: [%02d:%02d/%02d:00]",
                state, PHASE_NAME[conf->phase], mins, secs, total / SECONDS_IN_MINUTE);
    } else {
        fprintf(out, "\r%s%s: [%02d:%02d/%02d:00]                             ",
                state, PHASE_NAME[conf->phase], mins, secs, total / SECONDS_IN_MINUTE);
        fprintf(out, "\e[?25l"); // hide cursor
    }

    fflush(out);
}

int get_phase_length(const PmonConf *conf) {
    if (conf->phase == PMON_WORK) return conf->work_time;
    if (conf->phase == PMON_L_BREAK) return conf->lbreak_time;
    return conf->sbreak_time;
}

void update_tracked_time(PmonConf *conf) {
    if (conf->phase == PMON_WORK) {
        conf->work_secs += conf->work_time;
    } else {
        int mins = (conf->phase == PMON_L_BREAK) ? conf->lbreak_time : conf->sbreak_time;
        conf->break_secs += mins;
    }
}

PmonPhase get_next_phase(PmonConf *conf) {
    if (conf->phase == PMON_WORK) {
        conf->cycle_count++;
        return (conf->cycle_count >= conf->cycles) ? PMON_L_BREAK : PMON_S_BREAK;
    }

    if (conf->phase == PMON_L_BREAK) conf->cycle_count = 0;

    return PMON_WORK;
}

void run_phase(PmonConf *conf) {
    int phase_length = get_phase_length(conf);
    time_t end_time = time(NULL) + phase_length;
    time_t pause_start = 0;

    while (time(NULL) < end_time) {
        int left = end_time - time(NULL);
        conf->current_phase_secs = phase_length - left;
        log_time(conf, left / 60, left % 60, phase_length);
        sleep(1);

        if (paused) {
            pause_start = time(NULL);
            log_time(conf, left / 60, left % 60, phase_length);
            while (paused) { sleep(1); };
            end_time += time(NULL) - pause_start;
            clear_log_file(conf);
        }
    }

    update_tracked_time(conf);
    conf->current_phase_secs = 0;
}

void print_final_stats(int status, void *ptr) {
    if (status != 2) {
        PmonConf *c = (PmonConf*)ptr;
        int work_secs = c->work_secs + (c->phase == PMON_WORK ? c->current_phase_secs : 0);
        int break_secs = c->break_secs + (c->phase != PMON_WORK ? c->current_phase_secs : 0);

        printf("\n\nTime Studying: %d hrs %d mins %d secs\n",
            work_secs / 3600, (work_secs % 3600) / 60, work_secs % 60);
        printf("Time On Break: %d hrs %d mins %d secs\n",
            break_secs / 3600, (break_secs % 3600) / 60, break_secs % 60);

        clear_log_file(c);
        if (c->log_file != NULL) fclose(c->log_file);
        printf("\e[?25h"); // restore cursor
    }
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
        "  -o    Path to print timer output to\n"
        "Signals:\n"
        "  SIGUSR1 - Send to pause/resume the timer\n",
        prgm, DEFAULT_CYCLES, DEFAULT_WORK_MINS, DEFAULT_LONG_BREAK_MINS, DEFAULT_SHORT_BREAK_MINS
    );
}

void pause_handler(int sig) {
    paused = !paused;
}

void on_exit_handler(int sig) {
    exit(0);
}

PmonConf parse_cmd_args(int argc, char **argv) {
    FILE *log_file = NULL;
    const char *log_filepath = NULL;
    unsigned int opt, cycles = 0, work_mins = 0, lbreak_mins = 0, sbreak_mins = 0;

    while ((opt = getopt(argc, argv, "c:w:l:s:o:h")) != -1) {
        switch (opt) {
            case 'c': cycles = atoi(optarg); break;
            case 'w': work_mins = atoi(optarg); break;
            case 'l': lbreak_mins = atoi(optarg); break;
            case 's': sbreak_mins = atoi(optarg); break;
            case 'o': log_filepath = optarg; break;
            case '?':
            case 'h':
            default: print_usage(argv[0]); exit(2);
        }
    }

    if (log_filepath) {
        log_file = fopen(log_filepath, "w");
        if (!log_file) {
            perror("fopen");
            exit(2);
        }
    }

    return (PmonConf) {
        .phase = PMON_WORK,
        .log_file = log_file,
        .log_filepath = log_filepath,
        .cycles = cycles ? cycles : DEFAULT_CYCLES,
        .work_time = (work_mins ? work_mins : DEFAULT_WORK_MINS) * SECONDS_IN_MINUTE,
        .lbreak_time = (lbreak_mins ?  lbreak_mins : DEFAULT_LONG_BREAK_MINS) * SECONDS_IN_MINUTE,
        .sbreak_time = (sbreak_mins ? sbreak_mins : DEFAULT_SHORT_BREAK_MINS) * SECONDS_IN_MINUTE,
    };
}

int main(int argc, char **argv) {
    PmonConf conf = parse_cmd_args(argc, argv);

    signal(SIGINT, on_exit_handler);
    signal(SIGTERM, on_exit_handler);
    signal(SIGUSR1, pause_handler);
    on_exit(print_final_stats, &conf);

    while (1) {
        run_phase(&conf);
        conf.phase = get_next_phase(&conf);
        clear_log_file(&conf);
    }

    return 0;
}
