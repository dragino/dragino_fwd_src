# 1. Introduce.
SX1301/SX1302/SX1308 LoRaWAN concentrator driver. For devices:
- Armbian Platform: HP0A, HP0C, LPS8v2. 
- Raspberry Pi Platform: HP0D
- OpenWrt Platform: LG308, LG308N, LPS8, LPS8N, LIG16, DLOS8, DLOS8N.

# 2. How to Compile

## Requirements:
1. sudo apt install -y libsqlite3-dev
2. sudo apt install -y libftdi1-dev

## 2.1 For Debian Platform

###Compile:
1. Git Clone from: git clone https://github.com/dragino/dragino_fwd_src.git 
2. Enter into dragino_fwd_src/src, type command: ./hp0z-make-deb.sh  c
   the option 'c' mean you will compile for HP0C 
   the option 'd' mean you will compile for HP0D 
3. Wait until you get the *.deb package
4. Run 'dpkg -i' for install 

Reference Link: [Armbian Compile Instruction](http://wiki1.dragino.com/xwiki/bin/view/Main/Armbian%20OS%20instruction/#HHowtorecompileLoRaWANConcentratorDriver28dragino-fwdpackage29.)


## 2.2 For Raspberry Pi Platform

### download the source: git clone https://github.com/dragino/dragino_fwd_src.git

make if with sx1301/sx1308
```
cd dragino_fwd_src/src
make clean
./hp0z-make-deb.sh  r1
```

make if with sx1302
```
cd dragino_fwd_src/src
make clean
./hp0z-make-deb.sh  r2
```

### After build succeed, install debian package . 
```
dpkg -i draginofwd-($board)_($version).deb
```
$board will be rasp301 or rasp302
$version will be xxxx-xx-xx , which is current date.

## 2.3 For OpenWrt Platform
See [OpenWrt compile instruction](https://github.com/dragino/openwrt_lede-18.06#how-to-develop-a-c-software-before-build-the-image)



# 3. History Before move to this github
## 2021.12.16
file: fwd/src/pkt_serv.c
remove element from dn_list when size biger than 16
remove element from dn_list when devaddr duplicated 
change the loop time of read_dir 

file: fwd/src/mac-header-decode.c 
fix bug of macMsg elemen

## 2022/02/10
file:fwd/inc/gwcfg.h
change struct serv_net_s addr leng to 256

## 2022/04/24  
version:dragino-fwd-2.3
file:/fwd/src/pkt_serv.c
1.Fix typo for Class C downlink in ABP mode. 
2.Fix FCNT initiate issue for Manually ABP Downlink.

## 2022/05/26  
Move all SX1302 src for different platform (hp0x series, pi series, lgxxx series.)  to the same github
hp0x use new station, 
lg0x series use old station


## 2022/05/26  
Fix MAX definition error in utilities and parson.h

## 2022/05/31
add support sx1301 chips

## 2022/05/31
1. add cfg-301 cfg-302 cfg-308
2. fix sx1301 chips error
3. change compile option: if raspberry, build with sx1301 use option 'r1', build with sx1302 use 'r2'

## 2022/06/06
1. change gps time's  log level to timesync in fwd.c

## 2022/06/08
1. add PATH env in postinst script

## 2022/06/13
1. remove i2c link in postinst script
2. copy sx1302 include file loragw_i2c.h to loragw_i2c.h.rasp when using board hp0d or rasp
3. add config option: td_enabled(ture/false), time_diff(+-number) strings format (3 chars). 

## 2022/06/20  (release: dragino_gw_fwd_2.7.0-1_mips_24kc.ipk )
1. lbt function add
   configure:  
   1. lbt_tty_enabled : true/false
   2. lbt_tty_path :  example /dev/ttyUSB4
   3. lbt_tty_baude(option) : default 9600
   4. lbt_rssi_target(option) : default -85
   5. lbt_scan_time_ms(option) : default 6ms
2. lbt test utily:  lbt_test_utily
   useage: lbt_test_utily /dev/ttyUSB4 923200000
           This will be send AT command to ttyUSB4, 10 loops every 1ms
   run lbt_test_utily display how to useage.

usb module can only make 60ms scan loop.

   
3. localtime zone
    confiure:
    1. td_enabled : true/false
    if td_enabled, will be attach 3 characters to payload ( example: +08 / -08 )

## 2022/07/05  (release: dragino_gw_fwd_2.7.1-1_mips_24kc.ipk )
1. fix bug can't get counter time form concentor when use sx1302

## 2022/07/11  (release: dragino_gw_fwd_2.7.1-1_mips_24kc.ipk )
1. fix bug  rssi and snr not equal when macdecode: change rssi_snr[18] to rssi_snr[32]
2. add cfg-30? configure file to config path

## 2022/07/14  (release: dragino_gw_fwd_2.7.2-0_mips_24kc.ipk )
1. add time section for rxpkt when timer_ref by system  (semtech_serv.c)

## 2022/07/22  (release: dragino_gw_fwd_2.7.3-0_mips_24kc.ipk )
1. fix bug of fport filter  (65536 to 0)

## 2022/09/08 
1. chan fwd VERSION to 2.1.0

## 2022/09/26
1. custom downlink add fport control
2. chan fwd VERSION to 2.7.5

## 2022/10/13
1. basicstaion version upgrade to V2.0.6 (openwrt platform)

## 2022/10/17
1. fix sx1302 gps GRMC parser error

## 2022/11/01
1. log display of SF 
2. custom down option of bw

## 2022/11/04  fwd-2.7.8
1. fix bug: can't remove pkt on rxpkts link
   post sem to all service when pkt size more than 8 
   post sem after receive pkt?
2. add sem lock when getrxpkt

## 2022/11/16  fwd-2.7.9
1. fix bug: ( stack over ) fwd/src/mac-header-decode.c  change payloaden size 480 to 512

## 2022/11/30  fwd-2.7.9
1. change insert rxpkgs times, use cur_hal_time
2.  add  MAX_RXPKTS_LIST_SIZE 

## 2022/12/30  fwd-2.7.9
1.station: timeline is not correct of downlink, because OS is not RT system, remove time conditon when EU8686.

## 2023/01/17  fwd-2.7.9
1. fix network state , fwd can pull ack set state to online.

## 2023/02/01  fwd-2.8.0
1. station add AS_923 Handle

## 2023/02/28  fwd-2.8.1
1. station add EU865 Handle

## 2023/03/03  fwd-2.8.2
1. for sx1301 station, disable lbt by default 

## 2023/03/07  fwd-2.8.3
1. set I2C_DEVICE env for i2c device, if can't connet i2c device , ignore and use virtual data continue.

## 2023/03/15  fwd-2.8.4
1. gpst fix, (Quect L76)not support ubx, disable gps time.


## 2023/03/21  fwd-2.8.5
1. ghost thread which can receive package from udp port

## 2023/03/21  fwd-2.8.6
1. option set pull_interval on semtech protol pull

## 2023/06/12  fwd-2.8.7
1. customer download for opts option (mac command support)

## 2023/06/19  fwd-2.8.8
1. net status records in sqlite scheme (config with status_index)
2. sqlite scheme optimize (fix: is locked message)

## 2023/10/10  fwd-2.8.9
1. add filter of fwd by nwkid (nwkid if prefix of devaddr)

## 2023/10/18  fwd-2.9.0
1. add config time_ref for gps, if true rxpkt time will reference from gps

## 2023/12/08  fwd-3.0.0
1. add pthread list for control pkt decode
2. change rxpkt list for process simultan
3. add struct 'serv_ct' pthread data

## 2023/12/08  fwd-3.0.1
1. configure pkt service as other service (local_conf.json)

## 2024/02/26  fwd-3.0.2
1. fix counter of packages  (stats.c)

## 2024/02/28  fwd-3.0.3
1. 增加relay功能，网关可以将收到lora包通过接入的la66转发。
   -- edit pkt_serv.c 

## 2024/02/28  fwd-3.1.0
GW as relay or has relay function add
  -- some config for relay ( GW.relay:  1. as_relay 2. has_relay 3. relay_tty_path 4. relay_tty_baude )
  -- new file ( relay_serv.c )

## 2024/03/19 fwd-3.1.0
1。更改init_sock时的回收bug，修改online/offline输出的频率。
2。增加delay 功能，当前网络状态如果是offline，将会在内存里缓存lora包（64个）
   当网络转为online时，将缓存的包上发。

## 2024/05/19 fwd-3.1.2
station 的问题： 1.时间漂移值过大   2. 会收到莫名的包导致下发失败（diid=0的包） 3. 下发abandon的问题
适当改动：1.修改station同步时间的阀值timesysnc.c:  _MAX_DT  2. 跳过某些diid=0的包，因为这些是没有内容的包

## 2024/06/19 fwd-3.1.5
在文件/var/tmp/station_status.log里输出station的online/offline状态
增加了stattion状态的持续输出和取消了fwd logread的颜色显示

## 2024/07/01 fwd-3.1.6
found MIC not match ?
如果MIC不对称，继续解码。

## 2024/07/01 fwd-3.1.7
添加fcnt_gap配置，配置这个参数可以调整fcnt的最大值.

## 2024/07/01 fwd-3.1.9
pkt_service.c 发现一隐藏bug,接收到空包时解码导致出错。

## 2024/09/21 fwd-3.1.9
修改mac-header-decode.c: Fctrl里的上发包fpending应为classB位符
semtech_serv.c: 打开beacon调试功能

## 2024/10/14 fwd-3.2.0
修改pkt_serv.c semtech_serv.c里面关于macmsg处理的方法

