# HID driver for Activision GH Live PS3 and Wii U Guitar devices #

As far as I know this is the first module for the GH Live PS3 and Wii U Guitar devices. I really wanted to play a 6-fret guitar on [Clone Hero](https://clonehero.net/). I could not figure how to get my PS4 dongles to cooperate (yet), but thankfuly it's a different matter for the Wii U ones. Furthermore, the PS3 dongles happen to be exactly the same as the Wii U ones, so this driver supports those as well.

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
3. Without `dkms`, run `cd hid-ghlive && make modules && sudo make modules_install`
5. Done!

### How to Update ###
1. Move to the cloned-repository folder
2. Update the cloned repository: `git pull`
3. With `dkms`, run `sudo ./install.sh` or
3. Without `dkms`, run `cd hid-ghlive && make reinstall`

### How to Uninstall ###
1. Move to the cloned-repository folder
2. With `dkms`, run `sudo ./uninstall.sh` or
3. Without `dkms`, you are on your own, I can't reasonably cover all possibilities

