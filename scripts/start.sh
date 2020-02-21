
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


export APAPI_EVENTLIST=ms/default_eventlist.txt
export APAPI_CMPLIST=rapl 
export LD_LIBRARY_PATH=/usr/ampehre/lib/
export LD_PRELOAD=/usr/ampehre/lib/libms_common_apapi.so
#export SCHED_LOG_PRIORITY=DEBUG

build/sched
