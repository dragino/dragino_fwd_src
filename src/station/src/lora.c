/*
 * --- Revised 3-Clause BSD License ---
 * Copyright Semtech Corporation 2022. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright notice,
 *       this list of conditions and the following disclaimer in the documentation
 *       and/or other materials provided with the distribution.
 *     * Neither the name of the Semtech corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SEMTECH CORPORATION. BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include "s2conf.h"
#include "uj.h"
#include "rt.h"
#include "db.h"
#include "sys_linux.h"


#define MHDR_FTYPE  0xE0
#define MHDR_RFU    0x1C
#define MHDR_MAJOR  0x03
#define MHDR_DNFLAG 0x20

#define MAJOR_V1    0x00

#define FRMTYPE_JREQ   0x00
#define FRMTYPE_JACC   0x20
#define FRMTYPE_DAUP   0x40  // data (unconfirmed) up
#define FRMTYPE_DADN   0x60  // data (unconfirmed) dn
#define FRMTYPE_DCUP   0x80  // data confirmed up
#define FRMTYPE_DCDN   0xA0  // data confirmed dn
#define FRMTYPE_REJOIN 0xC0  // rejoin
#define FRMTYPE_PROP   0xE0  // propriatary
#define FTYPE_BIT(t) (1<<(((t) & MHDR_FTYPE)>>5))
#define DNFRAME_TYPE (FTYPE_BIT(FRMTYPE_JACC) | \
                      FTYPE_BIT(FRMTYPE_DADN) | \
                      FTYPE_BIT(FRMTYPE_DCDN) )


// +-----------------------------------------+
// |                JOIN FRAME               |
// +-----+---------+--------+----------+-----+
// |  1  |     8   |    8   |    2     |  4  |  bytes - all fields little endian
// +=====+=========+========+==========+=====+
// | mhdr| joineui | deveui | devnonce | MIC |
// +-----+---------+--------+----------+-----+
#define OFF_mhdr      0
#define OFF_joineui   1
#define OFF_deveui    9
#define OFF_devnonce 17
#define OFF_jreq_mic 19
#define OFF_jreq_len 23

// +------------------------------------------------------------+
// |                           DATA FRAME                       |
// +-----+---------+-----+-------+-------+------+---------+-----+
// |  1  |    4    |  1  |   2   |  0/15 | 0/1  |   0-?   |  4  |   bytes - all fields little endian
// +=====+=========+=====+=======+=======+======+=========+=====+
// | mhdr| devaddr |fctrl|  fcnt | fopts | port | payload | MIC |
// +-----+---------+-----+-------+-------+------+---------+-----+
#define OFF_devaddr     1
#define OFF_fctrl       5
#define OFF_fcnt        6
#define OFF_fopts       8
#define OFF_df_minlen  12


uL_t* s2e_joineuiFilter;
u4_t  s2e_netidFilter[4] = { 0xffFFffFF, 0xffFFffFF, 0xffFFffFF, 0xffFFffFF };

typedef struct LoraMessage{
	uint8_t fPort;
	int32_t devAddr;
	uint64_t deveui;
}LoraMessage_t;

bool basic_station_filter(StationFilter_t *pBSFilter, LoraMessage_t *LoraMessage) {
    char addr_key[64] = {0};
    char fport_key[64] = {0};
    char nwkid_key[64] = {0};
    char deveui_key[64] = {0};
    uint8_t nwkid = 0;

    snprintf(addr_key, sizeof(addr_key), "%s/devaddr/%08X", pBSFilter->server_name, LoraMessage->devAddr);
    snprintf(fport_key, sizeof(fport_key), "%s/fport/%u", pBSFilter->server_name, LoraMessage->fPort);
#ifdef BIGENDIAN
    nwkid = (LoraMessage->devAddr >> 25) & 0x7F;   /* Devaddr Format:  31..25(NwkID)  24..0(NwkAddr) */
#else
    nwkid = (LoraMessage->devAddr) & 0x7F;
