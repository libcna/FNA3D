/* SPDX-License-Identifier: Zlib */
/* The CMake harness rejects validation messages written directly to stderr.
 * Device creation precedes the caller's first viewport, effect or draw. */
#include "texture_fixture.h"

int main(void)
{
	if (!Start()) return 1;
	TestEffect_ApplyDrawStates(device, TW, TH, 0);
	return Finish();
}
