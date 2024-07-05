#include "Warnings/Warnings.h"

#include "Warnings/CreateTexture2DWarning.h"
#include "Warnings/ImageSpaceAdapterWarning.h"

namespace Warnings
{
	void PreLoad()
	{
		if (!REL::Module::IsNG() && *Settings::CreateTexture2D) {  // TODO: NG
			CreateTexture2DWarning::Install();
		}

		if (*Settings::ImageSpaceAdapter) {
			ImageSpaceAdapterWarning::Install();
		}
	}
}
