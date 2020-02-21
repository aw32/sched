# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause
cmake_minimum_required(VERSION 3.10)
project(sched)

if(NOT WIN32)
	string(ASCII 27 Esc)
	set(ColourReset "${Esc}[m")
	set(ColourBold  "${Esc}[1m")
	set(Red         "${Esc}[31m")
	set(Green       "${Esc}[32m")
	set(Yellow      "${Esc}[33m")
	set(Blue        "${Esc}[34m")
	set(Magenta     "${Esc}[35m")
	set(Cyan        "${Esc}[36m")
	set(White       "${Esc}[37m")
	set(BoldRed     "${Esc}[1;31m")
	set(BoldGreen   "${Esc}[1;32m")
	set(BoldYellow  "${Esc}[1;33m")
	set(BoldBlue    "${Esc}[1;34m")
	set(BoldMagenta "${Esc}[1;35m")
	set(BoldCyan    "${Esc}[1;36m")
	set(BoldWhite   "${Esc}[1;37m")
endif(NOT WIN32)


### OPTIONS

option(MEASURE_AMPEHRE "build sched with Ampehre library" ON)



### LIBRARIES

# Threads
find_package( Threads )
if(NOT CMAKE_THREAD_LIBS_INIT)
	message(FATAL_ERROR "Thread library not found")
endif()

# YAML
find_library(YAML_LIBRARY libyaml.so PATHS
    /usr/lib
	lib/libyaml/install/lib
)
include_directories(
	lib/libyaml/install/include
)
if(NOT YAML_LIBRARY)
    message(FATAL_ERROR "Yaml library not found")
else()
	message(${YAML_LIBRARY})
endif()

# CJSON
find_library(CJSON_LIBRARY libcjson.so PATHS
    /usr/lib/libcjson.so
    /usr/lib
	lib/cJSON/install/lib64
)
include_directories(
	lib/cJSON/install/include
)
if(NOT CJSON_LIBRARY)
    message(FATAL_ERROR "cJSON library not found")
else()
	message(${CJSON_LIBRARY})
endif()

# LOG4CPP
find_library(LOG4CPP_LIBRARY liblog4cpp.so PATHS
	/usr/lib
)
if(NOT LOG4CPP_LIBRARY)
    message(FATAL_ERROR "log4cpp library not found")
else()
	message(${LOG4CPP_LIBRARY})
endif()

# AMPEHRE
#find_library(AMPEHRE_LIBRARY libms_common.so PATHS
#	/usr/ampehre/lib/libms_common.so
#)
include_directories(
	/usr/ampehre/include
)
#add_library(ampehre SHARED IMPORTED)
#set_target_properties(ampehre PROPERTIES IMPORTED_LOCATION /usr/ampehre/lib/libms_common.so)
#if(NOT AMPEHRE_LIBRARY)
#    message(FATAL_ERROR "Ampehre library not found")
#endif()



# SOURCE

