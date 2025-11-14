/**
 * @file app_flash.h
 * @brief W25Q128 Flash 存储器驱动（W25Qxx 系列）
 *
 * 功能：W25Q128 Flash 芯片的低层驱动与工具函数。
 * 芯片参数（示例）：16M 字节，24 位地址线；256 个 Block（每个 64KB），
 * 每个 Block 含 16 个 Sector（每个 4KB），每个 Sector 含 16 个 Page（每个 256 字节）。
 * 写操作的基本单元为 Page（256 字节），连续写入不能跨越 Page 边界，写入前需擦除。
 *
 * @author 王维波
 * @date 2019-06-05
 */

#ifndef _W25FLASH_H
#define _W25FLASH_H

#include "stm32g4xx_hal.h"
#include <string.h>
#include "spi.h"     /**< 使用 spi.h 中的 SPI 句柄（例如 hspi2） */


#define CS_PORT     GPIOB
#define CS_PIN      GPIO_PIN_12
#define SPI_HANDLE      hspi2

#define __Select_Flash()        HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_RESET)
#define __Deselect_Flash()      HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET)

#define W25_PAGE_SIZE         256     /* 一个 Page 的字节数 */
#define FLASH_SECTOR_SIZE     4096    /* 一个 Sector 的字节数 */
#define FLASH_BLOCK_SIZE      65536   /* 一个 Block 的字节数 */
#define FLASH_BLOCK_COUNT     128     /* Block 的总数 */
#define FLASH_SECTOR_COUNT    2048    /* Sector 的总数 */

//=======1. SPI 基本发送和接收函数，阻塞式传输============
/**
 * @brief 通过 SPI 发送一个字节（阻塞）
 * @param byteData 要发送的字节
 * @return HAL 状态
 */
HAL_StatusTypeDef SPI_TransmitOneByte(uint8_t byteData);

/**
 * @brief 通过 SPI 发送多个字节（阻塞）
 * @param pBuffer 指向要发送数据的缓冲区
 * @param byteCount 要发送的字节数
 * @return HAL 状态
 */
HAL_StatusTypeDef SPI_TransmitBytes(uint8_t *pBuffer, uint16_t byteCount);

/**
 * @brief 通过 SPI 接收一个字节（阻塞）
 * @return 接收到的字节
 */
uint8_t SPI_ReceiveOneByte(void);

/**
 * @brief 通过 SPI 接收多个字节（阻塞）
 * @param pBuffer 接收缓冲区
 * @param byteCount 要接收的字节数
 * @return HAL 状态
 */
HAL_StatusTypeDef SPI_ReceiveBytes(uint8_t *pBuffer, uint16_t byteCount);

//=========2. W25Qxx 基本控制指令==========
/**
 * @brief 读取芯片 ID（Manufacturer/Device ID）
 * @note Command = 0x90
 * @return 16 位 ID 值
 */
uint16_t Flash_ReadID(void);

/**
 * @brief 读取唯一序列号（Unique ID, 64-bit）
 * @note Command = 0x4B
 * @param High32 指向存放高 32 位的变量指针（可为 NULL）
 * @param Low32 指向存放低 32 位的变量指针（可为 NULL）
 * @return 64 位序列号（如果 High32/Low32 非 NULL，也会写回对应值）
 */
uint64_t Flash_ReadSerialNum(uint32_t *High32, uint32_t *Low32);

/**
 * @brief 写入易失性使能（只影响 volatile 状态寄存器相关写使能）
 * @note Command = 0x50
 * @return HAL 状态
 */
HAL_StatusTypeDef Flash_WriteVolatile_Enable(void);

/**
 * @brief 使能写操作（设置 WEL = 1）
 * @note Command = 0x06
 * @return HAL 状态
 */
HAL_StatusTypeDef Flash_Write_Enable(void);

/**
 * @brief 禁用写操作（清除 WEL）
 * @note Command = 0x04
 * @return HAL 状态
 */
HAL_StatusTypeDef Flash_Write_Disable(void);

/**
 * @brief 读取状态寄存器 SR1
 * @note Command = 0x05
 * @return SR1 的值
 */
uint8_t Flash_ReadSR1(void);

/**
 * @brief 读取状态寄存器 SR2
 * @note Command = 0x35
 * @return SR2 的值
 */
uint8_t Flash_ReadSR2(void);

/**
 * @brief 写入状态寄存器 SR1（仅写 SR1）
 * @note Command = 0x01
 * @param SR1 要写入的寄存器值
 */
void Flash_WriteSR1(uint8_t SR1);

/**
 * @brief 等待器件不忙（BUSY = 0），通过轮询 SR1 的 BUSY 位
 * @return 等待时长（相对单位，具体实现可能返回超时或循环计数）
 */
uint32_t Flash_Wait_Busy(void);

/**
 * @brief 进入掉电模式
 * @note Command = 0xB9
 */
