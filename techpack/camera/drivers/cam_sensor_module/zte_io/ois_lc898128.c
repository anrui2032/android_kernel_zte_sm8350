/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.

 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>

#include "cam_ois_dev.h"

#include "cam_sensor_io.h"
#include "cam_sensor_i2c.h"
#include "zte_camera_ois_util.h"
#include "ois_lc898128.h"


#define CONFIG_ZTE_OIS_UTIL_DEBUG

#undef CDBG
#ifdef CONFIG_ZTE_OIS_UTIL_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif


#define		MeasureFilterA_Coeff			0x8380
#define		MeasureFilterA_Coeff_b1			0x0000 + MeasureFilterA_Coeff
#define		MeasureFilterA_Coeff_c1			0x0004 + MeasureFilterA_Coeff
#define		MeasureFilterA_Coeff_a1			0x0008 + MeasureFilterA_Coeff

#define		MeasureFilterA_Coeff_b2			0x000C + MeasureFilterA_Coeff
#define		MeasureFilterA_Coeff_c2			0x0010 + MeasureFilterA_Coeff
#define		MeasureFilterA_Coeff_a2			0x0014 + MeasureFilterA_Coeff


#define		MeasureFilterB_Coeff			0x8398
#define		MeasureFilterB_Coeff_b1			0x0000 + MeasureFilterB_Coeff
#define		MeasureFilterB_Coeff_c1			0x0004 + MeasureFilterB_Coeff
#define		MeasureFilterB_Coeff_a1			0x0008 + MeasureFilterB_Coeff

#define		MeasureFilterB_Coeff_b2			0x000C + MeasureFilterB_Coeff
#define		MeasureFilterB_Coeff_c2			0x0010 + MeasureFilterB_Coeff
#define		MeasureFilterB_Coeff_a2			0x0014 + MeasureFilterB_Coeff

#define		StMeasureFunc					0x0278
#define		StMeasFunc_SiSampleNum			0x0000 + StMeasureFunc
#define		StMeasFunc_SiSampleMax			0x0004 + StMeasureFunc

#define		StMeasureFunc_MFA				0x0280
#define		StMeasFunc_MFA_SiMax1			0x0000 + StMeasureFunc_MFA
#define		StMeasFunc_MFA_SiMin1			0x0004 + StMeasureFunc_MFA
#define		StMeasFunc_MFA_PiMeasureRam1	0x0020 + StMeasureFunc_MFA
#define		StMeasFunc_MFA_LLiIntegral1		0x0010 + StMeasureFunc_MFA
#define		StMeasFunc_MFA_LLiAbsInteg1		0x0018 + StMeasureFunc_MFA



#define		StMeasureFunc_MFB				0x02A8
#define		StMeasFunc_MFB_SiMax2			0x0000 + StMeasureFunc_MFB
#define		StMeasFunc_MFB_SiMin2			0x0004 + StMeasureFunc_MFB
#define		StMeasFunc_MFB_PiMeasureRam2	0x0020 + StMeasureFunc_MFB
#define		StMeasFunc_MFB_LLiIntegral2		0x0010 + StMeasureFunc_MFB
#define		StMeasFunc_MFB_LLiAbsInteg2		0x0018 + StMeasureFunc_MFB


#define		WaitTimerData					0x0324
#define		WaitTimerData_UiWaitCounter		0x0000 + WaitTimerData
#define		WaitTimerData_UiTargetCount		0x0004 + WaitTimerData


#define		GYRO_RAM_COMMON					0x0220
#define		GYRO_RAM_GX_ADIDAT				0x0000 + GYRO_RAM_COMMON
#define		GYRO_RAM_GY_ADIDAT				0x0004 + GYRO_RAM_COMMON
#define		GYRO_RAM_GXOFFZ					0x0020 + GYRO_RAM_COMMON
#define		GYRO_RAM_GYOFFZ					0x0024 + GYRO_RAM_COMMON
#define		GYRO_ZRAM_GZOFFZ				0x03A0


#define		GYRO_ZRAM_GZ_ADIDAT			0x0394

