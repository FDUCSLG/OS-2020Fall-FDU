// Borrow from https://github.com/moizumi99/RPiHaribote/blob/master/sdcard.c 
// For more discussion see: https://github.com/bztsrc/raspi3-tutorial/issues/18

// Taken from here
// https://www.raspberrypi.org/forums/viewtopic.php?f=72&t=94133
// Update: Control2 is removed according the the advice from LdB. Thanks a lot.
//
// Notes:
// VOLTAGE_SWITCH: Haven't yet got this to work.  Command appears to work but dat0-3 go low and stay there.
// Data transfer notes:
// The EMMC module restricts the maximum block size to the size of the internal data FIFO which is 1k bytes
// 0x80  Extension FIFO config - what's that?
// This register allows fine tuning the dma_req generation for paced DMA transfers when reading from the card.

#include "sd.h"

#include "string.h"
#include "arm.h"
#include "peripherals/gpio.h"
#include "peripherals/mbox.h"
#include "console.h"

#include "proc.h"

// Private functions.
static void sd_start(struct buf *b);
static void sd_delayus(uint32_t cnt);
static int sdInit();
static void sdParseCID();
static void sdParseCSD();
static int sdSendCommand(int index);
static int sdSendCommandA(int index, int arg);
static int sdWaitForInterrupt(unsigned int mask);
static int sdWaitForData();
int fls_long (unsigned long x);

// EMMC registers
#define EMMC_ARG2           ((volatile unsigned int*)(MMIO_BASE+0x00300000))
#define EMMC_BLKSIZECNT     ((volatile unsigned int*)(MMIO_BASE+0x00300004))
#define EMMC_ARG1           ((volatile unsigned int*)(MMIO_BASE+0x00300008))
#define EMMC_CMDTM          ((volatile unsigned int*)(MMIO_BASE+0x0030000C))
#define EMMC_RESP0          ((volatile unsigned int*)(MMIO_BASE+0x00300010))
#define EMMC_RESP1          ((volatile unsigned int*)(MMIO_BASE+0x00300014))
#define EMMC_RESP2          ((volatile unsigned int*)(MMIO_BASE+0x00300018))
#define EMMC_RESP3          ((volatile unsigned int*)(MMIO_BASE+0x0030001C))
#define EMMC_DATA           ((volatile unsigned int*)(MMIO_BASE+0x00300020))
#define EMMC_STATUS         ((volatile unsigned int*)(MMIO_BASE+0x00300024))
#define EMMC_CONTROL0       ((volatile unsigned int*)(MMIO_BASE+0x00300028))
#define EMMC_CONTROL1       ((volatile unsigned int*)(MMIO_BASE+0x0030002C))
#define EMMC_INTERRUPT      ((volatile unsigned int*)(MMIO_BASE+0x00300030))
#define EMMC_IRPT_MASK      ((volatile unsigned int*)(MMIO_BASE+0x00300034))
#define EMMC_IRPT_EN        ((volatile unsigned int*)(MMIO_BASE+0x00300038))
#define EMMC_CONTROL2       ((volatile unsigned int*)(MMIO_BASE+0x0030003C))
// #define EMMC_BOOT_TIMEOUT   ((volatile unsigned int*)(MMIO_BASE+0x00300070))
// #define EMMC_EXRDFIFO_EN    ((volatile unsigned int*)(MMIO_BASE+0x00300084))
// #define EMMC_SPI_INT_SPT    ((volatile unsigned int*)(MMIO_BASE+0x003000f0))
#define EMMC_SLOTISR_VER    ((volatile unsigned int*)(MMIO_BASE+0x003000fc))

// This register is not available on the Pi.
// #define EMMC_HOST_CAPS      ((volatile unsigned int*)(MMIO_BASE+0x00300040))

// EMMC command flags
#define CMD_TYPE_NORMAL  0x00000000
#define CMD_TYPE_SUSPEND 0x00400000
#define CMD_TYPE_RESUME  0x00800000
#define CMD_TYPE_ABORT   0x00c00000
#define CMD_IS_DATA      0x00200000
#define CMD_IXCHK_EN     0x00100000
#define CMD_CRCCHK_EN    0x00080000
#define CMD_RSPNS_NO     0x00000000
#define CMD_RSPNS_136    0x00010000
#define CMD_RSPNS_48     0x00020000
#define CMD_RSPNS_48B    0x00030000
#define TM_MULTI_BLOCK   0x00000020
#define TM_DAT_DIR_HC    0x00000000
#define TM_DAT_DIR_CH    0x00000010
#define TM_AUTO_CMD23    0x00000008
#define TM_AUTO_CMD12    0x00000004
#define TM_BLKCNT_EN     0x00000002
#define TM_MULTI_DATA    (CMD_IS_DATA|TM_MULTI_BLOCK|TM_BLKCNT_EN)

// INTERRUPT register settings
#define INT_AUTO_ERROR   0x01000000
#define INT_DATA_END_ERR 0x00400000
#define INT_DATA_CRC_ERR 0x00200000
#define INT_DATA_TIMEOUT 0x00100000
#define INT_INDEX_ERROR  0x00080000
#define INT_END_ERROR    0x00040000
#define INT_CRC_ERROR    0x00020000
#define INT_CMD_TIMEOUT  0x00010000
#define INT_ERR          0x00008000
#define INT_ENDBOOT      0x00004000
#define INT_BOOTACK      0x00002000
#define INT_RETUNE       0x00001000
#define INT_CARD         0x00000100
#define INT_READ_RDY     0x00000020
#define INT_WRITE_RDY    0x00000010
#define INT_BLOCK_GAP    0x00000004
#define INT_DATA_DONE    0x00000002
#define INT_CMD_DONE     0x00000001
#define INT_ERROR_MASK   (INT_CRC_ERROR|INT_END_ERROR|INT_INDEX_ERROR|  \
                          INT_DATA_TIMEOUT|INT_DATA_CRC_ERR|INT_DATA_END_ERR| \
                          INT_ERR|INT_AUTO_ERROR)
#define INT_ALL_MASK     (INT_CMD_DONE|INT_DATA_DONE|INT_READ_RDY|INT_WRITE_RDY|INT_ERROR_MASK)

// CONTROL register settings
#define C0_SPI_MODE_EN   0x00100000
#define C0_HCTL_HS_EN    0x00000004
#define C0_HCTL_DWITDH   0x00000002

#define C1_SRST_DATA     0x04000000
#define C1_SRST_CMD      0x02000000
#define C1_SRST_HC       0x01000000
#define C1_TOUNIT_DIS    0x000f0000
#define C1_TOUNIT_MAX    0x000e0000
#define C1_CLK_GENSEL    0x00000020
#define C1_CLK_EN        0x00000004
#define C1_CLK_STABLE    0x00000002
#define C1_CLK_INTLEN    0x00000001

#define FREQ_SETUP           400000  // 400 Khz
#define FREQ_NORMAL        25000000  // 25 Mhz

// CONTROL2 values
#define C2_VDD_18        0x00080000
#define C2_UHSMODE       0x00070000
#define C2_UHS_SDR12     0x00000000
#define C2_UHS_SDR25     0x00010000
#define C2_UHS_SDR50     0x00020000
#define C2_UHS_SDR104    0x00030000
#define C2_UHS_DDR50     0x00040000

// SLOTISR_VER values
#define HOST_SPEC_NUM              0x00ff0000
#define HOST_SPEC_NUM_SHIFT        16
#define HOST_SPEC_V3               2
#define HOST_SPEC_V2               1
#define HOST_SPEC_V1               0

// STATUS register settings
#define SR_DAT_LEVEL1        0x1e000000
#define SR_CMD_LEVEL         0x01000000
#define SR_DAT_LEVEL0        0x00f00000
#define SR_DAT3              0x00800000
#define SR_DAT2              0x00400000
#define SR_DAT1              0x00200000
#define SR_DAT0              0x00100000
#define SR_WRITE_PROT        0x00080000  // From SDHC spec v2, BCM says reserved
#define SR_READ_AVAILABLE    0x00000800  // ???? undocumented
#define SR_WRITE_AVAILABLE   0x00000400  // ???? undocumented
#define SR_READ_TRANSFER     0x00000200
#define SR_WRITE_TRANSFER    0x00000100
#define SR_DAT_ACTIVE        0x00000004
#define SR_DAT_INHIBIT       0x00000002
#define SR_CMD_INHIBIT       0x00000001

