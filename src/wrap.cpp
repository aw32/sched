// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CWrapMain.h"

int main(){
	sched::wrap::CWrapMain* main = new sched::wrap::CWrapMain();
	int res = main->main();

	delete main;

	return res;
};
