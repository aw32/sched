// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CMeasureAmpehre.h"
#include "CConfig.h"
#include "CLogger.h"
using namespace sched::measure;


CMeasureAmpehre::CMeasureAmpehre(){

	version.major = MS_MAJOR_VERSION;
	version.minor = MS_MINOR_VERSION;
	version.revision = MS_REVISION_VERSION;

	// read config
	// read "measure" attribute
	CConfig* config = CConfig::getConfig();
	int res = 0;
	res = config->conf->getBool((char*)"measure", &mMeasure);
	if (res == -1) {
		CLogger::mainlog->error("MeasureAmpehre: config key \"measure\" not found, using default: false");
	}

	if (true == mMeasure) {
		CLogger::mainlog->info("MeasureAmpehre: Measurement activated");
	} else {
		CLogger::mainlog->info("MeasureAmpehre: Measurement deactivated");
	}

	// read interval attributes for several resources
	config->conf->getUint64((char*)"amphere_cpu_s", &cpu_s);
	config->conf->getUint64((char*)"amphere_cpu_ns", &cpu_ns);
	config->conf->getUint64((char*)"amphere_gpu_s", &gpu_s);
	config->conf->getUint64((char*)"amphere_gpu_ns", &gpu_ns);
	config->conf->getUint64((char*)"amphere_fpga_s", &fpga_s);
	config->conf->getUint64((char*)"amphere_fpga_ns", &fpga_ns);
	config->conf->getUint64((char*)"amphere_system_s", &system_s);
	config->conf->getUint64((char*)"amphere_system_ns", &system_ns);
	config->conf->getUint64((char*)"amphere_mic_s", &mic_s);
	config->conf->getUint64((char*)"amphere_mic_ns", &mic_ns);

}


CMeasureAmpehre::~CMeasureAmpehre(){
	if (mMeasure == false) {
		return;
	}
	ms_free_measurement(ml);
	ms_fini(ms);
}


void CMeasureAmpehre::start(){

	if (mMeasure == false) {
		return;
	}
	ms = ms_init(&version, CPU_GOVERNOR_ONDEMAND,
			0, 0, GPU_FREQUENCY_CUR,
			IPMI_SET_TIMEOUT, SKIP_PERIODIC, VARIANT_FULL);

	ml = ms_alloc_measurement(ms);

	ms_set_timer(ml, CPU, 	cpu_s, cpu_ns, 10);
	ms_set_timer(ml, GPU, 	gpu_s, gpu_ns, 10);
	ms_set_timer(ml, FPGA, 	fpga_s, fpga_ns, 10);
	ms_set_timer(ml, SYSTEM, system_s, system_ns, 10);
	ms_set_timer(ml, MIC, 	mic_s, mic_ns, 10);

	ms_init_measurement(ms, ml, CPU|GPU|FPGA|SYSTEM|MIC);

	cpu = (MS_MEASUREMENT_CPU *) getMeasurement(&ml, CPU);
	gpu = (MS_MEASUREMENT_GPU *) getMeasurement(&ml, GPU);
	fpga = (MS_MEASUREMENT_FPGA *) getMeasurement(&ml, FPGA);
	mic = (MS_MEASUREMENT_MIC *) getMeasurement(&ml, MIC);
	sys = (MS_MEASUREMENT_SYS *) getMeasurement(&ml, SYSTEM);

	ms_start_measurement(ms);
	
}


void CMeasureAmpehre::stop(){

	if (mMeasure == false) {
		CLogger::mainlog->info("MeasureAmpehre: measurement was not activated");
		return;
	}
	ms_stop_measurement(ms);
	ms_join_measurement(ms);
	ms_fini_measurement(ms);
	CLogger::mainlog->info("MeasureAmpehre: measurement stopped");
}