#define		MeasureFilterA_Delay			0x02D0
#define		MeasureFilterA_Delay_z11		0x0000 + MeasureFilterA_Delay
#define		MeasureFilterA_Delay_z12		0x0004 + MeasureFilterA_Delay
#define		MeasureFilterA_Delay_z21		0x0008 + MeasureFilterA_Delay
#define		MeasureFilterA_Delay_z22		0x000C + MeasureFilterA_Delay

#define		MeasureFilterB_Delay			0x02E0
#define		MeasureFilterB_Delay_z11		0x0000 + MeasureFilterB_Delay
#define		MeasureFilterB_Delay_z12		0x0004 + MeasureFilterB_Delay
#define		MeasureFilterB_Delay_z21		0x0008 + MeasureFilterB_Delay
#define		MeasureFilterB_Delay_z22		0x000C + MeasureFilterB_Delay


#define		EXE_GXADJ	0x00000042L		//!< Adjust NG : X Gyro NG (offset)
#define		EXE_GYADJ	0x00000082L		//!< Adjust NG : Y Gyro NG (offset)
#define		EXE_GZADJ	0x00400002L		//!< Adjust NG : Z Gyro NG (offset)
#define		EXE_END		0x00000002L		//!< Execute End (Adjust OK)

#define		GYRO_RAM_X						0x01D8
#define		GYRO_RAM_GYROX_OFFSET			0x0000 + GYRO_RAM_X

#define		GYRO_RAM_Y						0x01FC
#define		GYRO_RAM_GYROY_OFFSET			0x0000 + GYRO_RAM_Y

#define		GyroRAM_Z_GYRO_OFFSET		0x0370

#define		GyroFilterDelayX_GXH1Z2			0x019C
#define		GyroFilterDelayY_GYH1Z2			0x01C4


#define		AcclFilDly_X					0x03B8
#define		AcclFilDly_Y					0x03E8
#define		AcclFilDly_Z					0x0418


#define 	ONE_MSEC_COUNT	18			// 18.0446kHz * 18 à 1ms
#define 	MESOF_NUM		2048			// 2048times
#define 	GYROFFSET_H		(0x06D6 << 16)


typedef struct {
	int32_t				SiSampleNum ;			//!< Measure Sample Number
	int32_t				SiSampleMax ;			//!< Measure Sample Number Max

	struct {
		int32_t			SiMax1 ;				//!< Max Measure Result
		int32_t			SiMin1 ;				//!< Min Measure Result
		uint32_t			UiAmp1 ;						//!< Amplitude Measure Result
		int64_t			LLiIntegral1 ;				//!< Integration Measure Result
		int64_t			LLiAbsInteg1 ;				//!< Absolute Integration Measure Result
		int32_t			PiMeasureRam1 ;			//!< Measure Delay RAM Address
	} MeasureFilterA ;

	struct {
		int32_t			SiMax2 ;				//!< Max Measure Result
		int32_t			SiMin2 ;				//!< Min Measure Result
		uint32_t			UiAmp2 ;						//!< Amplitude Measure Result
		int64_t			LLiIntegral2 ;				//!< Integration Measure Result
		int64_t			LLiAbsInteg2 ;				//!< Absolute Integration Measure Result
		int32_t			PiMeasureRam2 ;			//!< Measure Delay RAM Address
	} MeasureFilterB ;
} MeasureFunction_Type ;

union	DWDVAL {
	uint32_t	UlDwdVal ;
	uint16_t	UsDwdVal[2] ;
	struct {
		uint16_t	UsHigVal ;
		uint16_t	UsLowVal ;
	} StDwdVal ;
	struct {
		uint8_t	UcRamVa3 ;
		uint8_t	UcRamVa2 ;
		uint8_t	UcRamVa1 ;
		uint8_t	UcRamVa0 ;
	} StCdwVal ;
};

typedef union DWDVAL UnDwdVal;

union	ULLNVAL {
	u64	UllnValue ;
	uint32_t	UlnValue[ 2 ] ;
	struct {
		uint32_t	UlLowVal ;
		uint32_t	UlHigVal ;
	} StUllnVal ;
} ;

