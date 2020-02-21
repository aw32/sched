// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CRESOURCELOADER_H__
#define __CRESOURCELOADER_H__
namespace sched {
namespace schedule {

	class CResource;

	/// @brief Loads additional resource information
	class CResourceLoader {

		public:
			virtual int loadInfo();
			virtual void clearInfo();
			virtual void getInfo(CResource* resource);


			static CResourceLoader* loadResourceLoader();

			virtual ~CResourceLoader();
	};

} }
#endif