// Arguments for specific commands.
// TODO: What's the correct voltage window for the RPi SD interface?
// 2.7v-3.6v (given by 0x00ff8000) or something narrower?
// TODO: For now, don't offer to switch voltage.
#define ACMD41_HCS           0x40000000
#define ACMD41_SDXC_POWER    0x10000000
#define ACMD41_S18R          0x01000000
#define ACMD41_VOLTAGE       0x00ff8000
#define ACMD41_ARG_HC        (ACMD41_HCS|ACMD41_SDXC_POWER|ACMD41_VOLTAGE|ACMD41_S18R)
#define ACMD41_ARG_SC        (ACMD41_VOLTAGE|ACMD41_S18R)

// R1 (Status) values
#define ST_OUT_OF_RANGE      0x80000000  // 31   E
#define ST_ADDRESS_ERROR     0x40000000  // 30   E
#define ST_BLOCK_LEN_ERROR   0x20000000  // 29   E
#define ST_ERASE_SEQ_ERROR   0x10000000  // 28   E
#define ST_ERASE_PARAM_ERROR 0x08000000  // 27   E
#define ST_WP_VIOLATION      0x04000000  // 26   E
#define ST_CARD_IS_LOCKED    0x02000000  // 25   E
#define ST_LOCK_UNLOCK_FAIL  0x01000000  // 24   E
#define ST_COM_CRC_ERROR     0x00800000  // 23   E
#define ST_ILLEGAL_COMMAND   0x00400000  // 22   E
#define ST_CARD_ECC_FAILED   0x00200000  // 21   E
#define ST_CC_ERROR          0x00100000  // 20   E
#define ST_ERROR             0x00080000  // 19   E
#define ST_CSD_OVERWRITE     0x00010000  // 16   E
#define ST_WP_ERASE_SKIP     0x00008000  // 15   E
#define ST_CARD_ECC_DISABLED 0x00004000  // 14   E
#define ST_ERASE_RESET       0x00002000  // 13   E
#define ST_CARD_STATE        0x00001e00  // 12:9
#define ST_READY_FOR_DATA    0x00000100  // 8
#define ST_APP_CMD           0x00000020  // 5
#define ST_AKE_SEQ_ERROR     0x00000004  // 3    E

#define R1_CARD_STATE_SHIFT  9
#define R1_ERRORS_MASK       0xfff9c004  // All above bits which indicate errors.

// R3 (ACMD41 APP_SEND_OP_COND)
#define R3_COMPLETE    0x80000000
#define R3_CCS         0x40000000
#define R3_S18A        0x01000000

// R6 (CMD3 SEND_REL_ADDR)
#define R6_RCA_MASK    0xffff0000
#define R6_ERR_MASK    0x0000e000
#define R6_STATE_MASK  0x00001e00

// Card state values as they appear in the status register.
#define CS_IDLE    0 // 0x00000000
#define CS_READY   1 // 0x00000200
#define CS_IDENT   2 // 0x00000400
#define CS_STBY    3 // 0x00000600
#define CS_TRAN    4 // 0x00000800
#define CS_DATA    5 // 0x00000a00
#define CS_RCV     6 // 0x00000c00
#define CS_PRG     7 // 0x00000e00
#define CS_DIS     8 // 0x00001000

// Response types.
// Note that on the PI, the index and CRC are dropped, leaving 32 bits in RESP0.
#define RESP_NO    0     // No response
#define RESP_R1    1     // 48  RESP0    contains card status
#define RESP_R1b  11     // 48  RESP0    contains card status, data line indicates busy
#define RESP_R2I   2     // 136 RESP0..3 contains 128 bit CID shifted down by 8 bits as no CRC
#define RESP_R2S  12     // 136 RESP0..3 contains 128 bit CSD shifted down by 8 bits as no CRC
#define RESP_R3    3     // 48  RESP0    contains OCR register
#define RESP_R6    6     // 48  RESP0    contains RCA and status bits 23,22,19,12:0
#define RESP_R7    7     // 48  RESP0    contains voltage acceptance and check pattern

#define RCA_NO     1
#define RCA_YES    2

typedef struct EMMCCommand
{
  const char* name;
  unsigned int code;
  unsigned char resp;
  unsigned char rca;
  int delay;
} EMMCCommand;

// Command table.
// TODO: TM_DAT_DIR_CH required in any of these?
static EMMCCommand sdCommandTable[] =
  {
    { "GO_IDLE_STATE", 0x00000000|CMD_RSPNS_NO                             , RESP_NO , RCA_NO  ,0},
    { "ALL_SEND_CID" , 0x02000000|CMD_RSPNS_136                            , RESP_R2I, RCA_NO  ,0},
    { "SEND_REL_ADDR", 0x03000000|CMD_RSPNS_48                             , RESP_R6 , RCA_NO  ,0},
    { "SET_DSR"      , 0x04000000|CMD_RSPNS_NO                             , RESP_NO , RCA_NO  ,0},
    { "SWITCH_FUNC"  , 0x06000000|CMD_RSPNS_48                             , RESP_R1 , RCA_NO  ,0},
    { "CARD_SELECT"  , 0x07000000|CMD_RSPNS_48B                            , RESP_R1b, RCA_YES ,0},
    { "SEND_IF_COND" , 0x08000000|CMD_RSPNS_48                             , RESP_R7 , RCA_NO  ,100},
    { "SEND_CSD"     , 0x09000000|CMD_RSPNS_136                            , RESP_R2S, RCA_YES ,0},
    { "SEND_CID"     , 0x0A000000|CMD_RSPNS_136                            , RESP_R2I, RCA_YES ,0},
    { "VOLT_SWITCH"  , 0x0B000000|CMD_RSPNS_48                             , RESP_R1 , RCA_NO  ,0},
    { "STOP_TRANS"   , 0x0C000000|CMD_RSPNS_48B                            , RESP_R1b, RCA_NO  ,0},
    { "SEND_STATUS"  , 0x0D000000|CMD_RSPNS_48                             , RESP_R1 , RCA_YES ,0},
    { "GO_INACTIVE"  , 0x0F000000|CMD_RSPNS_NO                             , RESP_NO , RCA_YES ,0},
    { "SET_BLOCKLEN" , 0x10000000|CMD_RSPNS_48                             , RESP_R1 , RCA_NO  ,0},
    { "READ_SINGLE"  , 0x11000000|CMD_RSPNS_48 |CMD_IS_DATA  |TM_DAT_DIR_CH, RESP_R1 , RCA_NO  ,0},
    { "READ_MULTI"   , 0x12000000|CMD_RSPNS_48 |TM_MULTI_DATA|TM_DAT_DIR_CH, RESP_R1 , RCA_NO  ,0},
    { "SEND_TUNING"  , 0x13000000|CMD_RSPNS_48                             , RESP_R1 , RCA_NO  ,0},
    { "SPEED_CLASS"  , 0x14000000|CMD_RSPNS_48B                            , RESP_R1b, RCA_NO  ,0},
    { "SET_BLOCKCNT" , 0x17000000|CMD_RSPNS_48                             , RESP_R1 , RCA_NO  ,0},
    { "WRITE_SINGLE" , 0x18000000|CMD_RSPNS_48 |CMD_IS_DATA  |TM_DAT_DIR_HC, RESP_R1 , RCA_NO  ,0},
    { "WRITE_MULTI"  , 0x19000000|CMD_RSPNS_48 |TM_MULTI_DATA|TM_DAT_DIR_HC, RESP_R1 , RCA_NO  ,0},
    { "PROGRAM_CSD"  , 0x1B000000|CMD_RSPNS_48                             , RESP_R1 , RCA_NO  ,0},
    { "SET_WRITE_PR" , 0x1C000000|CMD_RSPNS_48B                            , RESP_R1b, RCA_NO  ,0},
    { "CLR_WRITE_PR" , 0x1D000000|CMD_RSPNS_48B                            , RESP_R1b, RCA_NO  ,0},
    { "SND_WRITE_PR" , 0x1E000000|CMD_RSPNS_48                             , RESP_R1 , RCA_NO  ,0},
    { "ERASE_WR_ST"  , 0x20000000|CMD_RSPNS_48                             , RESP_R1 , RCA_NO  ,0},
    { "ERASE_WR_END" , 0x21000000|CMD_RSPNS_48                             , RESP_R1 , RCA_NO  ,0},
    { "ERASE"        , 0x26000000|CMD_RSPNS_48B                            , RESP_R1b, RCA_NO  ,0},
    { "LOCK_UNLOCK"  , 0x2A000000|CMD_RSPNS_48                             , RESP_R1 , RCA_NO  ,0},
    { "APP_CMD"      , 0x37000000|CMD_RSPNS_NO                             , RESP_NO , RCA_NO  ,100},
    { "APP_CMD"      , 0x37000000|CMD_RSPNS_48                             , RESP_R1 , RCA_YES ,0},
    { "GEN_CMD"      , 0x38000000|CMD_RSPNS_48                             , RESP_R1 , RCA_NO  ,0},

    // APP commands must be prefixed by an APP_CMD.
    { "SET_BUS_WIDTH", 0x06000000|CMD_RSPNS_48                             , RESP_R1 , RCA_NO  ,0},
    { "SD_STATUS"    , 0x0D000000|CMD_RSPNS_48                             , RESP_R1 , RCA_YES ,0}, // RCA???
    { "SEND_NUM_WRBL", 0x16000000|CMD_RSPNS_48                             , RESP_R1 , RCA_NO  ,0},
    { "SEND_NUM_ERS" , 0x17000000|CMD_RSPNS_48                             , RESP_R1 , RCA_NO  ,0},
    { "SD_SENDOPCOND", 0x29000000|CMD_RSPNS_48                             , RESP_R3 , RCA_NO  ,1000},
    { "SET_CLR_DET"  , 0x2A000000|CMD_RSPNS_48                             , RESP_R1 , RCA_NO  ,0},
    { "SEND_SCR"     , 0x33000000|CMD_RSPNS_48|CMD_IS_DATA|TM_DAT_DIR_CH   , RESP_R1 , RCA_NO  ,0},
  };