#endif
    snprintf(nwkid_key, sizeof(nwkid_key), "%s/nwkid/%02X", pBSFilter->server_name, nwkid);

    char deveui_str[17]={0};
    if(LoraMessage->deveui>0){
    	sprintf(deveui_str, "%16llX", LoraMessage->deveui);
    	deveui_str[16]='\0';
		snprintf(deveui_key, sizeof(deveui_key), "%s/deveui/%s", pBSFilter->server_name, deveui_str);
    }else{
		snprintf(deveui_key, sizeof(deveui_key), "%s/deveui/", pBSFilter->server_name);
    }

	LOG(MOD_S2E|INFO, "[%s-filter] fport-lv=%d, addr-lv=%d, nwkid-lv=%d, deveui-lv=%d, addr_key=%s, fport_key=%s, nwkid_key=%s, deveui_key=%s\n", 
						pBSFilter->server_name, pBSFilter->filter.fport, pBSFilter->filter.devaddr, 
						pBSFilter->filter.nwkid, pBSFilter->filter.deveui, addr_key, fport_key, 
						nwkid_key, deveui_key);
    
    switch(pBSFilter->filter.fport) {
        case INCLUDE: // 1
            if (lgw_db_key_exist(fport_key)){
				LOG(MOD_S2E|INFO, "[%s-filter] fport filter include\n", pBSFilter->server_name);
				return true;  // filter
			}
			LOG(MOD_S2E|INFO, "[%s-filter] fport filter not include\n", pBSFilter->server_name);
            break;
        case EXCLUDE: // 2
            if (!lgw_db_key_exist(fport_key) && LoraMessage->fPort!=0){
				LOG(MOD_S2E|INFO, "[%s-filter] fport filter exclude\n", pBSFilter->server_name);
                return true;  //filter
			}
			LOG(MOD_S2E|INFO, "[%s-filter] fport filter not exclude\n", pBSFilter->server_name);
            break;
        default:
            LOG(MOD_S2E|INFO, "[%s-filter] fport no filter\n", pBSFilter->server_name);
            break;
    }

    switch(pBSFilter->filter.devaddr) {
        case INCLUDE: //1
            if (lgw_db_key_exist(addr_key)){
				LOG(MOD_S2E|INFO, "[%s-filter] devaddr filter include\n", pBSFilter->server_name);
                return true; // filter
			}
			LOG(MOD_S2E|INFO, "[%s-filter] devaddr filter not include\n", pBSFilter->server_name);
            break;
        case EXCLUDE:
            if (!lgw_db_key_exist(addr_key) && LoraMessage->devAddr>0){
				LOG(MOD_S2E|INFO, "[%s-filter] devaddr filter exclude\n", pBSFilter->server_name);
                return true; 
			}
			LOG(MOD_S2E|INFO, "[%s-filter] devaddr filter not exclude\n", pBSFilter->server_name);
            break;
        default:
            LOG(MOD_S2E|INFO, "[%s-filter] devaddr no filter\n", pBSFilter->server_name);
            break;
    }

    switch(pBSFilter->filter.nwkid) {
        case INCLUDE: //1
            if (lgw_db_key_exist(nwkid_key)){
				LOG(MOD_S2E|INFO, "[%s-filter] nwkid(%02X) filter include \n", pBSFilter->server_name, nwkid);
                return true; // filter
			}
			LOG(MOD_S2E|INFO, "[%s-filter] nwkid(%02X) filter not include \n", pBSFilter->server_name, nwkid);
            break;
        case EXCLUDE:
            if (!lgw_db_key_exist(nwkid_key) && LoraMessage->devAddr>0){
				LOG(MOD_S2E|INFO, "[%s-filter] nwkid(%02X) filter exclude \n", pBSFilter->server_name, nwkid);
                return true; 
            }
			LOG(MOD_S2E|INFO, "[%s-filter] nwkid(%02X) filter not exclude \n", pBSFilter->server_name, nwkid);
            break;
        default:
            LOG(MOD_S2E|INFO, "[%s-filter] nwkid(%02X) no filter\n", pBSFilter->server_name, nwkid);
            break;
    }

	if(LoraMessage->deveui>0){
		switch(pBSFilter->filter.deveui) {
			case INCLUDE: //1
				if (lgw_db_key_exist(deveui_key)){
					LOG(MOD_S2E|INFO, "[%s-filter] deveui(%s) filter include \n", pBSFilter->server_name, deveui_str);
					return true; // filter
				}
				LOG(MOD_S2E|INFO, "[%s-filter] deveui(%s) filter not include \n", pBSFilter->server_name, deveui_str);
				break;
			case EXCLUDE:
				if (!lgw_db_key_exist(deveui_key)){
					LOG(MOD_S2E|INFO, "[%s-filter] deveui(%s) filter exclude \n", pBSFilter->server_name, deveui_str);
					return true; 
				}
				LOG(MOD_S2E|INFO, "[%s-filter] deveui(%s) filter not exclude \n", pBSFilter->server_name, deveui_str);
				break;
			default:
				LOG(MOD_S2E|INFO, "[%s-filter] deveui(%s) no filter\n", pBSFilter->server_name, deveui_str);
				break;
		}
	}
	
    return false;  // no-filter
}