void CMeasureAmpehre::print(){
	if (mMeasure == false) {
		return;
	}

	double clockCpu0_temp = 0.0;
	double clockCpu1_temp = 0.0;
	for (int i=0; i<CORES; ++i) {
		clockCpu0_temp  += cpu->msr_freq_core_eff_cur[0][i];
		clockCpu1_temp  += cpu->msr_freq_core_eff_cur[1][i];
	}

	double cpu0_clock = clockCpu0_temp / CORES;
	double cpu1_clock = clockCpu1_temp / CORES;

	CLogger::eventlog->info("\"event\":\"MEASURE\","
"\"cpu_power\":[%lf,%lf],"
"\"gpu_power\":%lf,"
"\"fpga_power\":%lf,"
"\"mic_power\":%lf,"
"\"sys_power\":%ld,"
"\"cpu_temp\":[%d,%d],"
"\"gpu_temp\":%d,"
"\"fpga_temp_m\":%lf,"
"\"fpga_temp_i\":%lf,"
"\"mic_temp\":%d,"
"\"sys_temp\":%lf,"
"\"cpu_clock\":[%lf,%lf],"
"\"gpu_clock_gfx\":%d,"
"\"gpu_clock_mem\":%d,"
"\"mic_clock_core\":%d,"
"\"mic_clock_mem\":%d,"
"\"cpu_util\":%lf,"
"\"gpu_util_core\":%d,"
"\"gpu_util_mem\":%d,"
"\"fpga_util\":%lf,"
"\"mic_util\":%lf,"
"\"cpu_mem\":%ld,"
"\"cpu_swap\":%ld,"
"\"gpu_mem\":%d,"
"\"mic_mem\":%d,"

"\"cpu_energy_acc\":[%lf,%lf],"
"\"gpu_energy_acc\":%lf,"
"\"fpga_energy_acc\":%lf,"
"\"mic_energy_acc\":%lf,"
"\"sys_energy_acc\":%d,"

"\"cpu_runtime\":%lf,"
"\"gpu_runtime\":%lf,"
"\"fpga_runtime\":%lf,"
"\"mic_runtime\":%lf,"
"\"sys_runtime\":%lf"
,
		(cpu->msr_power_cur[0][PKG] + cpu->msr_power_cur[0][DRAM])/1000.0,
		(cpu->msr_power_cur[1][PKG] + cpu->msr_power_cur[1][DRAM])/1000.0,
		gpu->nvml_power_cur/1000.0,
		fpga->maxeler_power_cur[POWER]/1000.0,
		mic->mic_power_cur[MIC_POWER]/1000.0,
		sys->ipmi_power_server_cur,
		cpu->msr_temperature_pkg_cur[0],
		cpu->msr_temperature_pkg_cur[1],
		gpu->nvml_temperature_cur,
		fpga->maxeler_temperature_cur[MTEMP],
		fpga->maxeler_temperature_cur[ITEMP],
		mic->mic_temperature_cur[MIC_DIE_TEMP],
		sys->ipmi_temperature_sysboard_cur,
		cpu0_clock,
		cpu1_clock,
		gpu->nvml_clock_graphics_cur,
		gpu->nvml_clock_mem_cur,
		mic->mic_freq_core_cur,
		mic->mic_freq_mem_cur,
		cpu->measure_util_avg_cur,
		gpu->nvml_util_gpu_cur,
		gpu->nvml_util_mem_cur,
		fpga->maxeler_util_comp_cur,
		mic->mic_util_avg_cur,
		cpu->measure_memory_cur[CPU_MEM_RAM_USED]>>10,
		cpu->measure_memory_cur[CPU_MEM_SWAP_USED]>>10,
		gpu->nvml_memory_used_cur>>10,
		mic->mic_memory_used_cur>>10,
		(cpu->msr_energy_acc[0][PKG] + cpu->msr_energy_acc[0][DRAM])/1000.0,
		(cpu->msr_energy_acc[1][PKG] + cpu->msr_energy_acc[1][DRAM])/1000.0,
		gpu->nvml_energy_acc/1000.0,
		fpga->maxeler_energy_acc[POWER]/1000.0,
		mic->mic_energy_acc[MIC_POWER]/1000.0,
		sys->ipmi_energy_server_acc,

		cpu->msr_time_runtime,
		gpu->nvml_time_runtime,
		fpga->maxeler_time_runtime,
		mic->mic_time_runtime,
		sys->ipmi_time_runtime
	);

}