typedef union ULLNVAL	UnllnVal;


int32_t RamWrite32A(void *data, int addr, int value)
{
	msm_ois_debug_info_t *ptr = (msm_ois_debug_info_t *)data;
	int32_t rc = 0;

	rc = zte_cam_cci_i2c_write(&(ptr->s_ctrl->io_master_info),
			addr, value,
			CAMERA_SENSOR_I2C_TYPE_WORD,
			CAMERA_SENSOR_I2C_TYPE_DWORD);
	if (rc < 0) {
		pr_err("%s:%d: i2c write 0x%x=0x%x failed", __func__, __LINE__, addr, value);
		return rc;
	}

	return rc;
}

int32_t RamRead32A(void *data, int addr, uint32_t *value)
{
	msm_ois_debug_info_t *ptr = (msm_ois_debug_info_t *)data;
	int32_t rc = 0;

	rc = camera_io_dev_read(&(ptr->s_ctrl->io_master_info),
		addr, value, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_DWORD);
	if (rc < 0) {
		pr_err("%s:%d: i2c read addr[%d] = 0x%x failed",
			__func__, __LINE__, addr, *value);
	}

	return rc;
}

int32_t MesFil(void *data) // 18.0446kHz/15.027322kHz
{
	int32_t rc = 0;
	uint32_t UlMeasFilaA = 0x7FFFFFFF;
	uint32_t UlMeasFilaB = 0x00000000 ;
	uint32_t UlMeasFilaC = 0x00000000 ;
	uint32_t UlMeasFilbA = 0x7FFFFFFF ;
	uint32_t UlMeasFilbB = 0x00000000 ;
	uint32_t UlMeasFilbC = 0x00000000 ;

	if(RamWrite32A(data, MeasureFilterA_Coeff_a1, UlMeasFilaA))return -1;
	if(RamWrite32A(data, MeasureFilterA_Coeff_b1, UlMeasFilaB))return -1;
	if(RamWrite32A(data, MeasureFilterA_Coeff_c1, UlMeasFilaC))return -1;

	if(RamWrite32A(data, MeasureFilterA_Coeff_a2, UlMeasFilbA))return -1;
	if(RamWrite32A(data, MeasureFilterA_Coeff_b2, UlMeasFilbB))return -1;
	if(RamWrite32A(data, MeasureFilterA_Coeff_c2, UlMeasFilbC))return -1;

	if(RamWrite32A(data, MeasureFilterB_Coeff_a1, UlMeasFilaA))return -1;
	if(RamWrite32A(data, MeasureFilterB_Coeff_b1, UlMeasFilaB))return -1;
	if(RamWrite32A(data, MeasureFilterB_Coeff_c1, UlMeasFilaC))return -1;

	if(RamWrite32A(data, MeasureFilterB_Coeff_a2, UlMeasFilbA))return -1;
	if(RamWrite32A(data, MeasureFilterB_Coeff_b2, UlMeasFilbB))return -1;
	if(RamWrite32A(data, MeasureFilterB_Coeff_c2, UlMeasFilbC))return -1;

	return rc;
}

void MeasAddressSelection(uint8_t mode, int32_t *measadr_a, int32_t *measadr_b)
{
	if (mode == 0)
	{
		*measadr_a = GYRO_RAM_GX_ADIDAT; // Set Measure RAM Address
		*measadr_b = GYRO_RAM_GY_ADIDAT; // Set Measure RAM Address
	} else if(mode == 1)
	{
		*measadr_a = GYRO_ZRAM_GZ_ADIDAT; // Set Measure RAM Address
		*measadr_b = GYRO_ZRAM_GZ_ADIDAT; // Set Measure RAM Address
	}
}


int32_t MemoryClear(void *data, uint16_t UsSourceAddress, uint16_t UsClearSize)
{
	int32_t rc = 0;
	uint16_t	UsLoopIndex ;

	for ( UsLoopIndex = 0 ; UsLoopIndex < UsClearSize;)
	{
		if(RamWrite32A(data, UsSourceAddress, 0x00000000))return -1;
		UsSourceAddress += 4;
		UsLoopIndex += 4 ;
	}

	return rc;
}

