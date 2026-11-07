/*
 * mb.c
 *
 *  Created on: May 29, 2025
 *      Author: talon
 */

#include "mb.h"
#include "mbfunc.h"
#include <stddef.h>
#include "mbutils.h"
#include "board.h"

xMBFunctionHandler MBFunc[MB_FUNC_HANDLERS_MAX] =
		{
#if MB_FUNC_OTHER_REP_SLAVEID_ENABLED > 0
				{ MB_FUNC_OTHER_REPORT_SLAVEID, eMBFuncReportSlaveID },
#endif
#if MB_FUNC_READ_INPUT_ENABLED > 0
				{ MB_FUNC_READ_INPUT_REGISTER, eMBFuncReadInputRegister },
#endif
#if MB_FUNC_READ_HOLDING_ENABLED > 0
				{ MB_FUNC_READ_HOLDING_REGISTER, eMBFuncReadHoldingRegister },
#endif
#if MB_FUNC_WRITE_MULTIPLE_HOLDING_ENABLED > 0
				{ MB_FUNC_WRITE_MULTIPLE_REGISTERS,
						eMBFuncWriteMultipleHoldingRegister },
#endif
#if MB_FUNC_WRITE_HOLDING_ENABLED > 0
				{ MB_FUNC_WRITE_REGISTER, eMBFuncWriteHoldingRegister },
#endif
#if MB_FUNC_READWRITE_HOLDING_ENABLED > 0
				{ MB_FUNC_READWRITE_MULTIPLE_REGISTERS,
						eMBFuncReadWriteMultipleHoldingRegister },
#endif
#if MB_FUNC_READ_COILS_ENABLED > 0
				{ MB_FUNC_READ_COILS, eMBFuncReadCoils },
#endif
#if MB_FUNC_WRITE_COIL_ENABLED > 0
				{ MB_FUNC_WRITE_SINGLE_COIL, eMBFuncWriteCoil },
#endif
#if MB_FUNC_WRITE_MULTIPLE_COILS_ENABLED > 0
				{ MB_FUNC_WRITE_MULTIPLE_COILS, eMBFuncWriteMultipleCoils },
#endif
#if MB_FUNC_READ_DISCRETE_INPUTS_ENABLED > 0
				{ MB_FUNC_READ_DISCRETE_INPUTS, eMBFuncReadDiscreteInputs },
#endif
		};

/* ----------------------- Static variables ---------------------------------*/
/*------------------------Slave mode use these variables----------------------*/

//Slave mode:DiscreteInputs variables
#if S_DISCRETE_INPUT_NDISCRETES > 0
USHORT   usSDiscInStart                               = S_DISCRETE_INPUT_START;
#if S_DISCRETE_INPUT_NDISCRETES%8
UCHAR    ucSDiscInBuf[S_DISCRETE_INPUT_NDISCRETES/8+1];
#else
UCHAR    ucSDiscInBuf[S_DISCRETE_INPUT_NDISCRETES/8]  ;
#endif

UCHAR *input_coils = ucSDiscInBuf;

#endif



//Slave mode:Coils variables
#if S_COIL_NCOILS > 0
USHORT   usSCoilStart                                 = S_COIL_START;
#if S_COIL_NCOILS%8
UCHAR    ucSCoilBuf[S_COIL_NCOILS/8+1]                ;
#else
UCHAR    ucSCoilBuf[S_COIL_NCOILS/8]                  ;
#endif

UCHAR *output_coils = ucSCoilBuf;

#endif

//Slave mode:InputRegister variables
#if S_REG_INPUT_NREGS > 0
USHORT   usSRegInStart                                = S_REG_INPUT_START;
USHORT   usSRegInBuf[S_REG_INPUT_NREGS]               ;

USHORT *input_reg = usSRegInBuf;

#endif

//Slave mode:HoldingRegister variables
#if S_REG_HOLDING_NREGS > 0
USHORT   usSRegHoldStart                              = S_REG_HOLDING_START;
USHORT   usSRegHoldBuf[S_REG_HOLDING_NREGS]           ;

USHORT *hoding_reg = usSRegHoldBuf;
#endif



static mb_tcp_recv tcp_recv_fn = NULL;
static mb_tcp_send tcp_send_fn = NULL;

void eMBTCPInit(mb_tcp_recv recv_fn, mb_tcp_send send_fn) {
	tcp_recv_fn = recv_fn;
	tcp_send_fn = send_fn;
}

