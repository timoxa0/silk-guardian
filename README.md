## silk-guradian
silk-guardian is a kill switch for usb devices, build as kernel module. This is a fork of [kmile's fork](https://github.com/kmille/silk-guardian) of [NateBrune's silk-guardian](https://github.com/NateBrune/silk-guardian). I've added blacklist feature. There is an [Arch AUR package](https://aur.archlinux.org/packages/silk-guardian-blacklist-dkms) for it. 

Check out how it looks like:

[![asciicast](https://asciinema.org/a/vDXyJBwfYdV4TVpYkcRdv5y6e.svg)](https://asciinema.org/a/vDXyJBwfYdV4TVpYkcRdv5y6e)

Configuration takes place in `/etc/silk.yaml`:

```bash
[root@archlinux vagrant]# cat /etc/silk.yaml
---
shell_command: echo $(date) >> /tmp/silk.txt
shutdown: true
devlist:
  - 0x1234, 0x5678
devlist_is_whitelist: false
```
In this case, the usb device 0x1234,0x5678 is blacklisted. If it is plugged in (or out), a shell command is executed (`echo $(date) >> /tmp/silk.txt`). The computer is turned off (`shutdown: true`).

```bash
[root@archlinux vagrant]# cat /etc/silk.yaml
---
shell_command: echo $(date) >> /tmp/silk.txt
shutdown: false
devlist:
  - 0x1234, 0x5678
devlist_is_whitelist: true
```
In this case, the usb device 0x1234,0x5678 is whitelisted. If any other device is plugged in (or out), a shell command is executed (`echo $(date) >> /tmp/silk.txt`). The computer is not turned off (`shutdown: false`). You can use it with [go-luks-suspend](https://aur.archlinux.org/packages/go-luks-suspend). During installation your current usb devices are added to the whitelist. If you change the config file, you have to rebuild the module:
```bash
[root@archlinux vagrant]# dkms status
silk-guardian/v1.0.0, 6.3.8-arch1-1, x86_64: installed

[root@archlinux vagrant]# dkms build -m silk-guardian/v1.0.0
Module silk-guardian/v1.0.0 already built for kernel 6.3.8-arch1-1 (x86_64), skip. You may override by specifying --force.

[root@archlinux vagrant]# dkms build -m silk-guardian/v1.0.0 --force
Sign command: /usr/lib/modules/6.3.8-arch1-1/build/scripts/sign-file
Signing key: /var/lib/dkms/mok.key
Public certificate (MOK): /var/lib/dkms/mok.pub

Building module:
Cleaning build area...
PACKAGE_VERSION=v1.0.0 make all....
Signing module /var/lib/dkms/silk-guardian/v1.0.0/build/silk.ko
Cleaning build area...
[root@archlinux vagrant]# dkms install -m silk-guardian/v1.0.0 --force
Module silk-guardian-v1.0.0 for kernel 6.3.8-arch1-1 (x86_64).
Before uninstall, this module version was ACTIVE on this kernel.

silk.ko.zst:
 - Uninstallation
   - Deleting from: /usr/lib/modules/6.3.8-arch1-1/updates/dkms/
 - Original module
   - No original module was found for this module on this kernel.
   - Use the dkms install command to reinstall any previous module version.
depmod....

silk.ko.zst:
Running module version sanity check.
 - Original module
   - No original module exists within this kernel
 - Installation
   - Installing to /usr/lib/modules/6.3.8-arch1-1/updates/dkms/
depmod...
[root@archlinux vagrant]# 
```

You can check the current configuration with:
```bash
[root@archlinux vagrant]# cat /proc/silk
silk is turned on
version: v1.0.0
shell command is enabled: echo $(date) >> /tmp/silk.txt
whitelisted device: 0x1d6b, 0x0002
whitelisted device: 0x1d6b, 0x0001
whitelisted device: 0x1312, 0x1234
```

You can turn it off/on temporarily with
```bash
[root@archlinux vagrant]# echo 0 > /proc/silk
```
This is nice if you want to add a new device that needs to be whitelisted. silk-guardian logs can be found with `journalctl -k`.

## Rebuild
If you want to whitelist/use a new device
1) temporarly disable silk-guardian with `echo 0 > /proc/silk`, then 
2) rebuild/reinstall the kernel module with

```bash
#!/bin/bash -eu

SILK=$(dkms status | grep silk-guardian | awk -F, '{ print $1 }' | head -n 1)
# SILK is for example silk-guardian/v1.0.0
sudo dkms build -m "$SILK" --force  # uses the Makefile to build the kernel module (silk.ko)
sudo dkms install -m "$SILK" --force  # copy the file to /usr/lib/modules/$(uname -r)/updates/dkms/silk.ko.zst + depmod
sudo rmmod silk
sudo modprobe silk
sudo cat /proc/silk
```

## Known issues
silk-guardian conflicts with [lkrg](https://github.com/lkrg-org/lkrg). It does not like spawning a shell from the kernel :)
```
Jun 19 19:43:33 linux kernel: LKRG: ALERT: BLOCK: UMH: Executing program name /usr/bin/bash
```

silk-guardian only checks newly attached devices during runtime. A hardware keylogger plugged-in before boot will not be blocked! You can use [USBGuard](https://github.com/USBGuard/usbguard) to prevent that.

## Development

In the dev directory, there is a Vagrantfile. The SETUP file has some helpful commands.

This is the old documentation:

# silk-guardian

Silk Guardian is an anti-forensic LKM kill-switch that waits for a change on your usb ports then deletes precious files and turns off your computer.

 Inspired by [usbkill](https://github.com/hephaest0s/usbkill). 
 I remade this project as a Linux kernel driver for fun and to learn. Many people have contributed since, and I thank them.

To run:

```shell
make
sudo insmod silk.ko
```

You will need to have the `linux-headers` package installed. If you haven't:

```shell
sudo apt-get install linux-headers
```
### Why?

There are 3 reasons (maybe more?) to use this tool:

- In case the police or other thugs come busting in. The police commonly uses a « [mouse jiggler](http://www.amazon.com/Cru-dataport-Jiggler-Automatic-keyboard-Activity/dp/B00MTZY7Y4/ref=pd_bxgy_pc_text_y/190-3944818-7671348) » to keep the screensaver and sleep mode from activating.
- You don't want someone retrieve documents from your computer via USB or install malware or backdoors.
- You want to improve the security of your (Full Disk Encrypted) home or corporate server (e.g. Your Raspberry).

> **[!] Important**: Make sure to use (partial) disk encryption ! Otherwise intruders will be able to access your harddrive.

> **Tip**: Additionally, you may use a cord to attach a USB key to your wrist. Then insert the key into your computer and insert the kernel module. If they steal your computer, the USB will be removed and the computer shuts down immediately.

### Feature List

- Shutdown the computer when there is USB activity
- Secure deletion of incriminating files before shutdown
- No dependencies
- Difficult to detect

### To Do
- Ability to whitelist USB devices ![](http://www.gia.edu/img/sprites/icon-green-check.png)
- Remove files before shutdown ![](http://www.gia.edu/img/sprites/icon-green-check.png)
- Remove userspace dependancy upon shutdown ![](http://www.gia.edu/img/sprites/icon-green-check.png)

More like... to-done. Way to go community you did it!

### Change Log
2.0 - Updated to use notifier interface.

1.5 - Updated to use shred and remove files on shutdown

1.0 - Initial release.

### Contact

[natebrune@gmail.com](mailto:natebrune@gmail.com)  
https://keybase.io/natebrune