int32_t SetTransDataAdr(void *data, uint16_t UsLowAddress, uint32_t UlLowAdrBeforeTrans)
{
	int32_t rc = 0;
	UnDwdVal	StTrsVal ;

	if( UlLowAdrBeforeTrans > 0x00009000 ){
		StTrsVal.StDwdVal.UsHigVal = (uint16_t)((UlLowAdrBeforeTrans & 0x0000F000 ) >> 8);
		StTrsVal.StDwdVal.UsLowVal = (uint16_t)(UlLowAdrBeforeTrans & 0x00000FFF);
	}else{
		StTrsVal.UlDwdVal = UlLowAdrBeforeTrans;
	}

	if(RamWrite32A(data, UsLowAddress, StTrsVal.UlDwdVal))return -1;

	return rc;
}

int32_t ClrMesFil(void *data)
{
	int32_t rc = 0;

	if(RamWrite32A(data, MeasureFilterA_Delay_z11, 0))return -1;
	if(RamWrite32A(data, MeasureFilterA_Delay_z12, 0))return -1;

	if(RamWrite32A(data, MeasureFilterA_Delay_z21, 0))return -1;
	if(RamWrite32A(data, MeasureFilterA_Delay_z22, 0))return -1;

	if(RamWrite32A(data, MeasureFilterB_Delay_z11, 0))return -1;
	if(RamWrite32A(data, MeasureFilterB_Delay_z12, 0))return -1;

	if(RamWrite32A(data, MeasureFilterB_Delay_z21, 0))return -1;
	if(RamWrite32A(data, MeasureFilterB_Delay_z22, 0))return -1;

	return rc;
}

int32_t SetWaitTime(void *data, uint16_t UsWaitTime)
{
	int32_t rc = 0;

	if(RamWrite32A(data, WaitTimerData_UiWaitCounter, 0))return -1;
	if(RamWrite32A(data, WaitTimerData_UiTargetCount, (uint32_t)(ONE_MSEC_COUNT * UsWaitTime)))return -1;

	return rc;
}


int32_t MeasureStart(void *data, int32_t SlMeasureParameterNum , int32_t SlMeasureParameterA , int32_t SlMeasureParameterB )
{
	int32_t rc = 0;

	if(MemoryClear(data, StMeasFunc_SiSampleNum, sizeof(MeasureFunction_Type)))return -1;
	if(RamWrite32A(data, StMeasFunc_MFA_SiMax1, 0x80000000))return -1;
	if(RamWrite32A(data, StMeasFunc_MFB_SiMax2, 0x80000000))return -1;
	if(RamWrite32A(data, StMeasFunc_MFA_SiMin1, 0x7FFFFFFF))return -1;
	if(RamWrite32A(data, StMeasFunc_MFB_SiMin2, 0x7FFFFFFF))return -1;

	if(SetTransDataAdr(data, StMeasFunc_MFA_PiMeasureRam1, (uint32_t)SlMeasureParameterA))return -1;
	if(SetTransDataAdr(data, StMeasFunc_MFB_PiMeasureRam2, (uint32_t)SlMeasureParameterB))return -1;

	if(RamWrite32A(data, StMeasFunc_SiSampleNum, 0))return -1;

	if(ClrMesFil(data))return -1;

	SetWaitTime(data, 1);

	if(RamWrite32A(data, StMeasFunc_SiSampleMax, SlMeasureParameterNum))return -1;

	return rc;
}

int32_t MeasureWait(void *data)
{
	uint32_t	SlWaitTimerSt ;
	uint16_t	UsTimeOut = 2000;

	usleep_range(1000 * 10, 1010 * 10);

	do {
		if(RamRead32A(data, StMeasFunc_SiSampleMax, &SlWaitTimerSt))return -1;
		usleep_range(1000, 1010);
	} while (SlWaitTimerSt && --UsTimeOut);

	if (SlWaitTimerSt == 0)
	{
		return 0;
	} else {
		pr_err("%s:%d: fail SlWaitTimerSt=%d=UsTimeOut = %d", __func__, __LINE__, SlWaitTimerSt, UsTimeOut);
		return -1;
	}
}