int eMBTCPPoll(SOCKET sock, UCHAR *buff, ULONG length) {
	ModbusTCPFrame_t frame;
	int ret;
	USHORT usLength;
	UCHAR *aucTCPBuf = buff;
	if (tcp_recv_fn == NULL)
		return -1;

	ret = tcp_recv_fn(sock, aucTCPBuf, length);

	if (ret < 0) {
		return -1;
	}
	frame.TDI[0] = aucTCPBuf[0];
	frame.TDI[1] = aucTCPBuf[1];
	frame.PID[0] = aucTCPBuf[2];
	frame.PID[1] = aucTCPBuf[3];
	frame.LEN[0] = aucTCPBuf[4];
	frame.LEN[1] = aucTCPBuf[5];
	usLength = frame.LEN[0] << 8U | frame.LEN[1];
	if (ret == MB_TCP_UID + usLength) {
		frame.UID = aucTCPBuf[6];
		frame.FUNC = aucTCPBuf[7];
		frame.DATA = &aucTCPBuf[8];

		eMBException eException;

		eException = MB_EX_ILLEGAL_FUNCTION;

		for (int i = 0; i < MB_FUNC_HANDLERS_MAX; i++) {
			/* No more function handlers registered. Abort. */
			if (MBFunc[i].ucFunctionCode == 0) {
				return -1;
			} else if (MBFunc[i].ucFunctionCode == frame.FUNC) {
				usLength = usLength - 1;
				eException = MBFunc[i].pxHandler(&aucTCPBuf[7],
						&usLength);
				return -1;
			}
		}

		if (eException != MB_EX_NONE) {
			usLength = 0;
			frame.DATA[usLength++] = (UCHAR) (frame.FUNC | MB_FUNC_ERROR);
			frame.DATA[usLength++] = eException;
		}
		if(tcp_send_fn == NULL) return -1;
		ret = tcp_send_fn(sock, aucTCPBuf, usLength + 7);
		if (ret < 0) {
			return -1;
		}
	}
	return 0;
}

