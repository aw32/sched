// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CMain.h"

int main(){
	sched::CMain* main = new sched::CMain();
	int res = main->main();

	delete main;

	return res;
};