void Flash_PowerDown(void);

/**
 * @brief 唤醒从掉电模式
 * @note Command = 0xAB
 */
void Flash_WakeUp(void);

//========3. 计算地址的辅助功能函数========
/**
 * @brief 根据 Block（绝对编号）计算全局地址
 * @param BlockNo Block 的编号（0..255）
 * @return 全局地址（32 位）
 */
uint32_t Flash_Addr_byBlock(uint8_t BlockNo);

/**
 * @brief 根据 Sector（绝对编号）计算全局地址
 * @param SectorNo Sector 的编号（0..4095）
 * @return 全局地址（32 位）
 */
uint32_t Flash_Addr_bySector(uint16_t SectorNo);

/**
 * @brief 根据 Page（绝对编号）计算全局地址
 * @param PageNo Page 的编号（0..65535）
 * @return 全局地址（32 位）
 */
uint32_t Flash_Addr_byPage(uint16_t PageNo);

/**
 * @brief 根据 Block 编号和 Block 内的 Sector 编号计算全局地址
 * @param BlockNo Block 编号
 * @param SubSectorNo Block 内的 Sector 编号（0..15）
 * @return 全局地址
 */
uint32_t Flash_Addr_byBlockSector(uint8_t BlockNo, uint8_t SubSectorNo);

/**
 * @brief 根据 Block、子 Sector 和子 Page 编号计算全局地址
 * @param BlockNo Block 编号
 * @param SubSectorNo Sector 内编号
 * @param SubPageNo Page 内编号
 * @return 全局地址
 */
uint32_t Flash_Addr_byBlockSectorPage(uint8_t BlockNo, uint8_t SubSectorNo, uint8_t SubPageNo);

/**
 * @brief 将 24 位全局地址分解为三个字节（高/中/低）
 * @param globalAddr 全局地址
 * @param addrHigh 输出：高 8 位
 * @param addrMid 输出：中 8 位
 * @param addrLow 输出：低 8 位
 */
void Flash_SpliteAddr(uint32_t globalAddr, uint8_t *addrHigh, uint8_t *addrMid, uint8_t *addrLow);

/**
 * @brief 擦除整片芯片（Chip Erase）
 * @note Command = 0xC7，耗时约 25 秒
 */
void Flash_EraseChip(void);

/**
 * @brief 擦除 64KB Block（Block Erase）
 * @note Command = 0xD8，globalAddr 为所在 Block 的任意地址，耗时约 150 ms
 * @param globalAddr 全局地址（位于欲擦除的 Block 内）
 */
void Flash_EraseBlock64K(uint32_t globalAddr);

/**
 * @brief 擦除 4KB Sector（Sector Erase）
 * @note Command = 0x20，globalAddr 为所在 Sector 的任意地址，耗时约 30 ms
 * @param globalAddr 全局地址（位于欲擦除的 Sector 内）
 */
void Flash_EraseSector(uint32_t globalAddr);

/**
 * @brief 读取一个字节
 * @note Command = 0x03
 * @param globalAddr 读取的全局地址
 * @return 读取到的字节
 */
uint8_t Flash_ReadOneByte(uint32_t globalAddr);

/**
 * @brief 从全局地址开始连续读取多个字节
 * @note Command = 0x03
 * @param globalAddr 起始全局地址
 * @param pBuffer 接收数据的缓冲区
 * @param byteCount 要读取的字节数
 */
void Flash_ReadBytes(uint32_t globalAddr, uint8_t *pBuffer, uint16_t byteCount);

/**
 * @brief 高速连续读取多个字节（Fast Read）
 * @note Command = 0x0B，速度大约为普通读取的 2 倍
 * @param globalAddr 起始全局地址
 * @param pBuffer 接收缓冲区
 * @param byteCount 要读取的字节数
 */
void Flash_FastReadBytes(uint32_t globalAddr, uint8_t *pBuffer, uint16_t byteCount);

/**
 * @brief 对一个 Page 编程（Page Program），一次最多写入 256 字节
 * @note Command = 0x02，写操作通常耗时约 3 ms
 * @param globalAddr 起始全局地址（Page 内）
 * @param pBuffer 待写入数据缓冲区
 * @param byteCount 要写入的字节数（<= W25_PAGE_SIZE）
 */
void Flash_WriteInPage(uint32_t globalAddr, uint8_t *pBuffer, uint16_t byteCount);

/**
 * @brief 从某个 Sector 的起始地址开始写数据，写入数据可能跨越多个 Page 或 Sector
 * @note 总写入字节数不能超过一个 Block（64KB）
 * @param globalAddr 起始全局地址（通常为 Sector 起始地址）
 * @param pBuffer 待写入数据缓冲区
 * @param byteCount 要写入的字节数（<= 64KB）
 */
void Flash_WriteSector(uint32_t globalAddr, uint8_t *pBuffer, uint16_t byteCount);

#endif /* _W25FLASH_H */
