# zenbook-typec-workaround

Workaround for zenbook type C kernel regression:
https://bugzilla.kernel.org/show_bug.cgi?id=219626. Until the
regression is fixed (if ever) you can use this kernel module as a
workaround. It patches live kernel to revert back to the working DSM
function.

## Installation

Clone the repo under `/usr/src/zenbook-typec-workaround`. Install
`dkms` (eg. `dnf install dkms`). Run:

```
dkms add -m zenbook-typec-workaround -v 1.0
dkms build -m zenbook-typec-workaround -v 1.0
dkms install -m zenbook-typec-workaround -v 1.0
```

Make the module load at boot time:

```
echo zenbook-typec-workaround > /etc/modules-load.d/zenbook-typec-workaround.conf
```
