// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CEXTERNALHOOK_H__
#define __CEXTERNALHOOK_H__

namespace sched {

	/// @brief Represents an external program that can be executed
	///
	/// The program command is saved as string and needs to contain the full path.
	/// Execution of the program is blocking.
	class CExternalHook {

		private:
			char* mpProgram = 0;

		public:
			CExternalHook(char* program);
			/// @brief Executes the program
			///
			/// Forks and execs the program.
			/// Afterwards waits for the child process to be finished.
			/// @param arguments Arguments passed to the program
			int execute(char** arguments);

	};

}
#endif
