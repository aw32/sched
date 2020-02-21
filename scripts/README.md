# scripts

| Script | Comment |
|--------|---------|
| check  | Default test check script comparing makespan |
| env.sh | Load default environment variables |
| endtaskhook.sh | Default end task hook script, loads idle bitstream for FPGA resource |
| execexp.sh | Executes exp test |
| execsim.sh | Executes sim test |
| genconfig.sh | Generate default config file with overwrites from environment variables |
| log_stats.py | Log parsing |
| lp2mps.sh | Convert lp file to mps |
| lp_solve.sh | Runs lp_solve with default timeout |
| msresults.py | Loads measurement results |
| plot_beta.py | Plot beta (cumulative) distribution function |
| plot.py | Plot report diagrams, gantt charts, lifelines, energy plot |
| plot_taskset.py | Plot resource affinity triangle for given taskset |
| plot_tod_img.py | Generate background picture for resource affinity triangle |
| plot_tod.py | Plot resource affinity triangle for single task |
| report_gen.py | Generates report for logs in current working directory |
| report.md | Mako template for report generation |
| report.py | Create report for test |
| sleep.py | Example task similar to tasks_mig tasks |
| sleeptask.py | Example task for manual tests |
| sol_cor2lp.sh | Converts coin-or solution to lp solution |
| taskset_exec.py | Execute a taskset, wraps applications into wrap |
| taskset_gen.py | Generate a taskset |
| taskset_gen_schedule.py | Generate a schedule and adds it to the taskset |
| taskset_stats.py | Prints statistics for a taskset |
| test.py | Loads a test including the result logs |
| test.sh | Runs a test from the current working directory |
| test2ref.sh | Copy test results to test references |
| valwrap.sh | Executes a process using valgrind |
