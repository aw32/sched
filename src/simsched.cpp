// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CSimMain.h"

int main(){
	sched::sim::CSimMain* main = new sched::sim::CSimMain();
	int res = main->main();

	delete main;

	return res;
};
