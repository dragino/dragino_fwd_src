/*!>
Description: LoRa MAC layer implementation

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: skerlan
*/
#include <stdlib.h>
#include <stdint.h>
#include "utilities.h"

#include "aes.h"
#include "cmac.h"

#include "loramac-crypto.h"

/*!>!
 * CMAC/AES Message Integrity Code (MIC) Block B0 size
 */
#define LORAMAC_MIC_BLOCK_B0_SIZE                   16

/*!>!
 * MIC field computation initial data
 */
static uint8_t MicBlockB0[] = { 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                              };

/*!>!
 * Contains the computed MIC field.
 *
 * \remark Only the 4 first bytes are used
 */
static uint8_t Mic[16];

/*!>!
 * Encryption aBlock and sBlock
 */
static uint8_t aBlock[] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                          };
static uint8_t sBlock[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                          };

/*!>!
 * AES computation context variable
 */
static aes_context AesContext;

/*!>!
 * CMAC computation context variable
 */
static AES_CMAC_CTX AesCmacCtx[1];

/*!>!
 * \brief Computes the LoRaMAC frame MIC field  
 *
 * \param [IN]  buffer          Data buffer
 * \param [IN]  size            Data buffer size
 * \param [IN]  key             AES key to be used
 * \param [IN]  address         Frame address
 * \param [IN]  dir             Frame direction [0: uplink, 1: downlink]
 * \param [IN]  sequenceCounter Frame sequence counter
 * \param [OUT] mic Computed MIC field
 */
void LoRaMacComputeMic( const uint8_t *buffer, uint16_t size, const uint8_t *key, uint32_t address, uint8_t dir, uint32_t sequenceCounter, uint32_t *mic )
{
    MicBlockB0[5] = dir;
    
    MicBlockB0[6] = ( address ) & 0xFF;
    MicBlockB0[7] = ( address >> 8 ) & 0xFF;
    MicBlockB0[8] = ( address >> 16 ) & 0xFF;
    MicBlockB0[9] = ( address >> 24 ) & 0xFF;

    MicBlockB0[10] = ( sequenceCounter ) & 0xFF;
    MicBlockB0[11] = ( sequenceCounter >> 8 ) & 0xFF;
    MicBlockB0[12] = ( sequenceCounter >> 16 ) & 0xFF;
    MicBlockB0[13] = ( sequenceCounter >> 24 ) & 0xFF;

    MicBlockB0[15] = size & 0xFF;

    AES_CMAC_Init( AesCmacCtx );

    AES_CMAC_SetKey( AesCmacCtx, key );

    AES_CMAC_Update( AesCmacCtx, MicBlockB0, LORAMAC_MIC_BLOCK_B0_SIZE );
    
    AES_CMAC_Update( AesCmacCtx, buffer, size & 0xFF );
    
    AES_CMAC_Final( Mic, AesCmacCtx );
    
    *mic = ( uint32_t )( ( uint32_t )Mic[3] << 24 | ( uint32_t )Mic[2] << 16 | ( uint32_t )Mic[1] << 8 | ( uint32_t )Mic[0] );
}

void LoRaMacPayloadEncrypt( const uint8_t *buffer, uint16_t size, const uint8_t *key, uint32_t address, uint8_t dir, uint32_t sequenceCounter, uint8_t *encBuffer )
{
    uint16_t i;
    uint8_t bufferIndex = 0;
    uint16_t ctr = 1;

    lgw_memset( AesContext.ksch, '\0', 240 );
    aes_set_key( key, 16, &AesContext );

    aBlock[5] = dir;

    aBlock[6] = ( address ) & 0xFF;
    aBlock[7] = ( address >> 8 ) & 0xFF;
    aBlock[8] = ( address >> 16 ) & 0xFF;
    aBlock[9] = ( address >> 24 ) & 0xFF;

    aBlock[10] = ( sequenceCounter ) & 0xFF;
    aBlock[11] = ( sequenceCounter >> 8 ) & 0xFF;
    aBlock[12] = ( sequenceCounter >> 16 ) & 0xFF;
    aBlock[13] = ( sequenceCounter >> 24 ) & 0xFF;

    /*!> FOR DEBUG 
    printf("\naBlock:");
    for (i = 0; i < 16; i++) {
        printf("%02X", aBlock[i]);
    }
    printf("\n+++++++++++++++++++++++++++++++++++++++\n");

    printf("\nSKEY:");
    for (i = 0; i < 16; i++) {
        printf("%02X", key[i]);
    }
    printf("\n+++++++++++++++++++++++++++++++++++++++\n");
    */

    while( size >= 16 )
    {
        aBlock[15] = ( ( ctr ) & 0xFF );
        ctr++;
        aes_encrypt( aBlock, sBlock, &AesContext );
        for( i = 0; i < 16; i++ )
        {
            encBuffer[bufferIndex + i] = buffer[bufferIndex + i] ^ sBlock[i];
        }
        size -= 16;
        bufferIndex += 16;
    }

    if( size > 0 )
    {
        aBlock[15] = ( ( ctr ) & 0xFF );
        aes_encrypt( aBlock, sBlock, &AesContext );
        /*!>
        printf("\nsBlock:");
        for (i = 0; i < 16; i++) {
            printf("%02X", sBlock[i]);
        }
        printf("\n+++++++++++++++++++++++++++++++++++++++\n");
        */
        for( i = 0; i < size; i++ )
        {
            encBuffer[bufferIndex + i] = buffer[bufferIndex + i] ^ sBlock[i];
        }
    }
}