set(SRC_SCHED
	src/CLogger.cpp
    src/CConfig.cpp
	src/CTask.cpp
	src/ETaskOnEnd.cpp
	src/CTaskCopy.cpp
	src/CTaskWrapper.cpp
	src/CResource.cpp
	src/CTaskDatabase.cpp
	src/CSchedule.cpp
	src/CScheduleExt.cpp
	src/CFeedback.cpp
	src/CFeedbackMain.cpp
	src/CComServer.cpp
	src/CComClient.cpp
	src/CComSchedClient.cpp
	src/CComUnixServer.cpp
	src/CComUnixSchedServerMain.cpp
	src/CComUnixClient.cpp
	src/CComUnixSchedClient.cpp
	src/CComUnixSchedClientMain.cpp
	src/CComUnixWriteBuffer.cpp
	src/CTaskLoader.cpp
	src/CTaskLoaderMS.cpp
	src/CExternalHook.cpp
	src/CResourceLoader.cpp
	src/CResourceLoaderMS.cpp
	src/CScheduleAlgorithmMinMin.cpp
	src/CScheduleAlgorithmMinMin2.cpp
	src/CScheduleAlgorithmMinMin2Dyn.cpp
	src/CScheduleAlgorithmMinMinMig.cpp
	src/CScheduleAlgorithmMinMinMig2.cpp
	src/CScheduleAlgorithmMinMinMig2Dyn.cpp
	src/CScheduleAlgorithmMaxMin.cpp
	src/CScheduleAlgorithmMaxMin2.cpp
	src/CScheduleAlgorithmMaxMin2Dyn.cpp
	src/CScheduleAlgorithmMaxMinMig2.cpp
	src/CScheduleAlgorithmMaxMinMig2Dyn.cpp
	src/CScheduleAlgorithmSufferage.cpp
	src/CScheduleAlgorithmSufferage2.cpp
	src/CScheduleAlgorithmSufferage2Dyn.cpp
	src/CScheduleAlgorithmSufferageMig2.cpp
	src/CScheduleAlgorithmSufferageMig2Dyn.cpp
	src/CScheduleAlgorithmHEFT.cpp
	src/CScheduleAlgorithmHEFT2.cpp
	src/CScheduleAlgorithmHEFT2Dyn.cpp
	src/CScheduleAlgorithmHEFTMig2.cpp
	src/CScheduleAlgorithmHEFTMig2Dyn.cpp
	src/CScheduleAlgorithmGenetic.cpp
	src/CScheduleAlgorithmGeneticDyn.cpp
	src/CScheduleAlgorithmGeneticMig.cpp
	src/CScheduleAlgorithmGeneticMigDyn.cpp
	src/CScheduleAlgorithmGeneticMigSolverLP.cpp
	src/CScheduleAlgorithmGeneticMigSolverLPDyn.cpp
	src/CScheduleAlgorithmGeneticMigSolverLPEnergy.cpp
	src/CScheduleAlgorithmGeneticMigSolverLPEnergyDyn.cpp
	src/CScheduleAlgorithmSimulatedAnnealing.cpp
	src/CScheduleAlgorithmSimulatedAnnealingDyn.cpp
	src/CScheduleAlgorithmSimulatedAnnealingMig.cpp
	src/CScheduleAlgorithmSimulatedAnnealingMigDyn.cpp
	src/CScheduleAlgorithmLinear.cpp
	src/CScheduleAlgorithmTestMigration.cpp
	src/CScheduleAlgorithmMCT.cpp
	src/CScheduleAlgorithmMCT2.cpp
	src/CScheduleAlgorithmMCTMig.cpp
	src/CScheduleAlgorithmMCTMig2.cpp
	src/CScheduleAlgorithmMET.cpp
	src/CScheduleAlgorithmMET2.cpp
	src/CScheduleAlgorithmMETMig2.cpp
	src/CScheduleAlgorithmOLB.cpp
	src/CScheduleAlgorithmOLB2.cpp
	src/CScheduleAlgorithmOLBMig2.cpp
	src/CScheduleAlgorithmSA.cpp
	src/CScheduleAlgorithmSA2.cpp
	src/CScheduleAlgorithmSAMig2.cpp
	src/CScheduleAlgorithmKPB.cpp
	src/CScheduleAlgorithmKPB2.cpp
	src/CScheduleAlgorithmKPBMig2.cpp
	src/CScheduleAlgorithmReMinMin.cpp
	src/CScheduleAlgorithmReMinMin2.cpp
	src/CScheduleAlgorithmReMinMin2Dyn.cpp
	src/CScheduleAlgorithmReMinMinMig2.cpp
	src/CScheduleAlgorithmReMinMinMig2Dyn.cpp
	src/CScheduleAlgorithm.cpp
	src/CScheduleExecutor.cpp
	src/CScheduleExecutorMain.cpp
	src/CScheduleComputer.cpp
	src/CScheduleComputerMain.cpp
	src/CEstimationLinear.cpp
	src/CEstimation.cpp
	src/CTimer.cpp
	src/CMeasure.cpp
)
set(SRC_MAINSCHED
	src/CMain.cpp
)
set(SRC_SIMSCHED
	src/CSimMain.cpp
	src/CSimQueue.cpp
)
set(SRC_WRAP
	src/CComUnixSchedSchedulerWrap.cpp
	src/CComUnixSchedScheduler.cpp
	src/CComSchedScheduler.cpp
	src/CComUnixSchedClientWrap.cpp
	src/CComUnixSchedServerWrap.cpp
	src/CUnixWrapClient.cpp
	src/CWrapTaskGroup.cpp
	src/CTaskDefinitions.cpp
	src/CWrapMain.cpp
)

# OPTION MEASURE_AMPEHRE
if (MEASURE_AMPEHRE)

	# add source file
	set(SRC_SCHED
		${SRC_SCHED}
		src/CMeasureAmpehre.cpp
	)
	# add static library path to linker
	SET( CMAKE_EXE_LINKER_FLAGS  "/usr/ampehre/lib/libms_common.so")
	# add define
	add_definitions(-DSCHED_MEASURE_AMPEHRE)

	message("${BoldCyan}Use Ampehre library for measurement${ColourReset}")

else (MEASURE_AMPEHRE)

	# add source file
	set(SRC_SCHED
		${SRC_SCHED}
		src/CMeasureNull.cpp
	)
	# add define
	add_definitions(-DSCHED_MEASURE_NULL)

	message("${BoldCyan}Use no measurement${ColourReset}")

endif (MEASURE_AMPEHRE)


# EXECUTABLES

# main executable
add_executable(sched ${SRC_MAINSCHED} ${SRC_SCHED} "src/sched.cpp")
target_link_libraries(sched ${YAML_LIBRARY} ${CJSON_LIBRARY} ${LOG4CPP_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

# simulation executable
add_executable(simsched ${SRC_SCHED} ${SRC_SIMSCHED} "src/simsched.cpp")
target_link_libraries(simsched ${YAML_LIBRARY} ${CJSON_LIBRARY} ${LOG4CPP_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

# wrap executable
add_executable(wrap ${SRC_SCHED} ${SRC_WRAP} "src/wrap.cpp")
target_link_libraries(wrap ${YAML_LIBRARY} ${CJSON_LIBRARY} ${LOG4CPP_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

add_subdirectory(scripts)

install(TARGETS sched RUNTIME DESTINATION bin)
install(TARGETS simsched RUNTIME DESTINATION bin)
install(TARGETS wrap RUNTIME DESTINATION bin)
