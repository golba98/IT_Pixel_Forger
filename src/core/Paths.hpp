#pragma once

#include <QString>

namespace itpixel {

// Absolute path to the bundled default reference image, located relative to
// the running application binary (build tree, install tree, or repository
// root). Returns an empty string when the asset cannot be found.
[[nodiscard]] QString defaultReferencePath();

}  // namespace itpixel