int32_t GetGyroOffset(void *data)
{
	int32_t rc = 0;
	msm_ois_debug_info_t *ptr = (msm_ois_debug_info_t *)data;
	uint16_t GyroOffsetX = 0, GyroOffsetY = 0, GyroOffsetZ = 0;
	uint32_t	ReadValX = 0, ReadValY = 0, ReadValZ = 0;

	if(RamRead32A(data, GYRO_RAM_GXOFFZ, &ReadValX)) return rc;
	if(RamRead32A(data, GYRO_RAM_GYOFFZ, &ReadValY)) return rc;
	if(RamRead32A(data, GYRO_ZRAM_GZOFFZ, &ReadValZ)) return rc;
	GyroOffsetX = (uint16_t)((ReadValX >> 16) & 0x0000FFFF);
	GyroOffsetY = (uint16_t)((ReadValY >> 16) & 0x0000FFFF);
	GyroOffsetZ = (uint16_t)((ReadValZ >> 16) & 0x0000FFFF);

	ptr->cal_info.cal_success = 1;
	ptr->cal_info.reg1_cal_val = GyroOffsetX;
	ptr->cal_info.reg2_cal_val = GyroOffsetY;
	ptr->cal_info.reg3_cal_val = GyroOffsetZ;

	pr_info("%s:%d: GyroOffsetX=0x%x  GyroOffsetY=0x%x GyroOffsetZ=0x%x", __func__, __LINE__, GyroOffsetX, GyroOffsetY, GyroOffsetZ);

	return rc;
}