// Command indexes in the command table
#define IX_GO_IDLE_STATE    0
#define IX_ALL_SEND_CID     1
#define IX_SEND_REL_ADDR    2
#define IX_SET_DSR          3
#define IX_SWITCH_FUNC      4
#define IX_CARD_SELECT      5
#define IX_SEND_IF_COND     6
#define IX_SEND_CSD         7
#define IX_SEND_CID         8
#define IX_VOLTAGE_SWITCH   9
#define IX_STOP_TRANS       10
#define IX_SEND_STATUS      11
#define IX_GO_INACTIVE      12
#define IX_SET_BLOCKLEN     13
#define IX_READ_SINGLE      14
#define IX_READ_MULTI       15
#define IX_SEND_TUNING      16
#define IX_SPEED_CLASS      17
#define IX_SET_BLOCKCNT     18
#define IX_WRITE_SINGLE     19
#define IX_WRITE_MULTI      20
#define IX_PROGRAM_CSD      21
#define IX_SET_WRITE_PR     22
#define IX_CLR_WRITE_PR     23
#define IX_SND_WRITE_PR     24
#define IX_ERASE_WR_ST      25
#define IX_ERASE_WR_END     26
#define IX_ERASE            27
#define IX_LOCK_UNLOCK      28
#define IX_APP_CMD          29
#define IX_APP_CMD_RCA      30  // APP_CMD used once we have the RCA.
#define IX_GEN_CMD          31

// Commands hereafter require APP_CMD.
#define IX_APP_CMD_START    32
#define IX_SET_BUS_WIDTH    32
#define IX_SD_STATUS        33
#define IX_SEND_NUM_WRBL    34
#define IX_SEND_NUM_ERS     35
#define IX_APP_SEND_OP_COND 36
#define IX_SET_CLR_DET      37
#define IX_SEND_SCR         38

static const char* STATUS_NAME[] =
  { "idle", "ready", "identify", "standby", "transmit", "data", "receive", "prog", "dis" };

// CSD flags
// Note: all flags are shifted down by 8 bits as the CRC is not included.
// Most flags are common:
// in V1 the size is 12 bits with a 3 bit multiplier.
// in V1 currents for read and write are specified.
// in V2 the size is 22 bits, no multiplier, no currents.
#define CSD0_VERSION               0x00c00000
#define CSD0_V1                    0x00000000
#define CSD0_V2                    0x00400000

// CSD Version 1 and 2 flags
#define CSD1VN_TRAN_SPEED          0xff000000

#define CSD1VN_CCC                 0x00fff000
#define CSD1VN_READ_BL_LEN         0x00000f00
#define CSD1VN_READ_BL_LEN_SHIFT   8
#define CSD1VN_READ_BL_PARTIAL     0x00000080
#define CSD1VN_WRITE_BLK_MISALIGN  0x00000040
#define CSD1VN_READ_BLK_MISALIGN   0x00000020
#define CSD1VN_DSR_IMP             0x00000010

#define CSD2VN_ERASE_BLK_EN        0x00000040
#define CSD2VN_ERASE_SECTOR_SIZEH  0x0000003f
#define CSD3VN_ERASE_SECTOR_SIZEL  0x80000000

#define CSD3VN_WP_GRP_SIZE         0x7f000000

#define CSD3VN_WP_GRP_ENABLE       0x00800000
#define CSD3VN_R2W_FACTOR          0x001c0000
#define CSD3VN_WRITE_BL_LEN        0x0003c000
#define CSD3VN_WRITE_BL_LEN_SHIFT  14
#define CSD3VN_WRITE_BL_PARTIAL    0x00002000
#define CSD3VN_FILE_FORMAT_GROUP   0x00000080
#define CSD3VN_COPY                0x00000040
#define CSD3VN_PERM_WRITE_PROT     0x00000020
#define CSD3VN_TEMP_WRITE_PROT     0x00000010
#define CSD3VN_FILE_FORMAT         0x0000000c
#define CSD3VN_FILE_FORMAT_HDD     0x00000000
#define CSD3VN_FILE_FORMAT_DOSFAT  0x00000004
#define CSD3VN_FILE_FORMAT_UFF     0x00000008
#define CSD3VN_FILE_FORMAT_UNKNOWN 0x0000000c

// CSD Version 1 flags.
#define CSD1V1_C_SIZEH             0x00000003
#define CSD1V1_C_SIZEH_SHIFT       10

#define CSD2V1_C_SIZEL             0xffc00000
#define CSD2V1_C_SIZEL_SHIFT       22
#define CSD2V1_VDD_R_CURR_MIN      0x00380000
#define CSD2V1_VDD_R_CURR_MAX      0x00070000
#define CSD2V1_VDD_W_CURR_MIN      0x0000e000
#define CSD2V1_VDD_W_CURR_MAX      0x00001c00
#define CSD2V1_C_SIZE_MULT         0x00000380
#define CSD2V1_C_SIZE_MULT_SHIFT   7

// CSD Version 2 flags.
#define CSD2V2_C_SIZE              0x3fffff00
#define CSD2V2_C_SIZE_SHIFT        8

// SCR flags
// NOTE: SCR is big-endian, so flags appear byte-wise reversed from the spec.
#define SCR_STRUCTURE              0x000000f0
#define SCR_STRUCTURE_V1           0x00000000

#define SCR_SD_SPEC                0x0000000f
#define SCR_SD_SPEC_1_101          0x00000000
#define SCR_SD_SPEC_11             0x00000001
#define SCR_SD_SPEC_2_3            0x00000002

#define SCR_DATA_AFTER_ERASE       0x00008000

#define SCR_SD_SECURITY            0x00007000
#define SCR_SD_SEC_NONE            0x00000000
#define SCR_SD_SEC_NOT_USED        0x00001000
#define SCR_SD_SEC_101             0x00002000  // SDSC
#define SCR_SD_SEC_2               0x00003000  // SDHC
#define SCR_SD_SEC_3               0x00004000  // SDXC

#define SCR_SD_BUS_WIDTHS          0x00000f00
#define SCR_SD_BUS_WIDTH_1         0x00000100
#define SCR_SD_BUS_WIDTH_4         0x00000400

#define SCR_SD_SPEC3               0x00800000
#define SCR_SD_SPEC_2              0x00000000
#define SCR_SD_SPEC_3              0x00100000

#define SCR_EX_SECURITY            0x00780000

#define SCR_CMD_SUPPORT            0x03000000
#define SCR_CMD_SUPP_SET_BLKCNT    0x02000000
#define SCR_CMD_SUPP_SPEED_CLASS   0x01000000

