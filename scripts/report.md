${"%"} ${testname} Report

<pre>${log.filepath}</pre>

Test start: ${log.start_time().strftime("%Y.%m.%d %H:%M:%S") if log.start_time() != None else ""}

Test stop: ${log.real_stop_time().strftime("%Y.%m.%d %H:%M:%S") if log.real_stop_time() != None else ""}

Test length: ${"{0:.6f}".format(log.real_length()) if log.real_length() != None else ""}

Experiment length: ${"{0:.6f}".format(log.length()) if log.length() != None else ""}

${"##"} Resources

% for r in log.resources:
* ${r}
% endfor

% if log.algorithm != None:
${"##"} Algorithm

Name: <pre style="display:inline;">${str(log.algorithm.name)}</pre>
% for param in log.algorithm.parameters:
${param["name"] if "name" in param else "None"}: <pre style="display:inline;">${str(o["value"]) if "value" in param else "None"}</pre>
% endfor

% endif


${"##"} Overview

<%
	tasknum = len(test.taskset_tasks)
	appnum = len(test.taskset_reg)
	postnum = 0
	for appix, a in enumerate(log.apps):
		for taskix, task in enumerate(a.tasks):
			if task.state == "POST":
				postnum += 1
%>\

Finished tasks: ${postnum} / ${tasknum} in ${appnum} applications

<object data="${testtype}_all.svg" type="image/svg+xml"></object>

% if len(log.measure) > 0:
${"##"} Energy

| Resource | Energy (Ws) | Time (s) |
|----------+-------------+----------|
| CPU      | ${log.measure[-1]["cpu_energy_acc"]}  | ${log.measure[-1]["cpu_runtime"]}  |
| GPU      | ${log.measure[-1]["gpu_energy_acc"]}  | ${log.measure[-1]["gpu_runtime"]}  |
| FPGA     | ${log.measure[-1]["fpga_energy_acc"]} | ${log.measure[-1]["fpga_runtime"]} |
| SYSTEM   | ${log.measure[-1]["sys_energy_acc"]}  | ${log.measure[-1]["sys_runtime"]}  |

<object data="${testtype}_measurement.svg" type="image/svg+xml"></object>
% endif

${"##"} Applications

<object data="${testtype}_taskset_triangle.svg" type="image/svg+xml"></object>

<style>
table {
border-collapse: collapse;
}
table, th {
  border: 1px solid black;
}
table td {
  padding: 0.2em 0.4em;
  border: 0;
}
table td {
  border-right: 1px solid lightgray;
}
table tr:nth-child(even) {background-color: #f2f2f2;}
table td:last-child {
  border-right: 1px solid black;
}
</style>




| Id | Ix | Name | Size | Chkp. | Dep | Arrival | Finish | Duration | Parts | State | Status |
|----+----+------+------+-------------+-----+---------+--------+----------+-------+-------+--------|
% for appix, a in enumerate(log.apps):
% if appix != 0:
| **Id** | **Ix** | **Name** | **Size** | **Chkp.** | **Dep** | **Arrival** | **Finish** | **Duration** | **Parts** | **State** | **Status** | **Signal** |
% endif
| **#${appix}** |||||| \
**${"{0:.6f}".format(float(a.arrival_time())-log.minstart) if a.arrival_time() != None and log.minstart != None and float(a.arrival_time())-log.minstart != None else ""}**\
 |||||||
% for taskix, task in enumerate(a.tasks):
<%
	taskstatus = None
	tasksignal = None
	if testtype == "exp" and wraplogs != None and len(wraplogs) > 0:
		if len(wraplogs) >= appix+1:
			wl = wraplogs[appix]
			if wl != None and len(wl.apps) >= taskix+1:
				wl_app = wl.apps[taskix]
				taskstatus = wl_app.status
				tasksignal = wl_app.signaled_signal
	taskdur = 0.0
	for p in task.parts:
		if p.stop != None and p.start != None:
			taskdur += float(p.stop) - float(p.start)
	taskparts = ""
	for p in task.parts:
		taskparts += p.res_short
%>\
| ${task.tid} | ${taskix} | ${task.name} | ${task.size} | ${task.checkpoints} \
| ${" ".join([str(did) for did in task.dep])} \
| ${"{0:.6f}".format(float(task.arrival) - log.minstart) if task.arrival != None and log.minstart != None and float(task.arrival) - log.minstart != None else ""} \
| ${ "" if task.finish == None else "{0:.6f}".format(float(task.finish) - log.minstart) if task.finish != None and log.minstart != None and float(task.finish) - log.minstart != None else ""} \
| ${"{0:.6f}".format(taskdur) if taskdur != None else ""} \
| ${len(task.parts)} ${taskparts} \
| ${str(task.state)} \
| ${str(taskstatus)} \
| ${str(tasksignal)} |
% endfor
% endfor


${"##"} Schedules

% for s_ix, s in enumerate(log.schedules):
${"###"} Schedule #${s.schedule["schedule"]["id"]} - ${"{0:.6f}".format(float(s.schedule["time"])-log.minstart) if s.schedule["time"] != None and log.minstart != None and float(s.schedule["time"])-log.minstart != None else ""}


| Resources | Tasks |
|-----------+-------|
% for rid,tl in enumerate(s.schedule["schedule"]["tasks"]):
<%
	tasklist = ""
	for t in tl:
		tasklist += str(t["id"])+" " if "id" in t else "omg"
%>\
| ${log.resources[rid]} | ${tasklist} |
% endfor

<object data="${testtype}_schedule_${s_ix}.svg" type="image/svg+xml"></object>

% endfor


${"##"} Lifelines

<object data="${testtype}_tasklifelines.svg" type="image/svg+xml"></object>