int ois_lc898128_debugfs_cal_s(void *data)
{
	msm_ois_debug_info_t *ptr = (msm_ois_debug_info_t *)data;
	uint32_t UlRsltSts = EXE_END;
	int32_t SlMeasureParameterA, SlMeasureParameterB;
	int32_t SlMeasureParameterNum = MESOF_NUM;
	UnllnVal StMeasValueA, StMeasValueB;
	int32_t SlMeasureAveValueA[3], SlMeasureAveValueB[3];
	uint8_t i;

	memset(&(ptr->cal_info), 0, sizeof(msm_cal_info_t));

	if(MesFil(data))return -1;

	for(i = 0 ; i < 2 ; i++ )
	{
		MeasAddressSelection(i, &SlMeasureParameterA, &SlMeasureParameterB);
		if(MeasureStart(data, SlMeasureParameterNum, SlMeasureParameterA, SlMeasureParameterB))return -1;
		if(MeasureWait(data)) return -1;

		pr_info("%s:%d:Read Adr = %04x, %04xh", __func__, __LINE__, StMeasFunc_MFA_LLiIntegral1 + 4, StMeasFunc_MFA_LLiIntegral1);
		if(RamRead32A(data, StMeasFunc_MFA_LLiIntegral1, &(StMeasValueA.StUllnVal.UlLowVal)))return -1;
		if(RamRead32A(data, StMeasFunc_MFA_LLiIntegral1 + 4, &(StMeasValueA.StUllnVal.UlHigVal)))return -1;
		if(RamRead32A(data, StMeasFunc_MFB_LLiIntegral2, &(StMeasValueB.StUllnVal.UlLowVal)))return -1;
		if(RamRead32A(data, StMeasFunc_MFB_LLiIntegral2 + 4, &(StMeasValueB.StUllnVal.UlHigVal)))return -1;

		pr_info("%s:%d:(%d) AOFT = %08x, %08xh \n", __func__, __LINE__, i,(unsigned int)StMeasValueA.StUllnVal.UlHigVal,(unsigned int)StMeasValueA.StUllnVal.UlLowVal);
		pr_info("%s:%d:(%d) BOFT = %08x, %08xh \n", __func__, __LINE__, i,(unsigned int)StMeasValueB.StUllnVal.UlHigVal,(unsigned int)StMeasValueB.StUllnVal.UlLowVal);
		SlMeasureAveValueA[i] = (int32_t)( (int64_t)StMeasValueA.UllnValue / SlMeasureParameterNum);
		SlMeasureAveValueB[i] = (int32_t)( (int64_t)StMeasValueB.UllnValue / SlMeasureParameterNum);
		pr_info("%s:%d:AVEOFT = %08xh \n", __func__, __LINE__, (unsigned int)SlMeasureAveValueA[i]);
		pr_info("%s:%d:AVEOFT = %08xh \n", __func__, __LINE__, (unsigned int)SlMeasureAveValueB[i]);
	}


	if(abs(SlMeasureAveValueA[0]) > GYROFFSET_H)
	{
		UlRsltSts |= EXE_GXADJ ;
	}

	if(abs(SlMeasureAveValueB[0]) > GYROFFSET_H)
	{
		UlRsltSts |= EXE_GYADJ ;
	}
	if(abs(SlMeasureAveValueA[1]) > GYROFFSET_H)
	{
		UlRsltSts |= EXE_GZADJ ;
	}

	if(UlRsltSts == EXE_END)
	{
		if(RamWrite32A(data, GYRO_RAM_GXOFFZ, SlMeasureAveValueA[0]))return -1;
		if(RamWrite32A(data, GYRO_RAM_GYOFFZ, SlMeasureAveValueB[0]))return -1;
		if(RamWrite32A(data, GYRO_ZRAM_GZOFFZ, SlMeasureAveValueA[1]))return -1;

		if(RamWrite32A(data, GYRO_RAM_GYROX_OFFSET, 0x00000000))return -1;
		if(RamWrite32A(data, GYRO_RAM_GYROY_OFFSET, 0x00000000))return -1;
		if(RamWrite32A(data, GyroRAM_Z_GYRO_OFFSET, 0x00000000))return -1;
		if(RamWrite32A(data, GyroFilterDelayX_GXH1Z2, 0x00000000))return -1;
		if(RamWrite32A(data, GyroFilterDelayY_GYH1Z2, 0x00000000))return -1;
		if(RamWrite32A(data, AcclFilDly_X + 8, 0x00000000))return -1;
		if(RamWrite32A(data, AcclFilDly_Y + 8, 0x00000000))return -1;
		if(RamWrite32A(data, AcclFilDly_Z + 8, 0x00000000))return -1;
		if(RamWrite32A(data, AcclFilDly_X + 12, 0x00000000))return -1;
		if(RamWrite32A(data, AcclFilDly_Y + 12, 0x00000000))return -1;
		if(RamWrite32A(data, AcclFilDly_Z + 12, 0x00000000))return -1;
		if(RamWrite32A(data, AcclFilDly_X + 16, 0x00000000))return -1;
		if(RamWrite32A(data, AcclFilDly_Y + 16, 0x00000000))return -1;
		if(RamWrite32A(data, AcclFilDly_Z + 16, 0x00000000))return -1;
		if(RamWrite32A(data, AcclFilDly_X + 20, 0x00000000))return -1;
		if(RamWrite32A(data, AcclFilDly_Y + 20, 0x00000000))return -1;
		if(RamWrite32A(data, AcclFilDly_Z + 20, 0x00000000))return -1;

		if(GetGyroOffset(data))return -1;
	}

	return 0;
}

int ois_lc898128_debugfs_cal_g(void *data, u64 *val)
{
	msm_ois_debug_info_t *ptr = (msm_ois_debug_info_t *) data;

	ois_lc898128_debugfs_cal_s(data);

	if (ptr->cal_info.cal_success == 1) {
		*val = ptr->cal_info.reg1_cal_val;
		*val = (*val << 16);
		*val |= ptr->cal_info.reg2_cal_val;
		*val = (*val << 16);
		*val |= ptr->cal_info.reg3_cal_val;
		*val |= 0x100000000;
	} else {
		*val = 0;
	}
	pr_info("%s:%d: cal_success = %d  0x%llx",
		__func__, __LINE__, ptr->cal_info.cal_success, *val);
	return 0;
}