// Capabilities registers.  Not supported by the Pi.
/*
  #define EMMC_HC_V18_SUPPORTED      0x04000000
  #define EMMC_HC_V30_SUPPORTED      0x02000000
  #define EMMC_HC_V33_SUPPORTED      0x01000000
  #define EMMC_HC_SUSPEND            0x00800000
  #define EMMC_HC_SDMA               0x00400000
  #define EMMC_HC_HIGH_SPEED         0x00200000
  #define EMMC_HC_ADMA2              0x00080000
  #define EMMC_HC_MAX_BLOCK          0x00030000
  #define EMMC_HC_MAX_BLOCK_SHIFT    16
  #define EMMC_HC_BASE_CLOCK_FREQ    0x00003f00  // base clock frequency in units of 1MHz, range 10-63Mhz.
  #define EMMC_HC_BASE_CLOCK_FREQ_V3 0x0000ff00  // base clock frequency in units of 1MHz, range 10-255Mhz.
  #define EMMC_HC_BASE_CLOCK_SHIFT   8
  #define EMMC_HC_TOCLOCK_MHZ        0x00000080
  #define EMMC_HC_TOCLOCK_FREQ       0x0000003f  // timeout clock frequency in MHz or KHz, range 1-63
  #define EMMC_HC_TOCLOCK_SHIFT      0
*/

// SD card types
#define SD_TYPE_MMC  1
#define SD_TYPE_1    2
#define SD_TYPE_2_SC 3
#define SD_TYPE_2_HC 4

static const char* SD_TYPE_NAME[] =
  {
    "Unknown", "MMC", "Type 1", "Type 2 SC", "Type 2 HC"
  };

// SD card functions supported values.
#define SD_SUPP_SET_BLOCK_COUNT 0x80000000
#define SD_SUPP_SPEED_CLASS     0x40000000
#define SD_SUPP_BUS_WIDTH_4     0x20000000
#define SD_SUPP_BUS_WIDTH_1     0x10000000


// SD card descriptor
typedef struct SDDescriptor {
  // Static information about the SD Card.
  unsigned long long capacity;
  unsigned int cid[4];
  unsigned int csd[4];
  unsigned int scr[2];
  unsigned int ocr;
  unsigned int support;
  unsigned int fileFormat;
  unsigned char type;
  unsigned char uhsi;
  unsigned char init;
  unsigned char absent;

  // Dynamic information.
  unsigned int rca;
  unsigned int cardState;
  unsigned int status;

  EMMCCommand* lastCmd;
  unsigned int lastArg;
} SDDescriptor;

static SDDescriptor sdCard;

static int sdHostVer = 0;
static int sdDebug = 0;
static int sdBaseClock;

#define MBX_PROP_CLOCK_EMMC 1

/*
 * Initialize SD card and parse MBR.
 * 1. The first partition should be FAT and is used for booting.
 * 2. The second partition is used by our file system.
 *
 * See https://en.wikipedia.org/wiki/Master_boot_record
 */
void
sd_init()
{
    /*
     * Initialize the lock and request queue if any.
     * Remember to call sd_init() at somewhere.
     */
    /* TODO: Your code here. */

    sdInit();
    assert(sdCard.init);

    /*
     * Read and parse 1st block (MBR) and collect whatever
     * information you want.
     *
     * Hint: Maybe need to use sd_start for reading, and
     * sdWaitForInterrupt for clearing certain interrupt.
     */

    /* TODO: Your code here. */
}

static void
sd_delayus(uint32_t c)
{
    // Delay 3 times longer on rpi3.
    delayus(c*3);
}

/* Start the request for b. Caller must hold sdlock. */
static void
sd_start(struct buf *b)
{
    // Address is different depending on the card type.
    // HC pass address as block #.
    // SC pass address straight through.
    int bno = sdCard.type == SD_TYPE_2_HC ? b->blockno : b->blockno << 9;
    int write = b->flags & B_DIRTY;

    cprintf("- sd start: cpu %d, flag 0x%x, bno %d, write=%d\n", cpuid(), b->flags, bno, write);

    disb();
    // Ensure that any data operation has completed before doing the transfer.
    asserts(!*EMMC_INTERRUPT, "emmc interrupt flag should be empty: 0x%x. ", *EMMC_INTERRUPT);
    disb();

    // Work out the status, interrupt and command values for the transfer.
    int cmd = write ? IX_WRITE_SINGLE : IX_READ_SINGLE;

    int resp;
    *EMMC_BLKSIZECNT = 512;
    if ((resp = sdSendCommandA(cmd, bno))) {
        panic("* EMMC send command error.");
    }

    int done = 0;
    uint32_t *intbuf = (uint32_t *)b->data;
    asserts((((int64_t)b->data) & 0x03) == 0, "Only support word-aligned buffers. ");

    if (write) {
        // Wait for ready interrupt for the next block.
        if ((resp = sdWaitForInterrupt(INT_WRITE_RDY))) {
            panic("* EMMC ERROR: Timeout waiting for ready to write\n");
            // return sdDebugResponse(resp);
        }
        asserts(!*EMMC_INTERRUPT, "%d ", *EMMC_INTERRUPT);
        while (done < 128) *EMMC_DATA = intbuf[done++];
    }
}

/* The interrupt handler. */
void
sd_intr()
{
    /* TODO: Your code here. */

    /* Hint: Example pseudocode is provided as below. */

    /*
    acquire(&sdlock);
    if (list_empty(&sdque)) {
        cprintf("sd receive redundent interrupt 0x%x, omitted.\n", *EMMC_INTERRUPT);
    } else {
        int i = *EMMC_INTERRUPT;

        // FIXME: Restart when failed
        asserts((i & INT_DATA_DONE) || (i & INT_READ_RDY), "unexpected sd intr");

        *EMMC_INTERRUPT = i; // Clear interrupt.
        disb();

        struct buf *b = list_front(&sdque);
        int write = b->flags & B_DIRTY;
        if (!((write && i == INT_DATA_DONE) || (!write && INT_READ_RDY))) {
            sd_start(b);
            // FIXME: don't panic
            cprintf("sd intr unexpected: 0x%x, restarted.\n", i);
        } else {
            if (!write) {
                uint32_t *intbuf = (uint32_t *)b->data;
                for (int done = 0; done < 128; )
                    intbuf[done++] = *EMMC_DATA;
                sdWaitForInterrupt(INT_DATA_DONE);
            }

            b->flags |= B_VALID;
            b->flags &= ~B_DIRTY;
            wakeup(b);

            list_pop_front(&sdque);
            if (!list_empty(&sdque))
                sd_start(list_front(&sdque));
        }
    }
    release(&sdlock);
    */
}

/*
 * Sync buf with disk.
 * If B_DIRTY is set, write buf to disk, clear B_DIRTY, set B_VALID.
 * Else if B_VALID is not set, read buf from disk, set B_VALID.
 */
void
sdrw(struct buf *b)
{
    /* TODO: Your code here. */

}

/* SD card test and benchmark. */
void
sd_test()
{
    static struct buf b[1 << 11];
    int n = sizeof(b) / sizeof(b[0]);
    int mb = (n * BSIZE) >> 20;
    assert(mb);
    int64_t f, t;
    asm volatile ("mrs %[freq], cntfrq_el0" : [freq]"=r"(f));
    cprintf("- sd test: begin nblocks %d\n", n);

    cprintf("- sd check rw...\n");
    // Read/write test
    for (int i = 1; i < n; i ++) {
        // Backup.
        b[0].flags = 0;
        b[0].blockno = i;
        sdrw(&b[0]);

        // Write some value.
        b[i].flags = B_DIRTY;
        b[i].blockno = i;
        for (int j = 0; j < BSIZE; j++)
            b[i].data[j] = i*j & 0xFF;
        sdrw(&b[i]);

        memset(b[i].data, 0, sizeof(b[i].data));
        // Read back and check
        b[i].flags = 0;
        sdrw(&b[i]);
        for (int j = 0; j < BSIZE; j++)
            assert(b[i].data[j] == (i*j & 0xFF));
        // Restore previous value.
        b[0].flags = B_DIRTY;
        sdrw(&b[0]);
    }

    // Read benchmark
    disb();
    t = timestamp();
    disb();
    for (int i = 0; i < n; i++) {
        b[i].flags = 0;
        b[i].blockno = i;
        sdrw(&b[i]);
    }
    disb();
    t = timestamp() - t;
    disb();
    cprintf("- read %lldB (%lldMB), t: %lld cycles, speed: %lld.%lld MB/s\n",
            n*BSIZE, mb, t, mb * f / t, (mb*f*10/t) % 10);

    // Write benchmark
    disb();
    t = timestamp();
    disb();
    for (int i = 0; i < n; i++) {
        b[i].flags = B_DIRTY;
        b[i].blockno = i;
        sdrw(&b[i]);
    }
    disb();
    t = timestamp() - t;
    disb();

    cprintf("- write %lldB (%lldMB), t: %lld cycles, speed: %lld.%lld MB/s\n",
            n*BSIZE, mb, t, mb * f / t, (mb*f*10/t) % 10);
}

