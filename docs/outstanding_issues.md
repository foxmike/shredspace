# Outstanding Issues

Internal engineering document. Not user-facing.

## Confirmed Deferred Requirements

### Factory Profile Tuning Is Not Finalized

- **Current behavior:** The 12 bundled factory profile JSON documents load, but their final musical tuning has not completed the planned real-guitar evaluation.
- **Expected behavior:** Shipped factory profiles should be evaluated and adjusted with representative high-gain guitar material across the target techniques, sample rates, hosts, and plugin formats before being declared release-ready.
- **Impact:** The profile catalog is operational, but its final release tuning remains an explicit product task.
- **Evidence:** `docs/verification/phase_k_release.md:36-40` lists real-guitar evaluation and resulting bundled-profile tuning as remaining release work and explicitly states that successful JSON parsing does not make the profiles release-ready.
- **Next action:** Complete the documented real-guitar evaluation matrix, record the results, and tune the bundled profiles from that evidence.
