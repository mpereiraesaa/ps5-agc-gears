# GPU suspend-point lifecycle correction

The native submission adapter now calls `sceAgcSuspendPoint(void)` after a
successful `sceAgcDriverSubmitDcb`. The import NID is `h9z6+0hEydk`.
The declaration and link facade resolve to the console module, not an emulated
success implementation. The context requires both callbacks.

The motivating ps5vk FW12.02 control completed rendering and freed tracked
resources, but closing generated
`CPU_FAULT_SUSPENDPOINT_TIMEOUT_IN_SUSPEND_ASYNC` with `in_frame=1`.
After adding suspend points, the same diagnostic scene passed two close/relaunch
cycles; the first kernel trace showed suspension in 75ms and `in_frame=0`.
This is reference evidence from ps5vk, **not hardware validation of this repo**.

Host submission tests cover submit failure (no suspend call), successful ordering,
suspend failure propagation and missing callback rejection. A successful submit
followed by suspend failure still means submitted work: existing conservative
failure handling must retain resources. Neither return zero nor a suspend point
replaces exact GPU completion, VideoOut retirement or safe command-buffer reuse.

Full `make test` and `make native-release` passed. The resulting SELF SHA256 is
`5d51159723f836117219cb139c9d5022a277aab66cf0dc8c1741cd8e8d6e725f`.
It has not been deployed or hardware-close-tested. Same-artifact hardware
close/relaunch validation remains pending. Historical render/soak
evidence does not automatically certify this changed binary.