static int
sdDebugResponse(int resp)
{
    cprintf("- EMMC: Command %s resp %x: %x %x %x %x\n", sdCard.lastCmd->name, resp,
            *EMMC_RESP3, *EMMC_RESP2, *EMMC_RESP1, *EMMC_RESP0);
    cprintf("- EMMC: Status: %x, control1: %x, interrupt: %x\n", *EMMC_STATUS,
            *EMMC_CONTROL1, *EMMC_INTERRUPT);
    return resp;
}

/* Wait for interrupt. */
static int
sdWaitForInterrupt(unsigned int mask)
{
    // Wait up to 1 second for the interrupt.
    int count = 1000000;
    int waitMask = mask | INT_ERROR_MASK;
    int ival;

    // Wait for the specified interrupt or any error.
    while (!(*EMMC_INTERRUPT & waitMask) && count--)
        sd_delayus(1);
    ival = *EMMC_INTERRUPT;
    // cprintf("- sd intr 0x%x cost %d loops\n", mask, 1000000 - count);

    // Check for success.
    if (count <= 0 || (ival & INT_CMD_TIMEOUT) || (ival & INT_DATA_TIMEOUT)) {
        // printf("EMMC: Wait for interrupt %08x timeout: %08x %08x %08x\n",mask,*EMMC_STATUS,ival,*EMMC_RESP0);
        // printf("EMMC_STATUS:%08x\nEMMC_INTERRUPT: %08x\nEMMC_RESP0 : %08x\nn", *EMMC_STATUS, *EMMC_INTERRUPT, *EMMC_RESP0);

        // Clear the interrupt register completely.
        *EMMC_INTERRUPT = ival;

        return SD_TIMEOUT;
    } else if (ival & INT_ERROR_MASK) {
        cprintf("* EMMC: Error waiting for interrupt: %x %x %x\n",*EMMC_STATUS,ival,*EMMC_RESP0);

        // Clear the interrupt register completely.
        *EMMC_INTERRUPT = ival;

        return SD_ERROR;
    }

    // Clear the interrupt we were waiting for, leaving any other (non-error) interrupts.
    *EMMC_INTERRUPT = mask;

    return SD_OK;
}

/* Wait for any command that may be in progress. */
static int
sdWaitForCommand()
{
    // Check for status indicating a command in progress.
    int count = 1000000;
    while ((*EMMC_STATUS & SR_CMD_INHIBIT) && !(*EMMC_INTERRUPT & INT_ERROR_MASK) && count--)
        sd_delayus(1);
    if (count <= 0 || (*EMMC_INTERRUPT & INT_ERROR_MASK)) {
        cprintf("* EMMC: Wait for command aborted: %x %x %x\n",*EMMC_STATUS,*EMMC_INTERRUPT,*EMMC_RESP0);
        return SD_BUSY;
    }

    return SD_OK;
}

/* Wait for any data that may be in progress. */
static int
sdWaitForData()
{
    // Check for status indicating data transfer in progress.
    // Spec indicates a maximum wait of 500ms.
    // For now this is done by waiting for the DAT_INHIBIT flag to go from the status register,
    // or until an error is flagged in the interrupt register.
    // printf("EMMC: Wait for data started: %08x %08x %08x; dat: %d\n",*EMMC_STATUS,*EMMC_INTERRUPT,*EMMC_RESP0);
    int count = 0;
    while ((*EMMC_STATUS & SR_DAT_INHIBIT) &&
           !(*EMMC_INTERRUPT & INT_ERROR_MASK) && ++count < 500000)
        sd_delayus(1);
    if (count >= 500000 || (*EMMC_INTERRUPT & INT_ERROR_MASK)) {
        cprintf("* EMMC: Wait for data aborted: %x %x %x\n",
                *EMMC_STATUS, *EMMC_INTERRUPT, *EMMC_RESP0);
        return SD_BUSY;
    }
    // printf("EMMC: Wait for data OK: count = %d: %08x %08x %08x\n",count,*EMMC_STATUS,*EMMC_INTERRUPT,*EMMC_RESP0);

    return SD_OK;
}

/* Send command and handle response. */
static int
sdSendCommandP(EMMCCommand* cmd, int arg)
{
    // Check for command in progress
    if (sdWaitForCommand() != 0) return SD_BUSY;

    if (sdDebug)
        cprintf("- EMMC: Sending command %s code %x arg %x\n",
                cmd->name, cmd->code, arg);

    sdCard.lastCmd = cmd;
    sdCard.lastArg = arg;

    //  printf("EMMC: Sending command %08x:%s arg %d\n",cmd->code,cmd->name,arg);

    int result;

    // Clear interrupt flags.  This is done by setting the ones that are currently set.
    //  printf("EMMC_INTERRUPT before clearing: %08x\n", *EMMC_INTERRUPT);
    *EMMC_INTERRUPT = *EMMC_INTERRUPT;

    // Set the argument and the command code.
    // Some commands require a delay before reading the response.
    //  printf("EMMC_STATUS:%08x\nEMMC_INTERRUPT: %08x\nEMMC_RESP0 : %08x\n", *EMMC_STATUS, *EMMC_INTERRUPT, *EMMC_RESP0);
    // cprintf("- arg: 0x%x, code: 0x%x, delay: %d\n", arg, cmd->code, cmd->delay);
    *EMMC_ARG1 = arg;
    *EMMC_CMDTM = cmd->code;
    if (cmd->delay) sd_delayus(cmd->delay);

    // Wait until command complete interrupt.
    if ((result = sdWaitForInterrupt(INT_CMD_DONE))) return result;

    // Get response from RESP0.
    int resp0 = *EMMC_RESP0;
    // printf("EMMC: Sent command %08x:%s arg %d resp %08x\n",cmd->code,cmd->name,arg,resp0);

    // Handle response types.
    switch (cmd->resp) {
    // No response.
    case RESP_NO:
        return SD_OK;

    // RESP0 contains card status, no other data from the RESP* registers.
    // Return value non-zero if any error flag in the status value.
    case RESP_R1:
    case RESP_R1b:
        sdCard.status = resp0;
        // Store the card state.  Note that this is the state the card was in before the
        // command was accepted, not the new state.
        sdCard.cardState = (resp0 & ST_CARD_STATE) >> R1_CARD_STATE_SHIFT;
        return resp0 & R1_ERRORS_MASK;

    // RESP0..3 contains 128 bit CID or CSD shifted down by 8 bits as no CRC
    // Note: highest bits are in RESP3.
    case RESP_R2I:
    case RESP_R2S:
        sdCard.status = 0;
        unsigned int* data = cmd->resp == RESP_R2I ? sdCard.cid : sdCard.csd;
        data[0] = *EMMC_RESP3;
        data[1] = *EMMC_RESP2;
        data[2] = *EMMC_RESP1;
        data[3] = resp0;
        return SD_OK;

    // RESP0 contains OCR register
    // TODO: What is the correct time to wait for this?
    case RESP_R3:
        sdCard.status = 0;
        sdCard.ocr = resp0;
        return SD_OK;

    // RESP0 contains RCA and status bits 23,22,19,12:0
    case RESP_R6:
        sdCard.rca = resp0 & R6_RCA_MASK;
        sdCard.status = ((resp0 & 0x00001fff))          // 12:0 map directly to status 12:0
                      | ((resp0 & 0x00002000) << 6)     // 13 maps to status 19 ERROR
                      | ((resp0 & 0x00004000) << 8)     // 14 maps to status 22 ILLEGAL_COMMAND
                      | ((resp0 & 0x00008000) << 8);    // 15 maps to status 23 COM_CRC_ERROR
        // Store the card state.  Note that this is the state the card was in before the
        // command was accepted, not the new state.
        sdCard.cardState = (resp0 & ST_CARD_STATE) >> R1_CARD_STATE_SHIFT;
        return sdCard.status & R1_ERRORS_MASK;

    // RESP0 contains voltage acceptance and check pattern, which should match
    // the argument.
    case RESP_R7:
        sdCard.status = 0;
        return resp0 == arg ? SD_OK : SD_ERROR;
    default:
        panic("Unexpected response.");
    }

    return SD_ERROR;
}

