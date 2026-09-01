#!/usr/bin/env python3
from pathlib import Path

path = Path('.github/workflows/update-payloads.yml')
text = path.read_text()
old = '''          strings "$payload" | grep -F 'RMG_BOOT_V1'
          strings "$payload" | grep -F 'pa3q-S938BXXSBCZG3-app-physical-p0-oracle'
          for marker in RMG_RACE_V1 RMG_SCHED_V1 RMG_SYS_V1 RMG_PREP_V1 RMG_PREP_CHECKPOINT_V1 RMG_DIAG_V1; do
            if strings "$payload" | grep -Fq "$marker"; then
              echo "Unexpected in-band diagnostic marker in production payload: $marker" >&2
              exit 1
            fi
          done
'''
new = '''          grep -aFq 'RMG_BOOT_V1' "$payload"
          grep -aFq 'pa3q-S938BXXSBCZG3-app-physical-p0-oracle' "$payload"
          for marker in RMG_RACE_V1 RMG_SCHED_V1 RMG_SYS_V1 RMG_PREP_V1 RMG_PREP_CHECKPOINT_V1 RMG_DIAG_V1; do
            if grep -aFq "$marker" "$payload"; then
              echo "Unexpected in-band diagnostic marker in production payload: $marker" >&2
              exit 1
            fi
          done
'''
if text.count(old) != 1:
    raise SystemExit('expected production marker validation block exactly once')
path.write_text(text.replace(old, new, 1))