int s2e_parse_lora_frame (ujbuf_t* buf, const u1_t* frame , int len, dbuf_t* lbuf) {
    if( len == 0 ) {
    badframe:
        LOG(MOD_S2E|DEBUG, "Not a LoRaWAN frame: %16.4H", len, frame);
        return 0;
    }
    int ftype = frame[OFF_mhdr] & MHDR_FTYPE;
    if( (len < OFF_df_minlen && ftype != FRMTYPE_PROP) ||
        // (FTYPE_BIT(ftype) & DNFRAME_TYPE) != 0 || --- because of device_mode feature we parse everything
        (frame[OFF_mhdr] & (MHDR_RFU|MHDR_MAJOR)) != MAJOR_V1 ) {
	goto badframe;
    }
    
    LoraMessage_t lm;
    memset(&lm, 0x00, sizeof(LoraMessage_t));
    
    if( ftype == FRMTYPE_PROP || ftype == FRMTYPE_JACC ) {
        str_t msgtype = ftype == FRMTYPE_PROP ? "[PropDF]" : "[JACC]";
        uj_encKVn(buf,
                  "msgtype",   's', msgtype,
                  "FRMPayload",'H', len, &frame[0],
                  NULL);
        xprintf(lbuf, "%s %16.16H", msgtype, len, &frame[0]);
        return 1;
    }
    if( ftype == FRMTYPE_JREQ || ftype == FRMTYPE_REJOIN ) {
        if( len != OFF_jreq_len)
            goto badframe;
        uL_t joineui = rt_rlsbf8(&frame[OFF_joineui]);
        #if 0
        if( s2e_joineuiFilter[0] != 0 ) {
            uL_t* f = s2e_joineuiFilter-2;
            while( *(f += 2) ) {
                if( joineui >= f[0] && joineui <= f[1] )
                    goto out1;
            }
            
            xprintf(lbuf, "Join EUI %E filtered", joineui);
            return 0;
          out1:;
        }
        #endif
        str_t msgtype = (ftype == FRMTYPE_JREQ ? "[JoinReq]" : "[ReJoin]");
        u1_t  mhdr = frame[OFF_mhdr];
        uL_t  deveui = rt_rlsbf8(&frame[OFF_deveui]);
        u2_t  devnonce = rt_rlsbf2(&frame[OFF_devnonce]);
        s4_t  mic = (s4_t)rt_rlsbf4(&frame[len-4]);
        uj_encKVn(buf,
                  "msgtype", 's', msgtype,
                  "MHdr",    'i', mhdr,
                  rt_joineui,'E', joineui,
                  rt_deveui, 'E', deveui,
                  "DevNonce",'i', devnonce,
                  "MIC",     'i', mic,
                  NULL);
        LOG(MOD_S2E|INFO, "%s MHdr=%02X %s=%:E %s=%:E DevNonce=%d MIC=%d",
                msgtype, mhdr, rt_joineui, joineui, rt_deveui, deveui, devnonce, mic);
                
        lm.deveui=deveui;
        if(basic_station_filter(&gBSFilter, &lm)){
			LOG(MOD_S2E|INFO, "%s was filted by %s:[%:E]", msgtype, rt_deveui, deveui);
			return 0;
        }
        return 1;
    }
    u1_t foptslen = frame[OFF_fctrl] & 0xF;
    u1_t portoff = foptslen + OFF_fopts;
    if( portoff > len-4  )
        goto badframe;
    u4_t devaddr = rt_rlsbf4(&frame[OFF_devaddr]);
    #if 0
    u1_t netid = devaddr >> (32-7);
    if( ((1 << (netid & 0x1F)) & s2e_netidFilter[netid>>5]) == 0 ) {
        
        xprintf(lbuf, "DevAddr=%X with NetID=%d filtered", devaddr, netid);
        return 0;
    }
    #endif
    
    u1_t  mhdr  = frame[OFF_mhdr];
    u1_t  fctrl = frame[OFF_fctrl];
    u2_t  fcnt  = rt_rlsbf2(&frame[OFF_fcnt]);
    s4_t  mic   = (s4_t)rt_rlsbf4(&frame[len-4]);
    str_t dir   = ftype==FRMTYPE_DAUP || ftype==FRMTYPE_DCUP ? "[UPDF]" : "[DNDF]";
    
    lm.fPort=frame[portoff];
    lm.devAddr=(s4_t)devaddr;
    if(basic_station_filter(&gBSFilter, &lm)){
		LOG(MOD_S2E|INFO, "%s is Filted by filter FPort:[%d], DevAddr=%08X", dir, lm.fPort, lm.devAddr);
		return 0;
    }
    uj_encKVn(buf,
              "msgtype",   's', dir,
              "MHdr",      'i', mhdr,
              "DevAddr",   'i', (s4_t)devaddr,
              "FCtrl",     'i', fctrl,
              "FCnt",      'i', fcnt,
              "FOpts",     'H', foptslen, &frame[OFF_fopts],
              "FPort",     'i', portoff == len-4 ? -1 : frame[portoff],
              "FRMPayload",'H', max(0, len-5-portoff), &frame[portoff+1],
              "MIC",       'i', mic,
              NULL);
   	LOG(MOD_S2E|INFO, "%s mhdr=%02X DevAddr=%08X FCtrl=%02X FCnt=%d FOpts=[%H] %4.2H mic=%d (%d bytes)",
            dir, mhdr, devaddr, fctrl, fcnt,
            foptslen, &frame[OFF_fopts],
            max(0, len-4-portoff), &frame[portoff], mic, len);
    return 1;
}