void LoRaMacPayloadDecrypt( const uint8_t *buffer, uint16_t size, const uint8_t *key, uint32_t address, uint8_t dir, uint32_t sequenceCounter, uint8_t *decBuffer )
{
    LoRaMacPayloadEncrypt( buffer, size, key, address, dir, sequenceCounter, decBuffer );
}

void LoRaMacJoinComputeMic( const uint8_t *buffer, uint16_t size, const uint8_t *key, uint32_t *mic )
{
    AES_CMAC_Init( AesCmacCtx );

    AES_CMAC_SetKey( AesCmacCtx, key );

    AES_CMAC_Update( AesCmacCtx, buffer, size & 0xFF );

    AES_CMAC_Final( Mic, AesCmacCtx );

    *mic = ( uint32_t )( ( uint32_t )Mic[3] << 24 | ( uint32_t )Mic[2] << 16 | ( uint32_t )Mic[1] << 8 | ( uint32_t )Mic[0] );
}

void LoRaMacJoinDecrypt( const uint8_t *buffer, uint16_t size, const uint8_t *key, uint8_t *decBuffer )
{
    lgw_memset( AesContext.ksch, '\0', 240 );
    aes_set_key( key, 16, &AesContext );
    aes_decrypt( buffer, decBuffer, &AesContext );
    // Check if optional CFList is included
    if( size >= 16 )
    {
        aes_decrypt( buffer + 16, decBuffer + 16, &AesContext ); }
}

void LoRaMacJoinComputeSKeys( const uint8_t *key, const uint8_t *appNonce, uint16_t devNonce, uint8_t *nwkSKey, uint8_t *appSKey )
{
    uint8_t nonce[16];
    uint8_t *pDevNonce = ( uint8_t * )&devNonce;
    
    lgw_memset( AesContext.ksch, '\0', 240 );
    aes_set_key( key, 16, &AesContext );

    lgw_memset( nonce, 0, sizeof( nonce ) );
    nonce[0] = 0x01;
    /*!>
    nonce[1] = appNonce[2];
    nonce[2] = appNonce[1];
    nonce[3] = appNonce[0];
    nonce[4] = appNonce[5];
    nonce[5] = appNonce[4];
    nonce[6] = appNonce[3];
    */
    lgw_memcpy( nonce + 1, appNonce, 6 );
#ifdef BIGENDIAN
    nonce[7] = pDevNonce[1];
    nonce[8] = pDevNonce[0];
#else
    nonce[7] = pDevNonce[0];
    nonce[8] = pDevNonce[1];
#endif
    aes_encrypt( nonce, nwkSKey, &AesContext );

    lgw_memset( nonce, 0, sizeof( nonce ) );
    nonce[0] = 0x02;
    lgw_memcpy( nonce + 1, appNonce, 6 );
#ifdef BIGENDIAN
    nonce[7] = pDevNonce[1];
    nonce[8] = pDevNonce[0];
#else
    nonce[7] = pDevNonce[0];
    nonce[8] = pDevNonce[1];
#endif
    aes_encrypt( nonce, appSKey, &AesContext );
}
void LoRaMacJoinEncrypt( const uint8_t *buffer, uint16_t size, const uint8_t *key, uint8_t *encBuffer ){
	LoRaMacJoinDecrypt(buffer,size,key,encBuffer);
}
