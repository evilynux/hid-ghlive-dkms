# HID driver for Activision GH Live PS3, Wii U, and PS4 Guitar devices #

This driver module supports the GH Live devices for PS3, Wii U, and PS4. This module notably allows you to play songs with the 6-fret guitar in Clone Hero with your PS3, Wii U or PS4 dongle.

Many thanks to [InvoxiPlayGames](https://github.com/InvoxiPlayGames) for figuring out the magic control message to send to the PS3 and Wii U device. Also thanks to the authors of the [xpadneo](https://github.com/atar-axis/xpadneo) and [rtlwifi](https://github.com/rtlwifi-linux) modules, I've used their modules as examples of how to do things. *Monkey see, monkey do!*

### Related Project ###
- [GHLtar Utility](https://github.com/ghlre/GHLtarUtility): An application for MS Windows that allows you to use a GH Live PS3/Wii U dongle, or iOS Bluetooth Guitar, by emulating an Xbox 360 controller.

## Getting started ##
### Prerequisites ###

Easiest route is using `dkms`:

- On Arch and Arch-based distros (like Antergos): `sudo pacman -S dkms linux-headers`
- On Debian-based systems (like Ubuntu): `` sudo apt install dkms linux-headers-`uname -r` ``
- On Fedora: `` sudo dnf install dkms make kernel-devel-`uname -r` kernel-headers-`uname -r` ``
- On Manjaro: `sudo pacman -S dkms linux-latest-headers`
- On OSMC: `` sudo apt install dkms rbp2-headers-`uname -r` sudo ln -s "/usr/src/rbp2-headers-`uname -r`" "/lib/modules/`uname -r`/build" `` (as a workaround)
- On Raspbian: `sudo apt install dkms raspberrypi-kernel-headers`

Without `dkms`, you will require a configured kernel source tree. 

### How to Install ###
1. Clone the git repository: `git clone https://github.com/evilynux/hid-ghlive-dkms`
2. Move to the newly created folder: `cd hid-ghlive-dkms`
3. With `dkms`, run `sudo ./install.sh` or
4. Without `dkms`, run `cd hid-ghlive && make modules && sudo make modules_install`
5. Done!

### How to Update ###
1. Move to the cloned-repository folder
2. Update the cloned repository: `git pull`
3. With `dkms`, run `sudo ./uninstall.sh` followed by `sudo ./install.sh`or
4. Without `dkms`, run `cd hid-ghlive && make reinstall`

### How to Uninstall ###
1. Move to the cloned-repository folder
2. With `dkms`, run `sudo ./uninstall.sh` or
3. Without `dkms`, you are on your own, I can't reasonably cover all possibilities