static int crc16_no_table(uint8_t* pdu, int len) {
    uint32_t remainder = 0;
    uint32_t polynomial = 0x1021;
    for( int i=0; i<len; i++ ) {
        remainder ^= pdu[i] << 8;
        for( int bit=0; bit < 8; bit++ ) {
            if( remainder & 0x8000 ) {
                remainder = (remainder << 1) ^ polynomial;
            } else {
                remainder <<= 1;
            }
        }
    }
    return remainder & 0xFFFF;
}



// Pack parameters into a BEACON pdu with the following layout:
//    | 0-n |       4    |  2  |     1    |  3  |  3  | 0-n |  2  |   bytes - all fields little endian
//    | RFU | epoch_secs | CRC | infoDesc | lat | lon | RFU | CRC |
//
void s2e_make_beacon (uint8_t* layout, sL_t epoch_secs, int infodesc, double lat, double lon, uint8_t* pdu) {
    int time_off     = layout[0];
    int infodesc_off = layout[1];
    int bcn_len      = layout[2];
    memset(pdu, 0, bcn_len);
    for( int i=0; i<4; i++ ) 
        pdu[time_off+i] = epoch_secs>>(8*i);
    uint32_t ulon = (uint32_t)(lon / 180 * (1U<<31));
    uint32_t ulat = (uint32_t)(lat /  90 * (1U<<31));
    for( int i=0; i<3; i++ ) {
        pdu[infodesc_off+1+i] = ulon>>(8*i);
        pdu[infodesc_off+4+i] = ulat>>(8*i);
    } 
    pdu[infodesc_off] = infodesc;
    int crc1 = crc16_no_table(&pdu[0],infodesc_off-2);
    int crc2 = crc16_no_table(&pdu[infodesc_off], bcn_len-2-infodesc_off);
    for( int i=0; i<2; i++ ) {
        pdu[infodesc_off-2+i] = crc1>>(8*i);
        pdu[bcn_len-2+i]      = crc2>>(8*i);
    }
}