/* Send APP_CMD. */
static int
sdSendAppCommand()
{
  int resp;
  // If no RCA, send the APP_CMD and don't look for a response.
  if (!sdCard.rca)
    sdSendCommandP(&sdCommandTable[IX_APP_CMD],0x00000000);

  // If there is an RCA, include that in APP_CMD and check card accepted it.
  else
    {
      if ((resp = sdSendCommandP(&sdCommandTable[IX_APP_CMD_RCA],sdCard.rca))) return sdDebugResponse(resp);
      // Debug - check that status indicates APP_CMD accepted.
      if (!(sdCard.status & ST_APP_CMD))
        return SD_ERROR;
    }

  return SD_OK;
}

/*
 * Send a command with no argument.
 * RCA automatically added if required.
 * APP_CMD sent automatically if required.
 */
static int
sdSendCommand(int index)
{
  // Issue APP_CMD if needed.
  int resp;
  if (index >= IX_APP_CMD_START && (resp = sdSendAppCommand()))
    return sdDebugResponse(resp);

  // Get the command and set RCA if required.
  EMMCCommand* cmd = &sdCommandTable[index];
  int arg = 0;
  if (cmd->rca == RCA_YES)
    arg = sdCard.rca;

  if ((resp = sdSendCommandP(cmd,arg))) return resp;

  // Check that APP_CMD was correctly interpreted.
  if (index >= IX_APP_CMD_START && sdCard.rca && !(sdCard.status & ST_APP_CMD))
    return SD_ERROR_APP_CMD;

  return resp;
}

/*
 * Send a command with a specific argument.
 * APP_CMD sent automatically if required.
 */
static int
sdSendCommandA(int index, int arg)
{
  // Issue APP_CMD if needed.
  int resp;
  if (index >= IX_APP_CMD_START && (resp = sdSendAppCommand()))
    return sdDebugResponse(resp);

  // Get the command and pass the argument through.
  if ((resp = sdSendCommandP(&sdCommandTable[index],arg))) return resp;

  // Check that APP_CMD was correctly interpreted.
  if (index >= IX_APP_CMD_START && sdCard.rca && !(sdCard.status & ST_APP_CMD))
    return SD_ERROR_APP_CMD;

  return resp;
}

/* Read card's SCR. */
static int
sdReadSCR()
{
    // SEND_SCR command is like a READ_SINGLE but for a block of 8 bytes.
    // Ensure that any data operation has completed before reading the block.
    if (sdWaitForData()) return SD_TIMEOUT;

    // Set BLKSIZECNT to 1 block of 8 bytes, send SEND_SCR command
    *EMMC_BLKSIZECNT = (1 << 16) | 8;
    int resp;
    if ((resp = sdSendCommand(IX_SEND_SCR))) return sdDebugResponse(resp);

    // Wait for READ_RDY interrupt.
    if ((resp = sdWaitForInterrupt(INT_READ_RDY))) {
        cprintf("* ERROR EMMC: Timeout waiting for ready to read\n");
        return sdDebugResponse(resp);
    }

    // Allow maximum of 100ms for the read operation.
    int numRead = 0, count = 100000;
    while (numRead < 2) {
        if (*EMMC_STATUS & SR_READ_AVAILABLE)
            sdCard.scr[numRead++] = *EMMC_DATA;
        else {
            sd_delayus(1);
            if (--count == 0) break;
        }
    }

    // If SCR not fully read, the operation timed out.
    if (numRead != 2) {
        cprintf("* ERROR EMMC: SEND_SCR ERR: %x %x %x\n",
                *EMMC_STATUS, *EMMC_INTERRUPT, *EMMC_RESP0);
        cprintf("* EMMC: Reading SCR, only read %d words\n", numRead);
        return SD_TIMEOUT;
    }

    // Parse out the SCR.  Only interested in values in scr[0], scr[1] is mfr specific.
    if (sdCard.scr[0] & SCR_SD_BUS_WIDTH_4) sdCard.support |= SD_SUPP_BUS_WIDTH_4;
    if (sdCard.scr[0] & SCR_SD_BUS_WIDTH_1) sdCard.support |= SD_SUPP_BUS_WIDTH_1;
    if (sdCard.scr[0] & SCR_CMD_SUPP_SET_BLKCNT) sdCard.support |= SD_SUPP_SET_BLOCK_COUNT;
    if (sdCard.scr[0] & SCR_CMD_SUPP_SPEED_CLASS) sdCard.support |= SD_SUPP_SPEED_CLASS;

    return SD_OK;
}

int
fls_long(unsigned long x)
{
    int r = 32;
    if (!x) return 0;
    if (!(x & 0xffff0000u)) {
        x <<= 16;
        r -= 16;
    }
    if (!(x & 0xff000000u)) {
        x <<= 8;
        r -= 8;
    }
    if (!(x & 0xf0000000u)) {
        x <<= 4;
        r -= 4;
    }
    if (!(x & 0xc0000000u)) {
        x <<= 2;
        r -= 2;
    }
    if (!(x & 0x80000000u)) {
        x <<= 1;
        r -= 1;
    }
    return r;
}

unsigned long
roundup_pow_of_two(unsigned long x)
{
  return 1UL << fls_long(x - 1);
}

/*
 * Get the clock divider for the given requested frequency.
 * This is calculated relative to the SD base clock.
 */
static uint32_t
sdGetClockDivider(uint32_t freq)
{
    uint32_t divisor;
    // Pi SD frequency is always 41.66667Mhz on baremetal
    uint32_t closest = 41666666 / freq;

    // Get the raw shiftcount
    uint32_t shiftcount = fls_long(closest - 1);

    // Note the offset of shift by 1 (look at the spec)
    if (shiftcount > 0) shiftcount--;

    // It's only 8 bits maximum on HOST_SPEC_V2
    if (shiftcount > 7) shiftcount = 7;
    // Version 3 take closest
    if (sdHostVer > HOST_SPEC_V2) divisor = closest;
    // Version 2 take power 2
    else divisor = (1 << shiftcount);
   
    if (divisor <= 2) {     // Too dangerous to go for divisor 1 unless you test
        divisor = 2;        // You can't take divisor below 2 on slow cards
        shiftcount = 0;     // Match shift to above just for debug notification
    }

    cprintf("- Divisor selected = %u, pow 2 shift count = %u\n", divisor, shiftcount);
    uint32_t hi = 0;
    if (sdHostVer > HOST_SPEC_V2)
        hi = (divisor & 0x300) >> 2;    // Only 10 bits on Hosts specs above 2
    uint32_t lo = (divisor & 0x0ff);    // Low part always valid
    uint32_t cdiv = (lo << 8) + hi;     // Join and roll to position
    return cdiv;                        // Return cdiv
}

/* Set the SD clock to the given frequency. */
static int
sdSetClock(int freq)
{
    // Wait for any pending inhibit bits
    int count = 100000;
    while ((*EMMC_STATUS & (SR_CMD_INHIBIT|SR_DAT_INHIBIT)) && --count)
        sd_delayus(1);
    if (count <= 0) {
        cprintf("* EMMC ERROR: Set clock: timeout waiting for inhibit flags. Status %08x.\n",*EMMC_STATUS);
        return SD_ERROR_CLOCK;
    }

    // Switch clock off.
    *EMMC_CONTROL1 &= ~C1_CLK_EN;
    sd_delayus(10);

    // Request the new clock setting and enable the clock
    int cdiv = sdGetClockDivider(freq);
    *EMMC_CONTROL1 = (*EMMC_CONTROL1 & 0xffff003f) | cdiv;
    sd_delayus(10);

    // Enable the clock.
    *EMMC_CONTROL1 |= C1_CLK_EN;
    sd_delayus(10);

    // Wait for clock to be stable.
    count = 10000;
    while (!(*EMMC_CONTROL1 & C1_CLK_STABLE) && count--)
        sd_delayus(10);
    if (count <= 0) {
        cprintf("* EMMC: ERROR: failed to get stable clock.\n");
        return SD_ERROR_CLOCK;
    }

    cprintf("- EMMC: Set clock, status 0x%x CONTROL1: 0x%x\n",*EMMC_STATUS,*EMMC_CONTROL1);
    return SD_OK;
}

