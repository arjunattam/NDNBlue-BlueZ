# NDNBlue for Bluez

NDNBlue for BlueZ extends CCNx to work over Bluetooth. There is also an Android version called NDNBlue-Android. This work is based on [NDNLP](https://github.com/NDN-Routing/NDNLP).

### Prerequisites ###
* CCNx: Tested with version 0.7.1
* Linux with BlueZ libraries: Tested on Xubuntu 12.04

### Configuration ###
* Build binaries
        + ./waf configure
        + ./waf
        + sudo ./waf install
* Start ccnd if it is not running
        + ccndstart
* Recommended: Go to Bluetooth settings in OS and pair the two devices before running NDNBlue

### Usage ###
* Ensure CCNx is running and Bluetooth is enabled
* Start server or client
* Decide on prefix
* For server
        + ndnblue ccnx:/prefix_goes_here server
* For client
        + ndnblue ccnx:/prefix_goes_here client 00:10:60:AA:36:F8