VIRTUAL eMBErrorCode eMBRegInputCB(UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs )
{
#if S_REG_INPUT_NREGS > 0
    eMBErrorCode    eStatus = MB_ENOERR;
    USHORT          iRegIndex;
    USHORT *        pusRegInputBuf;
    USHORT          REG_INPUT_START;
    USHORT          REG_INPUT_NREGS;
    USHORT          usRegInStart;

    pusRegInputBuf = usSRegInBuf;
    REG_INPUT_START = S_REG_INPUT_START;
    REG_INPUT_NREGS = S_REG_INPUT_NREGS;
    usRegInStart = usSRegInStart;

    /* it already plus one in modbus function method. */
    usAddress--;

    if ((usAddress >= REG_INPUT_START) && (usAddress + usNRegs <= REG_INPUT_START + REG_INPUT_NREGS))
    {
        iRegIndex = usAddress - usRegInStart;
        while (usNRegs > 0)
        {
            *pucRegBuffer++ = (UCHAR) (pusRegInputBuf[iRegIndex] >> 8);
            *pucRegBuffer++ = (UCHAR) (pusRegInputBuf[iRegIndex] & 0xFF);
            iRegIndex++;
            usNRegs--;
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }

    return eStatus;
#else
	return MB_ENOREG;
#endif
}

VIRTUAL eMBErrorCode eMBRegHoldingCB(UCHAR *pucRegBuffer, USHORT usAddress,
		USHORT usNRegs, eMBRegisterMode eMode) {
#if S_REG_HOLDING_NREGS > 0
    eMBErrorCode    eStatus = MB_ENOERR;
    USHORT          iRegIndex;
    USHORT *        pusRegHoldingBuf;
    USHORT          REG_HOLDING_START;
    USHORT          REG_HOLDING_NREGS;
    USHORT          usRegHoldStart;

    pusRegHoldingBuf = usSRegHoldBuf;
    REG_HOLDING_START = S_REG_HOLDING_START;
    REG_HOLDING_NREGS = S_REG_HOLDING_NREGS;
    usRegHoldStart = usSRegHoldStart;

    /* it already plus one in modbus function method. */
    usAddress--;

    if ((usAddress >= REG_HOLDING_START) && (usAddress + usNRegs <= REG_HOLDING_START + REG_HOLDING_NREGS))
    {
        iRegIndex = usAddress - usRegHoldStart;
        switch (eMode)
        {
        /* read current register values from the protocol stack. */
        case MB_REG_READ:
            while (usNRegs > 0)
            {
                *pucRegBuffer++ = (UCHAR) (pusRegHoldingBuf[iRegIndex] >> 8);
                *pucRegBuffer++ = (UCHAR) (pusRegHoldingBuf[iRegIndex] & 0xFF);
                iRegIndex++;
                usNRegs--;
            }
            break;

        /* write current register values with new values from the protocol stack. */
        case MB_REG_WRITE:
            while (usNRegs > 0)
            {
                pusRegHoldingBuf[iRegIndex] = *pucRegBuffer++ << 8;
                pusRegHoldingBuf[iRegIndex] |= *pucRegBuffer++;
                iRegIndex++;
                usNRegs--;
            }
            break;
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }
    return eStatus;
#else
	return MB_ENOREG;
#endif
}
VIRTUAL eMBErrorCode eMBRegCoilsCB(UCHAR *pucRegBuffer, USHORT usAddress,
		USHORT usNCoils, eMBRegisterMode eMode) {
#if S_COIL_NCOILS > 0
    eMBErrorCode    eStatus = MB_ENOERR;
    USHORT          iRegIndex , iRegBitIndex , iNReg;
    UCHAR *         pucCoilBuf;
    USHORT          COIL_START;
    USHORT          COIL_NCOILS;
    USHORT          usCoilStart;
    iNReg =  usNCoils / 8 + 1;

    pucCoilBuf = ucSCoilBuf;
    COIL_START = S_COIL_START;
    COIL_NCOILS = S_COIL_NCOILS;
    usCoilStart = usSCoilStart;

    /* it already plus one in modbus function method. */
    usAddress--;

    if( ( usAddress >= COIL_START ) && ( usAddress + usNCoils <= COIL_START + COIL_NCOILS ) )
    {
        iRegIndex = (USHORT) (usAddress - usCoilStart) / 8;
        iRegBitIndex = (USHORT) (usAddress - usCoilStart) % 8;
        switch ( eMode )
        {
        /* read current coil values from the protocol stack. */
        case MB_REG_READ:
            while (iNReg > 0)
            {
                *pucRegBuffer++ = xMBUtilGetBits(&pucCoilBuf[iRegIndex++],
                        iRegBitIndex, 8);
                iNReg--;
            }
            pucRegBuffer--;
            /* last coils */
            usNCoils = usNCoils % 8;
            /* filling zero to high bit */
            *pucRegBuffer = *pucRegBuffer << (8 - usNCoils);
            *pucRegBuffer = *pucRegBuffer >> (8 - usNCoils);
            break;

            /* write current coil values with new values from the protocol stack. */
        case MB_REG_WRITE:
            while (iNReg > 1)
            {
                xMBUtilSetBits(&pucCoilBuf[iRegIndex++], iRegBitIndex, 8,
                        *pucRegBuffer++);
                iNReg--;
            }
            /* last coils */
            usNCoils = usNCoils % 8;
            /* xMBUtilSetBits has bug when ucNBits is zero */
            if (usNCoils != 0)
            {
                xMBUtilSetBits(&pucCoilBuf[iRegIndex++], iRegBitIndex, usNCoils,
                        *pucRegBuffer++);
            }
            break;
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }
    return eStatus;
#else
	return MB_ENOREG;
#endif
}

VIRTUAL eMBErrorCode eMBRegDiscreteCB(UCHAR *pucRegBuffer, USHORT usAddress,
		USHORT usNDiscrete) {
#if S_DISCRETE_INPUT_NDISCRETES > 0
    eMBErrorCode    eStatus = MB_ENOERR;
    USHORT          iRegIndex , iRegBitIndex , iNReg;
    UCHAR *         pucDiscreteInputBuf;
    USHORT          DISCRETE_INPUT_START;
    USHORT          DISCRETE_INPUT_NDISCRETES;
    USHORT          usDiscreteInputStart;
    iNReg =  usNDiscrete / 8 + 1;

    pucDiscreteInputBuf = ucSDiscInBuf;
    DISCRETE_INPUT_START = S_DISCRETE_INPUT_START;
    DISCRETE_INPUT_NDISCRETES = S_DISCRETE_INPUT_NDISCRETES;
    usDiscreteInputStart = usSDiscInStart;

    /* it already plus one in modbus function method. */
    usAddress--;

    if ((usAddress >= DISCRETE_INPUT_START) && (usAddress + usNDiscrete    <= DISCRETE_INPUT_START + DISCRETE_INPUT_NDISCRETES))
    {
        iRegIndex = (USHORT) (usAddress - usDiscreteInputStart) / 8;
        iRegBitIndex = (USHORT) (usAddress - usDiscreteInputStart) % 8;

        while (iNReg > 0)
        {
            *pucRegBuffer++ = xMBUtilGetBits(&pucDiscreteInputBuf[iRegIndex++],
                    iRegBitIndex, 8);
            iNReg--;
        }
        pucRegBuffer--;
        /* last discrete */
        usNDiscrete = usNDiscrete % 8;
        /* filling zero to high bit */
        *pucRegBuffer = *pucRegBuffer << (8 - usNDiscrete);
        *pucRegBuffer = *pucRegBuffer >> (8 - usNDiscrete);
    }
    else
    {
        eStatus = MB_ENOREG;
    }

    return eStatus;
#else
	return MB_ENOREG;
#endif
}