/* Reset card. */
static int
sdResetCard(int resetType)
{
    int resp, count;

    // Send reset host controller and wait for complete.
    *EMMC_CONTROL0 = 0; // C0_SPI_MODE_EN;
    //  *EMMC_CONTROL2 = 0;
    *EMMC_CONTROL1 |= resetType;
    //*EMMC_CONTROL1 &= ~(C1_CLK_EN|C1_CLK_INTLEN);
    sd_delayus(10);
    count = 10000;
    while ((*EMMC_CONTROL1 & resetType) && count--)
        sd_delayus(10);
    if (count <= 0) {
        cprintf("* EMMC: ERROR: failed to reset.\n");
        return SD_ERROR_RESET;
    }

    // Enable internal clock and set data timeout.
    // TODO: Correct value for timeout?
    *EMMC_CONTROL1 |= C1_CLK_INTLEN | C1_TOUNIT_MAX;
    sd_delayus(10);

    // Set clock to setup frequency.
    if ((resp = sdSetClock(FREQ_SETUP))) return resp;

    // Enable interrupts for command completion values.
    // *EMMC_IRPT_EN   = INT_ALL_MASK;
    // *EMMC_IRPT_MASK = INT_ALL_MASK;
    // Ignore INT_CMD_DONE and INT_WRITE_RDY.
    *EMMC_IRPT_EN = 0xffffffff & (~INT_CMD_DONE) & (~INT_WRITE_RDY);
    *EMMC_IRPT_MASK = 0xffffffff;
    // printf("EMMC: Interrupt enable/mask registers: %08x %08x\n",*EMMC_IRPT_EN,*EMMC_IRPT_MASK);
    // printf("EMMC: Status: %08x, control: %08x %08x %08x\n",*EMMC_STATUS,*EMMC_CONTROL0,*EMMC_CONTROL1,*EMMC_CONTROL2);

    // Reset card registers.
    sdCard.rca = 0;
    sdCard.ocr = 0;
    sdCard.lastArg = 0;
    sdCard.lastCmd = 0;
    sdCard.status = 0;
    sdCard.type = 0;
    sdCard.uhsi = 0;

    // Send GO_IDLE_STATE
    cprintf("- Send IX_GO_IDLE_STATE command\n");
    resp = sdSendCommand(IX_GO_IDLE_STATE);

    return resp;
}

/*
 * Common routine for APP_SEND_OP_COND.
 * This is used for both SC and HC cards based on the parameter.
 */
static int
sdAppSendOpCond(int arg)
{
    // Send APP_SEND_OP_COND with the given argument (for SC or HC cards).
    // Note: The host shall set ACMD41 timeout more than 1 second to abort repeat of issuing ACMD41
    // TODO: how to set ACMD41 timeout? Is that the wait?
    cprintf("- EMMC: Sending ACMD41 SEND_OP_COND status %x\n",*EMMC_STATUS);
    int resp, count;
    if ((resp = sdSendCommandA(IX_APP_SEND_OP_COND, arg)) && resp != SD_TIMEOUT) {
        cprintf("* EMMC: ACMD41 returned non-timeout error %d\n", resp);
        return resp;
    }
    count = 6;
    while (!(sdCard.ocr & R3_COMPLETE) && count--) {
        cprintf("- EMMC: Retrying ACMD SEND_OP_COND status %x\n",*EMMC_STATUS);
        // delay(400);
        delay(50000);
        if ((resp = sdSendCommandA(IX_APP_SEND_OP_COND,arg)) && resp != SD_TIMEOUT) {
            cprintf("* EMMC: ACMD41 returned non-timeout error %d\n", resp);
            return resp;
        }
    }

    // Return timeout error if still not busy.
    if (!(sdCard.ocr & R3_COMPLETE))
        return SD_TIMEOUT;

    // Check that at least one voltage value was returned.
    if (!(sdCard.ocr & ACMD41_VOLTAGE))
        return SD_ERROR_VOLTAGE;

    return SD_OK;
}

/* Switch voltage to 1.8v where the card supports it. */
static int
sdSwitchVoltage()
{
    cprintf("- EMMC: Pi does not support switch voltage, fixed at 3.3volt\n");
    return SD_OK;
}

/* Routine to initialize GPIO registers. */
static void
sdInitGPIO()
{
    uint32_t r;
    // GPIO_CD
    r = get32(GPFSEL4); r &= ~(7<<(7*3)); put32(GPFSEL4, r);
    put32(GPPUD, 2); delay(150);
    put32(GPPUDCLK1, 1<<15); delay(150);
    put32(GPPUD, 0); put32(GPPUDCLK1, 0);
    r = get32(GPHEN1); r|=1<<15; put32(GPHEN1, r);

    // GPIO_CLK, GPIO_CMD
    r = get32(GPFSEL4); r |= (7<<(8*3))|(7<<(9*3)); put32(GPFSEL4, r);
    put32(GPPUD, 2); delay(150);
    put32(GPPUDCLK1, (1<<16)|(1<<17)); delay(150);
    put32(GPPUD, 0); put32(GPPUDCLK1, 0);

    // GPIO_DAT0, GPIO_DAT1, GPIO_DAT2, GPIO_DAT3
    r = get32(GPFSEL5); r |= (7<<(0*3)) | (7<<(1*3)) | (7<<(2*3)) | (7<<(3*3)); put32(GPFSEL5, r);
    put32(GPPUD, 2); delay(150);
    put32(GPPUDCLK1, (1<<18) | (1<<19) | (1<<20) | (1<<21));
    delay(150); put32(GPPUD, 0); put32(GPPUDCLK1, 0);
}

/* Get the base clock speed. */
int
sdGetBaseClock()
{
    sdBaseClock = mbox_get_clock_rate(MBX_PROP_CLOCK_EMMC);
    if (sdBaseClock == -1) {
        cprintf("* EMMC: Error, failed to get base clock from mailbox\n");
        return SD_ERROR;
    }
    cprintf("- SD base clock rate from mailbox: %d.\n", sdBaseClock);
    return SD_OK;
}

/*
 * Initialize SD card.
 * Returns zero if initialization was successful, non-zero otherwise.
 */
