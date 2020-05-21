// =================== Declaration ======================
const String  FW_version = "0040F";       // xyyzz: x-chipID (0-ESP8266,1-ESP8285), yy-major version, zz-minor version

const char* ServerHost  = "tbdltee.thddns.net";
const int dataPort      = 8895;
const char* fotaUrlBase = "tbdltee.thddns.net";
const int fotaPort      = 8890;

/* =================== READ ME =======================
IDE Config: Board-Generic ESP8266 Module, Flash Mode-DIO, Flash Size:-1M (no SPIFFS).
ESP Response Code:
      0   - ESP inited and wait for ssid/pass
      1   - ssid/pass received. Connecting wifi with 3G
      2   - wifi connected, idle
      3   - incomplete data received
      4   - data complete and ready to send
      5   - ESP in Config Mode
      6   - Connenting to wifi
      7   - Connenting to wifi with wifi manager
      10  - Resolving Host address
      11  - Host IPaddress presented

      A   - OK response to Serial CMD
      B   - Error response to Serial CMD
        1 - B,1,wrong SEQ.
        2 - B,2,no data to send.
        3 - B,3,No data received (send CMD recived)
        4 - B,4,No wifi-Data reject (data received but no wifi)
        5 - B,5,Hostname lookup error, resone (no Host IP/unknow ESP state)
        6 - B,6,Host connect error (TCP)
        7 - B,7,Sent UDP fail
        9 - B,9,Unknown CMD
      C   - Debug message

WiFI Auto Config:
  Connect to WiF error --> Search for ssid : x0[ssid]:[pass]x0, e.g. x0IOTWL3:021015539x0 --> WiFi config Portal
  
data server URL:    tbdltee.thddns.net:8895
firmware URL:       tbdltee.thddns.net:8891/.fota/ESP8xxxxx.bin
data format:        xxxxy,FWver,arduino Data        y:5-TCP, 6/7:UDP#retry
response:           1,newKey,[FOTA],0
=================== end of READ ME ======================= */
