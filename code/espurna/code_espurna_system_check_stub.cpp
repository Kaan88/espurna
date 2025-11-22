#include "config/general.h"

#if !SYSTEM_CHECK_ENABLED

// Provide a safe, linkable replacement for the system-check helpers that
// other modules call even when SYSTEM_CHECK_ENABLED == 0.

// Return a reference so calls that expect an lvalue or a const& will bind.
unsigned int &systemStabilityCounter()
{
    static unsigned int counter = 0;
    return counter;
}

// No-op function used by button handling and elsewhere to force a stable state.
void systemForceStable()
{
    // reset or set to a stable value if you prefer:
    systemStabilityCounter() = 0;
}

#endif // !SYSTEM_CHECK_ENABLED