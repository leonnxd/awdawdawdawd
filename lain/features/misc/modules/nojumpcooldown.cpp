#include "nojumpcooldown.h"
#include "../../../util/globals.h"
#include "../../../util/classes/classes.h"

namespace frizy {
    namespace misc {
        void nojumpcooldown() {
            if (globals::instances::lp.humanoid.is_valid()) {
                globals::instances::lp.humanoid.write_jumppower(55.0f);
            }
        }
    }
}
