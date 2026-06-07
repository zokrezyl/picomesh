"""Pytest wiring for the playwright/ browser-suite.

The fixtures these tests rely on (`page`, `base_url`, `gateway_url`) all live
in the PARENT package's conftest at
`tests/integration/picoforge/conftest.py`; pytest loads that conftest for
everything beneath it, so they are inherited here verbatim — one stack is
brought up for the whole session and shared across both the top-level tests
and this subdirectory.

The only thing we add is putting the parent directory on `sys.path` so the
shared `from helpers import ...` (sign-in/register) resolves from inside this
subpackage, exactly as it does for the top-level tests.
"""

import os
import sys

_PARENT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _PARENT not in sys.path:
    sys.path.insert(0, _PARENT)
