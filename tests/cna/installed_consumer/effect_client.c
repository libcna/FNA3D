/* An Effect client of an installed FNA3D (see tests/cna/installed_consumer.cmake).
 *
 * FNA3D.h declares its Effect entry points in MojoShader's types and asks its user to include
 * mojoshader.h; a client declares MOJOSHADER_effectStateChanges for FNA3D_ApplyEffect and reads
 * MOJOSHADER_effect, whose records only exist with the definitions FNA3D was built with. Both
 * have to come from the installation itself.
 *
 * This file is part of the libcna fork of FNA3D; see tests/cna/README.md.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "FNA3D.h"
#include "mojoshader.h"

typedef void (*ApplyEffect)(FNA3D_Device*, FNA3D_Effect*, uint32_t, MOJOSHADER_effectStateChanges*);

int main(void)
{
	MOJOSHADER_effectStateChanges changes;
	MOJOSHADER_effect effect;
	ApplyEffect apply = FNA3D_ApplyEffect;

	memset(&changes, '\0', sizeof(changes));
	memset(&effect, '\0', sizeof(effect));
	printf(
		"FNA3D %u (compiled against %u); MOJOSHADER_effectStateChanges is %u bytes, "
		"MOJOSHADER_effect %u with technique_count at %u\n",
		(unsigned int) FNA3D_LinkedVersion(),
		(unsigned int) FNA3D_COMPILED_VERSION,
		(unsigned int) sizeof(changes),
		(unsigned int) sizeof(effect),
		(unsigned int) offsetof(MOJOSHADER_effect, technique_count)
	);
	return (	FNA3D_LinkedVersion() == FNA3D_COMPILED_VERSION &&
			apply != NULL &&
			changes.render_state_change_count == 0 &&
			effect.technique_count == 0	) ? 0 : 1;
}
