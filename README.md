## pmon
A [Suckless](https://suckless.org/philosophy/) style pomodoro timer.
```
Usage: pmon [-c cycles] [-w work_minutes] [-l long_break_minutes]
          [-s short_break_minute] [-o log_filepath]
  -h    Print this usage message
  -c    Number of work sessions before a long break (default 4)
  -w    Minutes per work session (default 25)
  -l    Minutes per long break session (default 30)
  -s    Minutes per short break session (default 5)
  -o    Path to print timer output to
```
## Compiling
```
gcc pmon.c -o pmon
```
## i3Status Integration
Add to your i3Status configuration file the following:
```
order += "read_file pmon"

read_file pmon {
    path = <path to your output file>
}
```
Run pmon in the background in file output mode, passing the same path as specified in your i3Status configuration:
```
pmon -o <path to your output file> &
```
