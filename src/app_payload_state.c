#include "common.h"

#if defined(APP_PAYLOAD) && APP_PAYLOAD && \
    (!defined(APP_REQUIRE_FRESH_P0_SESSION) || !APP_REQUIRE_FRESH_P0_SESSION)
/*
 * slide_app.c owns this state when fresh-session enforcement is enabled.
 * The CZG3 production profile does not enable that mode, but diagnostics in
 * main.c still read the exported state. Provide the disabled-mode definition
 * so the shared object never leaves a project symbol unresolved at runtime.
 */
int slide_p0_session_fresh;
#endif
