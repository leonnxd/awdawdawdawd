#include "misc.h"
#include "../../util/globals.h"
#include "../../util/classes/classes.h"

namespace frizy {
	namespace misc {
		void run() {
			// This function can be used for general misc features that run continuously
		}

		void jumppower() {
			if (globals::misc::jumppower && globals::misc::jumppowerkeybind.enabled) {
				globals::instances::lp.humanoid.write_jumppower(globals::misc::jumppowervalue);
			}
		}

		void autofriend() {
			// Auto-friend functionality is handled in the UI
			// This function can be used for any continuous autofriend operations
		}

	}
}