int
sdInit()
{
    // Ensure we've initialized GPIO.
    if (!sdCard.init) sdInitGPIO();
    //  wait(50);

    // Check GPIO 47 status
    //  int cardAbsent = gpioGetPinLevel(GPIO_CD);
    //  int cardAbsent = get32(GPLEV1) & (1 << (47-32)); // TEST
    int cardAbsent = 0;
    //  int cardEjected = gpioGetEventDetected(GPIO_CD);
  
    int cardEjected = get32(GPEDS1) & (1 << (47-32));
    int oldCID[4];
    //  printf("In SD init card, status %08x interrupt %08x card absent %d ejected %d\n",*EMMC_STATUS,*EMMC_INTERRUPT,cardAbsent,cardEjected);

    // No card present, nothing can be done.
    // Only log the fact that the card is absent the first time we discover it.
    if (cardAbsent) {
        sdCard.init = 0;
        int wasAbsent = sdCard.absent;
        sdCard.absent = 1;
        if (!wasAbsent) cprintf("* EMMC: no SD card detected");
        return SD_CARD_ABSENT;
    }

    // If initialized before, check status of the card.
    // Card present, but removed since last call to init, then need to
    // go back through init, and indicate that in the return value.
    // In this case we would expect INT_CARD_INSERT to be set.
    // Clear the insert and remove interrupts
    sdCard.absent = 0;
    if (cardEjected && sdCard.init) {
        sdCard.init = 0;
        memmove(oldCID, sdCard.cid, sizeof(int)*4);
    }
    else if (!sdCard.init)
        memset(oldCID, 0, sizeof(int)*4);

    // If already initialized and card not replaced, nothing to do.
    if (sdCard.init) return SD_OK;

    // TODO: check version >= 1 and <= 3?
    sdHostVer = (*EMMC_SLOTISR_VER & HOST_SPEC_NUM) >> HOST_SPEC_NUM_SHIFT;

    // Get base clock speed.
    //  sdDebug = 0;
    int resp;
    if ((resp = sdGetBaseClock())) return resp;

    // Reset the card.
    cprintf("- sdInit: reset the card\n");
    if ((resp = sdResetCard(C1_SRST_HC))) return resp;

    disb();
    // Send SEND_IF_COND,0x000001AA (CMD8) voltage range 0x1 check pattern 0xAA
    // If voltage range and check pattern don't match, look for older card.
    resp = sdSendCommandA(IX_SEND_IF_COND, 0x000001AA);
    cprintf("- sdSendCommandA response: %d\n", resp);
    if (resp == SD_OK) {
        // Card responded with voltage and check pattern.
        // Resolve voltage and check for high capacity card.
        // FIXME:
        delay(50);
        if ((resp = sdAppSendOpCond(ACMD41_ARG_HC))) return sdDebugResponse(resp);

        // Check for high or standard capacity.
        if (sdCard.ocr & R3_CCS)
            sdCard.type = SD_TYPE_2_HC;
        else
            sdCard.type = SD_TYPE_2_SC;
    }

    else if (resp == SD_BUSY) return resp;

    else {
        cprintf("- no response to SEND_IF_COND, treat as an old card.\n");
        // If there appears to be a command in progress, reset the card.
        if ((*EMMC_STATUS & SR_CMD_INHIBIT) &&
            (resp = sdResetCard(C1_SRST_HC)))
            return resp;

        // delay(50);
        // Resolve voltage.
        if ((resp = sdAppSendOpCond(ACMD41_ARG_SC))) return sdDebugResponse(resp);

        sdCard.type = SD_TYPE_1;
    }

    // If the switch to 1.8A is accepted, then we need to send a CMD11.
    // CMD11: Completion of voltage switch sequence is checked by high level of DAT[3:0].
    // Any bit of DAT[3:0] can be checked depends on ability of the host.
    // Appears for PI its any/all bits.
    if ((sdCard.ocr & R3_S18A) && (resp = sdSwitchVoltage())) return resp;

    // Send ALL_SEND_CID (CMD2)
    if ((resp = sdSendCommand(IX_ALL_SEND_CID))) return sdDebugResponse(resp);

    // Send SEND_REL_ADDR (CMD3)
    // TODO: In theory, loop back to SEND_IF_COND to find additional cards.
    if ((resp = sdSendCommand(IX_SEND_REL_ADDR))) return sdDebugResponse(resp);

    // From now on the card should be in standby state.
    // Actually cards seem to respond in identify state at this point.
    // Check this with a SEND_STATUS (CMD13)
    //if( (resp = sdSendCommand(IX_SEND_STATUS))) return sdDebugResponse(resp);
    //  printf("Card current state: %08x %s\n",sdCard.status,STATUS_NAME[sdCard.cardState]);

    // Send SEND_CSD (CMD9) and parse the result.
    if ((resp = sdSendCommand(IX_SEND_CSD))) return sdDebugResponse(resp);
    sdParseCSD();
    if (sdCard.fileFormat != CSD3VN_FILE_FORMAT_DOSFAT &&
        sdCard.fileFormat != CSD3VN_FILE_FORMAT_HDD) {
        cprintf("* EMMC: Error, unrecognised file format %02x\n",sdCard.fileFormat);
        return SD_ERROR;
    }

    // At this point, set the clock to full speed.
    if ((resp = sdSetClock(FREQ_NORMAL))) return sdDebugResponse(resp);

    // Send CARD_SELECT  (CMD7)
    // TODO: Check card_is_locked status in the R1 response from CMD7 [bit 25], if so, use CMD42 to unlock
    // CMD42 structure [4.3.7] same as a single block write; data block includes
    // PWD setting mode, PWD len, PWD data.
    if ((resp = sdSendCommand(IX_CARD_SELECT))) return sdDebugResponse(resp);

    // Get the SCR as well.
    // Need to do this before sending ACMD6 so that allowed bus widths are known.
    if ((resp = sdReadSCR())) return sdDebugResponse(resp);

    // Send APP_SET_BUS_WIDTH (ACMD6)
    // If supported, set 4 bit bus width and update the CONTROL0 register.
    if (sdCard.support & SD_SUPP_BUS_WIDTH_4) {
        if ((resp = sdSendCommandA(IX_SET_BUS_WIDTH, sdCard.rca | 2)))
            return sdDebugResponse(resp);
        *EMMC_CONTROL0 |= C0_HCTL_DWITDH;
    }

    // Send SET_BLOCKLEN (CMD16)
    // TODO: only needs to be sent for SDSC cards.  For SDHC and SDXC cards block length is fixed
    // at 512 anyway.
    if ((resp = sdSendCommandA(IX_SET_BLOCKLEN, 512))) return sdDebugResponse(resp);

    // Print out the CID having got this far.
    sdParseCID();

    // Initialisation complete.
    sdCard.init = 1;

    // Return value indicates whether the card was reinserted or replaced.
    if (memcmp(oldCID, sdCard.cid, sizeof(int)*4) == 0)
        return SD_CARD_REINSERTED;

    return SD_CARD_CHANGED;
}

/* Parse CID. */
static void
sdParseCID()
{
    // For some reason cards I have looked at seem to have everything
    // shifted 8 bits down.
    int manId = (sdCard.cid[0]&0x00ff0000) >> 16;
    char appId[3];
    appId[0] = (sdCard.cid[0]&0x0000ff00) >> 8;
    appId[1] = (sdCard.cid[0]&0x000000ff);
    appId[2] = 0;
    char name[6];
    name[0] = (sdCard.cid[1]&0xff000000) >> 24;
    name[1] = (sdCard.cid[1]&0x00ff0000) >> 16;
    name[2] = (sdCard.cid[1]&0x0000ff00) >> 8;
    name[3] = (sdCard.cid[1]&0x000000ff);
    name[4] = (sdCard.cid[2]&0xff000000) >> 24;
    name[5] = 0;
    int revH = (sdCard.cid[2]&0x00f00000) >> 20;
    int revL = (sdCard.cid[2]&0x000f0000) >> 16;
    int serial = ((sdCard.cid[2]&0x0000ffff) << 16) +
                 ((sdCard.cid[3]&0xffff0000) >> 16);

    // For some reason cards I have looked at seem to have the Y/M in
    // bits 11:0 whereas the spec says they should be in bits 19:8
    int dateY = ((sdCard.cid[3]&0x00000ff0) >> 4) + 2000;
    int dateM = (sdCard.cid[3]&0x0000000f);

    cprintf("- EMMC: SD Card %s %dMb UHS-I %d mfr %d '%s:%s' r%d.%d %d/%d, #%x RCA %x\n",
            SD_TYPE_NAME[sdCard.type], (int)(sdCard.capacity>>20),sdCard.uhsi,
            manId, appId, name, revH, revL, dateM, dateY, serial, sdCard.rca>>16);
}

/* Parse CSD. */
static void
sdParseCSD()
{
    int csdVersion = sdCard.csd[0] & CSD0_VERSION;

    // For now just work out the size.
    if (csdVersion == CSD0_V1) {
        int csize = ((sdCard.csd[1] & CSD1V1_C_SIZEH) << CSD1V1_C_SIZEH_SHIFT) +
                    ((sdCard.csd[2] & CSD2V1_C_SIZEL) >> CSD2V1_C_SIZEL_SHIFT);
        int mult = 1 << (((sdCard.csd[2] & CSD2V1_C_SIZE_MULT) >> CSD2V1_C_SIZE_MULT_SHIFT) + 2);
        long long blockSize = 1 << ((sdCard.csd[1] & CSD1VN_READ_BL_LEN) >> CSD1VN_READ_BL_LEN_SHIFT);
        long long numBlocks = (csize+1LL)*mult;
		  
        sdCard.capacity = numBlocks * blockSize;
    } else {
        // if (csdVersion == CSD0_V2)
        long long csize = (sdCard.csd[2] & CSD2V2_C_SIZE) >> CSD2V2_C_SIZE_SHIFT;
        sdCard.capacity = (csize+1LL)*512LL*1024LL;
    }
  
    // Get other attributes of the card.
    sdCard.fileFormat = sdCard.csd[3] & CSD3VN_FILE_FORMAT;
}
