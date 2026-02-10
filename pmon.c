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
    PmonPhase phase;
    unsigned int cycle_count;
    unsigned int time_worked;
    unsigned int time_on_break;
    unsigned int current_phase_secs;

    FILE *log_file;
    const unsigned int cycles;
    const unsigned int work_mins;
    const unsigned int long_break_mins;
    const unsigned int short_break_mins;
} PmonConf;

static const char* PHASE_NAME[] = {
    [PMON_WORK]    = "Work",
    [PMON_L_BREAK] = "Long Break",
    [PMON_S_BREAK] = "Short Break"
};

void log_time(const PmonConf *conf, int mins, int secs, int total) {
    FILE *out = conf->log_file ? conf->log_file : stdout;

    if (conf->log_file) {
        fseek(out, 0, SEEK_SET);
        fprintf(out, "%s: [%02d:%02d/%d]\n\n",
                PHASE_NAME[conf->phase], mins, secs, total);
    } else {
        fprintf(out, "\r%s: [%02d:%02d/%d]                   ",
                PHASE_NAME[conf->phase], mins, secs, total);
        fprintf(out, "\e[?25l"); // hide cursor
    }

    fflush(out);
}

int get_phase_minutes(const PmonConf *conf) {
    if (conf->phase == PMON_WORK) return conf->work_mins;
    if (conf->phase == PMON_L_BREAK) return conf->long_break_mins;
    return conf->short_break_mins;
}

void update_tracked_time(PmonConf *conf) {
    if (conf->phase == PMON_WORK) {
        conf->time_worked += conf->work_mins * SECONDS_IN_MINUTE;
    } else {
        int mins = (conf->phase == PMON_L_BREAK) ? conf->long_break_mins : conf->short_break_mins;
        conf->time_on_break += mins * SECONDS_IN_MINUTE;
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
    int phase_minutes = get_phase_minutes(conf);
    time_t end_time = time(NULL) + (phase_minutes * SECONDS_IN_MINUTE);

    while (time(NULL) < end_time) {
        int left = end_time - time(NULL);
        conf->current_phase_secs = (phase_minutes * SECONDS_IN_MINUTE) - left;
        log_time(conf, left / 60, left % 60, phase_minutes);
        sleep(1);
    }

    update_tracked_time(conf);
    conf->current_phase_secs = 0;
}

void print_final_stats(int status, void *ptr) {
    if (status != 2) {
        PmonConf *c = (PmonConf*)ptr;
        int work_secs = c->time_worked + (c->phase == PMON_WORK ? c->current_phase_secs : 0);
        int break_secs = c->time_on_break + (c->phase != PMON_WORK ? c->current_phase_secs : 0);

        printf("\n\nTime Studying: %d hrs %d mins %d secs\n",
            work_secs / 3600, (work_secs % 3600) / 60, work_secs % 60);
        printf("Time On Break: %d hrs %d mins %d secs\n",
            break_secs / 3600, (break_secs % 3600) / 60, break_secs % 60);

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
        "  -o    Path to print timer output to\n",
        prgm, DEFAULT_CYCLES, DEFAULT_WORK_MINS, DEFAULT_LONG_BREAK_MINS, DEFAULT_SHORT_BREAK_MINS
    );
}

void on_exit_handler(int sig) {
    exit(0);
}

PmonConf parse_cmd_args(int argc, char **argv) {
    FILE *log_file = NULL;
    const char *log_filename = NULL;
    unsigned int opt, cycles = 0, work_mins = 0, long_break_mins = 0, short_break_mins = 0;

    while ((opt = getopt(argc, argv, "c:w:l:s:o:h")) != -1) {
        switch (opt) {
            case 'c': cycles = atoi(optarg); break;
            case 'w': work_mins = atoi(optarg); break;
            case 'l': long_break_mins = atoi(optarg); break;
            case 's': short_break_mins = atoi(optarg); break;
            case 'o': log_filename = optarg; break;
            case '?':
            case 'h':
            default: print_usage(argv[0]); exit(2);
        }
    }

    if (log_filename) {
        log_file = fopen(log_filename, "w");
        if (!log_file) {
            perror("fopen");
            exit(2);
        }
    }

    return (PmonConf) {
        .phase = PMON_WORK,
        .log_file = log_file,
        .cycles = cycles ? cycles : DEFAULT_CYCLES,
        .work_mins = work_mins ?  work_mins : DEFAULT_WORK_MINS,
        .long_break_mins = long_break_mins ?  long_break_mins : DEFAULT_LONG_BREAK_MINS,
        .short_break_mins = short_break_mins ? short_break_mins : DEFAULT_SHORT_BREAK_MINS,
    };
}

int main(int argc, char **argv) {
    PmonConf conf = parse_cmd_args(argc, argv);

    signal(SIGINT, on_exit_handler);
    on_exit(print_final_stats, &conf);

    while (1) {
        run_phase(&conf);
        conf.phase = get_next_phase(&conf);
    }

    return 0;
}
