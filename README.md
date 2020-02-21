# Sched

Scheduling framework for management of heterogenous tasks on high-performance compute nodes written in C++.

## Requirements

* cmake3
* libyaml
* cjson
* log4cpp

Optional:
* Ampehre for measurement
* lpsolve for some algorithms
* valgrind for debugging

Additional requirements for scripts:
* scipy (python)
* matplotlib (python)
* numpy (python)
* mako (python)
* pandoc

## Build

If `libyaml` or `cjson` are not available in the official repositories for your system you may build them from source using the scripts in the `lib` directory.
The lib folder is also the local installation destination.

Change `Makefile` to correct the path to your C++ compiler.

Change `CMakeLists.txt` to edit the following options:
* `MEASURE_AMPEHRE`: build with support for Ampehre measurement, else no measurement is done

Compile:
* `make` to compile release binaries
* `make debug` to compile debug binaries
* `make docs` to generate doxygen documentation (written to `docs/doxygen`)



## Executables

* `sched` is the scheduler
* `simsched` is the simulation program
* `wrap` is the program to wrap tasks into one application



## Execute

The executables are configured using configuration files and environment variables.

See `config.yml` for an annotated example configuration file for sched and simsched.

The configuration file for wrap is similar, but limited to the variables: `resources`, `resourceloader` and `resourceloaderms_idle`.

### sched

| Variable      | Comment |
|---------------|---------|
| SCHED_LOG_PRIORITY | Main log priority, DEBUG, NOTICE, WARN, ERROR, CRIT, ALERT, FATAL, EMERG |
| SCHED_EVENTLOG     | Event log file |
| SCHED_LOG          | Main log file |
| SCHED_CONFIG       | Configuration file |
| SCHED_SOCKET       | Scheduler socket |


### simsched

*Additionally to the variables for sched.*

| Variable      | Comment |
|---------------|---------|
| SCHED_SIMFILE | Simulation file |
| SCHED_SIMLOG  | Simulation log file |


### wrap

| Variable       | Comment |
|----------------|---------|
| SCHED_EVENTLOG | Event log file (meaningless for the wrap program) |
| SCHED_CONFIG   | Wrap config |
| SCHED_SOCKET   | Scheduler socket |
| SCHED_TASKDEF  | Task definitions |
| WRAP_SOCKET    | Wrap socket |
| WRAP_FILE      | File with group definition |
| WRAP_LOGPREFIX | Path and prefix for additional log files, e.g. stdout/stderr of started processes |

### Additional variables for Ampehre

To execute sched with measurement consider the following variables for APAPI usage

```
LD_LIBRARY_PATH=/usr/ampehre/lib/
LD_PRELOAD=/usr/ampehre/lib/libms_common_apapi.so
```

## Example report

![](docs/example_report.png)


## Licence

Copyright 2019 Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>

SPDX-License-Identifier: BSD-2-Clause
