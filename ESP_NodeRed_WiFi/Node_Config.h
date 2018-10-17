// =================== Declaration ======================
const String  FW_version = "0040C";       // xyyzz: x-chipID (0-ESP8266,1-ESP8285), yy-major version, zz-minor version
const int     ledPin = 2;                 // GPIO2 of ESP8266

const char* ServerHost  = "myiotdata.duckdns.org";
const int dataPort       = 10722;
const int udpEchoPort   = 10721;
const char* fotaUrlBase = "myiotfota.duckdns.org";
const int fotaPort      = 10720;

/* =================== READ ME =======================
IDE Config: Board-Generic ESP8266 Module, Flash Mode-QIO, Flash Size:-1M (no SPIFFS).
ESP Response Code:
      0   - ESP inited and wait for ssid/pass
      1   - ssid/pass received. Connecting wifi with 3G
      2   - wifi connected, idle
      3   - incomplete data received
      4   - data complete and ready to send
      5   - ESP in Config Mode
      6   - Connenting to wifi
      7   - Connenting to wifi with wifi manager
      10  - Resolve Host address
      11  - Host IPaddress presented

      A   - OK response to Serial CMD
      B   - Error response to Serial CMD
        1 - B,1,wrong SEQ.
        2 - B,2,no CMD to execute
        3 - B,3,incorrect ESP state
        4 - B,4,Hostname not resolve,Wifi not connect.
        5 - B,5,Sent fail
        6 - B,6,Hostname lookup failed and no default Host IP
        9 - B,9,Unknown CMD
      C   - Debug message
    
MongoDB server URL: myiotdata.duckdns.org:8080
firmware URL:       myiotdata.duckdns.org:8080/.fota/ESP8xxxxx.bin
data format:        xxxxy,FWver,arduino Data        y:5-TCP, 6/7:UDP#retry
response:           1,newKey,[FOTA],0
=================== end of READ ME ======================= */
