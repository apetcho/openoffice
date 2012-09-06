/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_sc.hxx"
// INCLUDE ---------------------------------------------------------------

#include <rangelst.hxx>
#include <sfx2/app.hxx>
#include <sfx2/docfile.hxx>
#include <sfx2/objsh.hxx>
#include <basic/sbmeth.hxx>
#include <basic/sbmod.hxx>
#include <basic/sbstar.hxx>
#include <basic/sbx.hxx>
#include <svl/zforlist.hxx>
#include <tools/urlobj.hxx>
#include <rtl/logfile.hxx>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <com/sun/star/table/XCellRange.hpp>

#include "interpre.hxx"
#include "global.hxx"
#include "dbcolect.hxx"
#include "cell.hxx"
#include "callform.hxx"
#include "addincol.hxx"
#include "document.hxx"
#include "dociter.hxx"
#include "docoptio.hxx"
#include "scmatrix.hxx"
#include "adiasync.hxx"
#include "sc.hrc"
#include "cellsuno.hxx"
#include "optuno.hxx"
#include "rangeseq.hxx"
#include "addinlis.hxx"
#include "jumpmatrix.hxx"
#include "parclass.hxx"
#include "externalrefmgr.hxx"
#include "doubleref.hxx"

#include <math.h>
#include <float.h>
#include <map>
#include <algorithm>
#include <functional>
#include <memory>

using namespace com::sun::star;
using namespace formula;
using ::std::auto_ptr;

#define ADDIN_MAXSTRLEN 256

//-----------------------------static data -----------------

//-------------------------------------------------------------------------
// Funktionen fuer den Zugriff auf das Document
//-------------------------------------------------------------------------


void ScInterpreter::ReplaceCell( ScAddress& rPos )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::ReplaceCell" );
    ScInterpreterTableOpParams* pTOp = pDok->aTableOpList.First();
    while (pTOp)
    {
        if ( rPos == pTOp->aOld1 )
        {
            rPos = pTOp->aNew1;
            return ;
        }
        else if ( rPos == pTOp->aOld2 )
        {
            rPos = pTOp->aNew2;
            return ;
        }
        else
            pTOp = pDok->aTableOpList.Next();
    }
}


void ScInterpreter::ReplaceCell( SCCOL& rCol, SCROW& rRow, SCTAB& rTab )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::ReplaceCell" );
    ScAddress aCellPos( rCol, rRow, rTab );
    ScInterpreterTableOpParams* pTOp = pDok->aTableOpList.First();
    while (pTOp)
    {
        if ( aCellPos == pTOp->aOld1 )
        {
            rCol = pTOp->aNew1.Col();
            rRow = pTOp->aNew1.Row();
            rTab = pTOp->aNew1.Tab();
            return ;
        }
        else if ( aCellPos == pTOp->aOld2 )
        {
            rCol = pTOp->aNew2.Col();
            rRow = pTOp->aNew2.Row();
            rTab = pTOp->aNew2.Tab();
            return ;
        }
        else
            pTOp = pDok->aTableOpList.Next();
    }
}


sal_Bool ScInterpreter::IsTableOpInRange( const ScRange& rRange )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::IsTableOpInRange" );
    if ( rRange.aStart == rRange.aEnd )
        return sal_False;   // not considered to be a range in TableOp sense

    // we can't replace a single cell in a range
    ScInterpreterTableOpParams* pTOp = pDok->aTableOpList.First();
    while (pTOp)
    {
        if ( rRange.In( pTOp->aOld1 ) )
            return sal_True;
        if ( rRange.In( pTOp->aOld2 ) )
            return sal_True;
        pTOp = pDok->aTableOpList.Next();
    }
    return sal_False;
}


sal_uLong ScInterpreter::GetCellNumberFormat( const ScAddress& rPos, const ScBaseCell* pCell)
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::GetCellNumberFormat" );
    sal_uLong nFormat;
    sal_uInt16 nErr;
    if ( pCell )
    {
        if ( pCell->GetCellType() == CELLTYPE_FORMULA )
            nErr = ((ScFormulaCell*)pCell)->GetErrCode();
        else
            nErr = 0;
        nFormat = pDok->GetNumberFormat( rPos );
        if ( pCell->GetCellType() == CELLTYPE_FORMULA
          && ((nFormat % SV_COUNTRY_LANGUAGE_OFFSET) == 0) )
            nFormat = ((ScFormulaCell*)pCell)->GetStandardFormat( *pFormatter,
                nFormat );
    }
    else
    {
        nFormat = pDok->GetNumberFormat( rPos );
        nErr = 0;
    }
    SetError(nErr);
    return nFormat;
}


/// Only ValueCell, formula cells already store the result rounded.
double ScInterpreter::GetValueCellValue( const ScAddress& rPos, const ScValueCell* pCell )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::GetValueCellValue" );
    double fVal = pCell->GetValue();
    if ( bCalcAsShown && fVal != 0.0 )
    {
        sal_uLong nFormat = pDok->GetNumberFormat( rPos );
        fVal = pDok->RoundValueAsShown( fVal, nFormat );
    }
    return fVal;
}


/** Convert string content to numeric value.
    
    Converted are only integer numbers including exponent, and ISO 8601 dates 
    and times in their extended formats with separators. Anything else, 
    especially fractional numeric values with decimal separators or dates other 
    than ISO 8601 would be locale dependent and is a no-no. Leading and 
    trailing blanks are ignored.

    The following ISO 8601 formats are converted:

    CCYY-MM-DD
    CCYY-MM-DDThh:mm
    CCYY-MM-DDThh:mm:ss
    CCYY-MM-DDThh:mm:ss,s
    CCYY-MM-DDThh:mm:ss.s
    hh:mm
    hh:mm:ss
    hh:mm:ss,s
    hh:mm:ss.s

    The century CC may not be omitted and the two-digit year setting is not 
    taken into account. Instead of the T date and time separator exactly one 
    blank may be used.

    If a date is given, it must be a valid Gregorian calendar date. In this 
    case the optional time must be in the range 00:00 to 23:59:59.99999...
    If only time is given, it may have any value for hours, taking elapsed time 
    into account; minutes and seconds are limited to the value 59 as well.
 */

double ScInterpreter::ConvertStringToValue( const String& rStr )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::ConvertStringToValue" );
    double fValue = 0.0;
    if (mnStringNoValueError == errCellNoValue)
    {
        // Requested that all strings result in 0, error handled by caller.
        SetError( mnStringNoValueError);
        return fValue;
    }
    ::rtl::OUString aStr( rStr);
    rtl_math_ConversionStatus eStatus;
    sal_Int32 nParseEnd;
    // Decimal and group separator 0 => only integer and possibly exponent, 
    // stops at first non-digit non-sign.
    fValue = ::rtl::math::stringToDouble( aStr, 0, 0, &eStatus, &nParseEnd);
    sal_Int32 nLen;
    if (eStatus == rtl_math_ConversionStatus_Ok && nParseEnd < (nLen = aStr.getLength()))
    {
        // Not at string end, check for trailing blanks or switch to date or 
        // time parsing or bail out.
        const sal_Unicode* const pStart = aStr.getStr();
        const sal_Unicode* p = pStart + nParseEnd;
        const sal_Unicode* const pStop = pStart + nLen;
        switch (*p++)
        {
            case ' ':
                while (p < pStop && *p == ' ')
                    ++p;
                if (p < pStop)
                    SetError( mnStringNoValueError);
                break;
            case '-':
            case ':':
                {
                    bool bDate = (*(p-1) == '-');
                    enum State { year = 0, month, day, hour, minute, second, fraction, done, blank, stop };
                    sal_Int32 nUnit[done] = {0,0,0,0,0,0,0};
                    const sal_Int32 nLimit[done] = {0,12,31,0,59,59,0};
                    State eState = (bDate ? month : minute);
                    nCurFmtType = (bDate ? NUMBERFORMAT_DATE : NUMBERFORMAT_TIME);
                    nUnit[eState-1] = aStr.copy( 0, nParseEnd).toInt32();
                    const sal_Unicode* pLastStart = p;
                    // Ensure there's no preceding sign. Negative dates 
                    // currently aren't handled correctly. Also discard 
                    // +CCYY-MM-DD
                    p = pStart;
                    while (p < pStop && *p == ' ')
                        ++p;
                    if (p < pStop && !CharClass::isAsciiDigit(*p))
                        SetError( mnStringNoValueError);
                    p = pLastStart;
                    while (p < pStop && !nGlobalError && eState < blank)
                    {
                        if (eState == minute)
                            nCurFmtType |= NUMBERFORMAT_TIME;
                        if (CharClass::isAsciiDigit(*p))
                        {
                            // Maximum 2 digits per unit, except fractions.
                            if (p - pLastStart >= 2 && eState != fraction)
                                SetError( mnStringNoValueError);
                        }
                        else if (p > pLastStart)
                        {
                            // We had at least one digit.
                            if (eState < done)
                            {
                                nUnit[eState] = aStr.copy( pLastStart - pStart, p - pLastStart).toInt32();
                                if (nLimit[eState] && nLimit[eState] < nUnit[eState])
                                    SetError( mnStringNoValueError);
                            }
                            pLastStart = p + 1;     // hypothetical next start
                            // Delimiters must match, a trailing delimiter 
                            // yields an invalid date/time.
                            switch (eState)
                            {
                                case month:
                                    // Month must be followed by separator and 
                                    // day, no trailing blanks.
                                    if (*p != '-' || (p+1 == pStop))
                                        SetError( mnStringNoValueError);
                                    break;
                                case day:
                                    if ((*p != 'T' || (p+1 == pStop)) && *p != ' ')
                                        SetError( mnStringNoValueError);
                                    // Take one blank as a valid delimiter 
                                    // between date and time.
                                    break;
                                case hour:
                                    // Hour must be followed by separator and 
                                    // minute, no trailing blanks.
                                    if (*p != ':' || (p+1 == pStop))
                                        SetError( mnStringNoValueError);
                                    break;
                                case minute:
                                    if ((*p != ':' || (p+1 == pStop)) && *p != ' ')
                                        SetError( mnStringNoValueError);
                                    if (*p == ' ')
                                        eState = done;
                                    break;
                                case second:
                                    if (((*p != ',' && *p != '.') || (p+1 == pStop)) && *p != ' ')
                                        SetError( mnStringNoValueError);
                                    if (*p == ' ')
                                        eState = done;
                                    break;
                                case fraction:
                                    eState = done;
                                    break;
                                case year:
                                case done:
                                case blank:
                                case stop:
                                    SetError( mnStringNoValueError);
                                    break;
                            }
                            eState = static_cast<State>(eState + 1);
                        }
                        else
                            SetError( mnStringNoValueError);
                        ++p;
                    }
                    if (eState == blank)
                    {
                        while (p < pStop && *p == ' ')
                            ++p;
                        if (p < pStop)
                            SetError( mnStringNoValueError);
                        eState = stop;
                    }

                    // Month without day, or hour without minute.
                    if (eState == month || (eState == day && p <= pLastStart) ||
                            eState == hour || (eState == minute && p <= pLastStart))
                        SetError( mnStringNoValueError);

                    if (!nGlobalError)
                    {
                        // Catch the very last unit at end of string.
                        if (p > pLastStart && eState < done)
                        {
                            nUnit[eState] = aStr.copy( pLastStart - pStart, p - pLastStart).toInt32();
                            if (nLimit[eState] && nLimit[eState] < nUnit[eState])
                                SetError( mnStringNoValueError);
                        }
                        if (bDate && nUnit[hour] > 23)
                            SetError( mnStringNoValueError);
                        if (!nGlobalError)
                        {
                            if (bDate && nUnit[day] == 0)
                                nUnit[day] = 1;
                            double fFraction = (nUnit[fraction] <= 0 ? 0.0 : 
                                    ::rtl::math::pow10Exp( nUnit[fraction],
                                        static_cast<int>( -ceil( log10( static_cast<double>( nUnit[fraction]))))));
                            fValue = (bDate ? GetDateSerial( 
                                        sal::static_int_cast<sal_Int16>(nUnit[year]), 
                                        sal::static_int_cast<sal_Int16>(nUnit[month]), 
                                        sal::static_int_cast<sal_Int16>(nUnit[day]), 
                                        true) : 0.0);
                            fValue += ((nUnit[hour] * 3600) + (nUnit[minute] * 60) + nUnit[second] + fFraction) / 86400.0;
                        }
                    }
                }
                break;
            default:
                SetError( mnStringNoValueError);
        }
        if (nGlobalError)
            fValue = 0.0;
    }
    return fValue;
}


double ScInterpreter::GetCellValue( const ScAddress& rPos, const ScBaseCell* pCell )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::GetCellValue" );
    sal_uInt16 nErr = nGlobalError;
    nGlobalError = 0;
    double nVal = GetCellValueOrZero( rPos, pCell );
    if ( !nGlobalError || nGlobalError == errCellNoValue )
        nGlobalError = nErr;
    return nVal;
}


double ScInterpreter::GetCellValueOrZero( const ScAddress& rPos, const ScBaseCell* pCell )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::GetCellValueOrZero" );
    double fValue = 0.0;
    if (pCell)
    {
        CellType eType = pCell->GetCellType();
        switch ( eType )
        {
            case CELLTYPE_FORMULA:
            {
                ScFormulaCell* pFCell = (ScFormulaCell*) pCell;
                sal_uInt16 nErr = pFCell->GetErrCode();
                if( !nErr )
                {
                    if (pFCell->IsValue())
                    {
                        fValue = pFCell->GetValue();
                        pDok->GetNumberFormatInfo( nCurFmtType, nCurFmtIndex,
                            rPos, pFCell );
                    }
                    else
                    {
                        String aStr;
                        pFCell->GetString( aStr );
                        fValue = ConvertStringToValue( aStr );
                    }
                }
                else
                {
                    fValue = 0.0;
                    SetError(nErr);
                }
            }
            break;
            case CELLTYPE_VALUE:
            {
                fValue = ((ScValueCell*)pCell)->GetValue();
                nCurFmtIndex = pDok->GetNumberFormat( rPos );
                nCurFmtType = pFormatter->GetType( nCurFmtIndex );
                if ( bCalcAsShown && fValue != 0.0 )
                    fValue = pDok->RoundValueAsShown( fValue, nCurFmtIndex );
            }
            break;
            case  CELLTYPE_STRING:
            case  CELLTYPE_EDIT:
            {
                // SUM(A1:A2) differs from A1+A2. No good. But people insist on 
                // it ... #i5658#
                String aStr;
                if ( eType == CELLTYPE_STRING )
                    ((ScStringCell*)pCell)->GetString( aStr );
                else
                    ((ScEditCell*)pCell)->GetString( aStr );
                fValue = ConvertStringToValue( aStr );
            }
            break;
            case CELLTYPE_NONE:
            case CELLTYPE_NOTE:
                fValue = 0.0;       // empty or broadcaster cell
            break;
            case CELLTYPE_SYMBOLS:
#if DBG_UTIL
            case CELLTYPE_DESTROYED:
#endif
                SetError(errCellNoValue);
                fValue = 0.0;
            break;
        }
    }
    else
        fValue = 0.0;
    return fValue;
}


void ScInterpreter::GetCellString( String& rStr, const ScBaseCell* pCell )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::GetCellString" );
    sal_uInt16 nErr = 0;
    if (pCell)
    {
        switch (pCell->GetCellType())
        {
            case CELLTYPE_STRING:
                ((ScStringCell*) pCell)->GetString(rStr);
            break;
            case CELLTYPE_EDIT:
                ((ScEditCell*) pCell)->GetString(rStr);
            break;
            case CELLTYPE_FORMULA:
            {
                ScFormulaCell* pFCell = (ScFormulaCell*) pCell;
                nErr = pFCell->GetErrCode();
                if (pFCell->IsValue())
                {
                    double fVal = pFCell->GetValue();
                    sal_uLong nIndex = pFormatter->GetStandardFormat(
                                        NUMBERFORMAT_NUMBER,
                                        ScGlobal::eLnge);
                    pFormatter->GetInputLineString(fVal, nIndex, rStr);
                }
                else
                    pFCell->GetString(rStr);
            }
            break;
            case CELLTYPE_VALUE:
            {
                double fVal = ((ScValueCell*) pCell)->GetValue();
                sal_uLong nIndex = pFormatter->GetStandardFormat(
                                        NUMBERFORMAT_NUMBER,
                                        ScGlobal::eLnge);
                pFormatter->GetInputLineString(fVal, nIndex, rStr);
            }
            break;
            default:
                rStr = ScGlobal::GetEmptyString();
            break;
        }
    }
    else
        rStr = ScGlobal::GetEmptyString();
    SetError(nErr);
}


sal_Bool ScInterpreter::CreateDoubleArr(SCCOL nCol1, SCROW nRow1, SCTAB nTab1,
                            SCCOL nCol2, SCROW nRow2, SCTAB nTab2, sal_uInt8* pCellArr)
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::CreateDoubleArr" );

    // Old Add-Ins are hard limited to sal_uInt16 values.
#if MAXCOLCOUNT_DEFINE > USHRT_MAX
#error Add check for columns > USHRT_MAX!
#endif
    if (nRow1 > USHRT_MAX || nRow2 > USHRT_MAX)
        return sal_False;

    sal_uInt16 nCount = 0;
    sal_uInt16* p = (sal_uInt16*) pCellArr;
    *p++ = static_cast<sal_uInt16>(nCol1);
    *p++ = static_cast<sal_uInt16>(nRow1);
    *p++ = static_cast<sal_uInt16>(nTab1);
    *p++ = static_cast<sal_uInt16>(nCol2);
    *p++ = static_cast<sal_uInt16>(nRow2);
    *p++ = static_cast<sal_uInt16>(nTab2);
    sal_uInt16* pCount = p;
    *p++ = 0;
    sal_uInt16 nPos = 14;
    SCTAB nTab = nTab1;
    ScAddress aAdr;
    while (nTab <= nTab2)
    {
        aAdr.SetTab( nTab );
        SCROW nRow = nRow1;
        while (nRow <= nRow2)
        {
            aAdr.SetRow( nRow );
            SCCOL nCol = nCol1;
            while (nCol <= nCol2)
            {
                aAdr.SetCol( nCol );
                ScBaseCell* pCell = pDok->GetCell( aAdr );
                if (pCell)
                {
                    sal_uInt16  nErr = 0;
                    double  nVal = 0.0;
                    sal_Bool    bOk = sal_True;
                    switch ( pCell->GetCellType() )
                    {
                        case CELLTYPE_VALUE :
                            nVal = GetValueCellValue( aAdr, (ScValueCell*)pCell );
                            break;
                        case CELLTYPE_FORMULA :
                            if (((ScFormulaCell*)pCell)->IsValue())
                            {
                                nErr = ((ScFormulaCell*)pCell)->GetErrCode();
                                nVal = ((ScFormulaCell*)pCell)->GetValue();
                            }
                            else
                                bOk = sal_False;
                            break;
                        default :
                            bOk = sal_False;
                            break;
                    }
                    if (bOk)
                    {
                        if ((nPos + (4 * sizeof(sal_uInt16)) + sizeof(double)) > MAXARRSIZE)
                            return sal_False;
                        *p++ = static_cast<sal_uInt16>(nCol);
                        *p++ = static_cast<sal_uInt16>(nRow);
                        *p++ = static_cast<sal_uInt16>(nTab);
                        *p++ = nErr;
                        memcpy( p, &nVal, sizeof(double));
                        nPos += 8 + sizeof(double);
                        p = (sal_uInt16*) ( pCellArr + nPos );
                        nCount++;
                    }
                }
                nCol++;
            }
            nRow++;
        }
        nTab++;
    }
    *pCount = nCount;
    return sal_True;
}


sal_Bool ScInterpreter::CreateStringArr(SCCOL nCol1, SCROW nRow1, SCTAB nTab1,
                                    SCCOL nCol2, SCROW nRow2, SCTAB nTab2,
                                    sal_uInt8* pCellArr)
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::CreateStringArr" );

    // Old Add-Ins are hard limited to sal_uInt16 values.
#if MAXCOLCOUNT_DEFINE > USHRT_MAX
#error Add check for columns > USHRT_MAX!
#endif
    if (nRow1 > USHRT_MAX || nRow2 > USHRT_MAX)
        return sal_False;

    sal_uInt16 nCount = 0;
    sal_uInt16* p = (sal_uInt16*) pCellArr;
    *p++ = static_cast<sal_uInt16>(nCol1);
    *p++ = static_cast<sal_uInt16>(nRow1);
    *p++ = static_cast<sal_uInt16>(nTab1);
    *p++ = static_cast<sal_uInt16>(nCol2);
    *p++ = static_cast<sal_uInt16>(nRow2);
    *p++ = static_cast<sal_uInt16>(nTab2);
    sal_uInt16* pCount = p;
    *p++ = 0;
    sal_uInt16 nPos = 14;
    SCTAB nTab = nTab1;
    while (nTab <= nTab2)
    {
        SCROW nRow = nRow1;
        while (nRow <= nRow2)
        {
            SCCOL nCol = nCol1;
            while (nCol <= nCol2)
            {
                ScBaseCell* pCell;
                pDok->GetCell(nCol, nRow, nTab, pCell);
                if (pCell)
                {
                    String  aStr;
                    sal_uInt16  nErr = 0;
                    sal_Bool    bOk = sal_True;
                    switch ( pCell->GetCellType() )
                    {
                        case CELLTYPE_STRING :
                            ((ScStringCell*)pCell)->GetString(aStr);
                            break;
                        case CELLTYPE_EDIT :
                            ((ScEditCell*)pCell)->GetString(aStr);
                            break;
                        case CELLTYPE_FORMULA :
                            if (!((ScFormulaCell*)pCell)->IsValue())
                            {
                                nErr = ((ScFormulaCell*)pCell)->GetErrCode();
                                ((ScFormulaCell*)pCell)->GetString(aStr);
                            }
                            else
                                bOk = sal_False;
                            break;
                        default :
                            bOk = sal_False;
                            break;
                    }
                    if (bOk)
                    {
                        ByteString aTmp( aStr, osl_getThreadTextEncoding() );
                        // In case the xub_StrLen will be longer than USHORT
                        // one day, and room for pad byte check.
                        if ( aTmp.Len() > ((sal_uInt16)(~0)) - 2 )
                            return sal_False;
                        // Append a 0-pad-byte if string length is not even
                        //! MUST be sal_uInt16 and not xub_StrLen
                        sal_uInt16 nStrLen = (sal_uInt16) aTmp.Len();
                        sal_uInt16 nLen = ( nStrLen + 2 ) & ~1;

                        if (((sal_uLong)nPos + (5 * sizeof(sal_uInt16)) + nLen) > MAXARRSIZE)
                            return sal_False;
                        *p++ = static_cast<sal_uInt16>(nCol);
                        *p++ = static_cast<sal_uInt16>(nRow);
                        *p++ = static_cast<sal_uInt16>(nTab);
                        *p++ = nErr;
                        *p++ = nLen;
                        memcpy( p, aTmp.GetBuffer(), nStrLen + 1);
                        nPos += 10 + nStrLen + 1;
                        sal_uInt8* q = ( pCellArr + nPos );
                        if( !nStrLen & 1 )
                            *q++ = 0, nPos++;
                        p = (sal_uInt16*) ( pCellArr + nPos );
                        nCount++;
                    }
                }
                nCol++;
            }
            nRow++;
        }
        nTab++;
    }
    *pCount = nCount;
    return sal_True;
}


sal_Bool ScInterpreter::CreateCellArr(SCCOL nCol1, SCROW nRow1, SCTAB nTab1,
                                  SCCOL nCol2, SCROW nRow2, SCTAB nTab2,
                                  sal_uInt8* pCellArr)
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::CreateCellArr" );

    // Old Add-Ins are hard limited to sal_uInt16 values.
#if MAXCOLCOUNT_DEFINE > USHRT_MAX
#error Add check for columns > USHRT_MAX!
#endif
    if (nRow1 > USHRT_MAX || nRow2 > USHRT_MAX)
        return sal_False;

    sal_uInt16 nCount = 0;
    sal_uInt16* p = (sal_uInt16*) pCellArr;
    *p++ = static_cast<sal_uInt16>(nCol1);
    *p++ = static_cast<sal_uInt16>(nRow1);
    *p++ = static_cast<sal_uInt16>(nTab1);
    *p++ = static_cast<sal_uInt16>(nCol2);
    *p++ = static_cast<sal_uInt16>(nRow2);
    *p++ = static_cast<sal_uInt16>(nTab2);
    sal_uInt16* pCount = p;
    *p++ = 0;
    sal_uInt16 nPos = 14;
    SCTAB nTab = nTab1;
    ScAddress aAdr;
    while (nTab <= nTab2)
    {
        aAdr.SetTab( nTab );
        SCROW nRow = nRow1;
        while (nRow <= nRow2)
        {
            aAdr.SetRow( nRow );
            SCCOL nCol = nCol1;
            while (nCol <= nCol2)
            {
                aAdr.SetCol( nCol );
                ScBaseCell* pCell = pDok->GetCell( aAdr );
                if (pCell)
                {
                    sal_uInt16  nErr = 0;
                    sal_uInt16  nType = 0; // 0 = Zahl; 1 = String
                    double  nVal = 0.0;
                    String  aStr;
                    sal_Bool    bOk = sal_True;
                    switch ( pCell->GetCellType() )
                    {
                        case CELLTYPE_STRING :
                            ((ScStringCell*)pCell)->GetString(aStr);
                            nType = 1;
                            break;
                        case CELLTYPE_EDIT :
                            ((ScEditCell*)pCell)->GetString(aStr);
                            nType = 1;
                            break;
                        case CELLTYPE_VALUE :
                            nVal = GetValueCellValue( aAdr, (ScValueCell*)pCell );
                            break;
                        case CELLTYPE_FORMULA :
                            nErr = ((ScFormulaCell*)pCell)->GetErrCode();
                            if (((ScFormulaCell*)pCell)->IsValue())
                                nVal = ((ScFormulaCell*)pCell)->GetValue();
                            else
                                ((ScFormulaCell*)pCell)->GetString(aStr);
                            break;
                        default :
                            bOk = sal_False;
                            break;
                    }
                    if (bOk)
                    {
                        if ((nPos + (5 * sizeof(sal_uInt16))) > MAXARRSIZE)
                            return sal_False;
                        *p++ = static_cast<sal_uInt16>(nCol);
                        *p++ = static_cast<sal_uInt16>(nRow);
                        *p++ = static_cast<sal_uInt16>(nTab);
                        *p++ = nErr;
                        *p++ = nType;
                        nPos += 10;
                        if (nType == 0)
                        {
                            if ((nPos + sizeof(double)) > MAXARRSIZE)
                                return sal_False;
                            memcpy( p, &nVal, sizeof(double));
                            nPos += sizeof(double);
                        }
                        else
                        {
                            ByteString aTmp( aStr, osl_getThreadTextEncoding() );
                            // In case the xub_StrLen will be longer than USHORT
                            // one day, and room for pad byte check.
                            if ( aTmp.Len() > ((sal_uInt16)(~0)) - 2 )
                                return sal_False;
                            // Append a 0-pad-byte if string length is not even
                            //! MUST be sal_uInt16 and not xub_StrLen
                            sal_uInt16 nStrLen = (sal_uInt16) aTmp.Len();
                            sal_uInt16 nLen = ( nStrLen + 2 ) & ~1;
                            if ( ((sal_uLong)nPos + 2 + nLen) > MAXARRSIZE)
                                return sal_False;
                            *p++ = nLen;
                            memcpy( p, aTmp.GetBuffer(), nStrLen + 1);
                            nPos += 2 + nStrLen + 1;
                            sal_uInt8* q = ( pCellArr + nPos );
                            if( !nStrLen & 1 )
                                *q++ = 0, nPos++;
                        }
                        nCount++;
                        p = (sal_uInt16*) ( pCellArr + nPos );
                    }
                }
                nCol++;
            }
            nRow++;
        }
        nTab++;
    }
    *pCount = nCount;
    return sal_True;
}


//-----------------------------------------------------------------------------
// Stack operations
//-----------------------------------------------------------------------------


// Also releases a TempToken if appropriate.

void ScInterpreter::PushWithoutError( FormulaToken& r )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PushWithoutError" );
    if ( sp >= MAXSTACK )
        SetError( errStackOverflow );
    else
    {
        nCurFmtType = NUMBERFORMAT_UNDEFINED;
        r.IncRef();
        if( sp >= maxsp )
            maxsp = sp + 1;
        else
            pStack[ sp ]->DecRef();
        pStack[ sp ] = (ScToken*) &r;
        ++sp;
    }
}

void ScInterpreter::Push( FormulaToken& r )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::Push" );
    if ( sp >= MAXSTACK )
        SetError( errStackOverflow );
    else
    {
        if (nGlobalError)
        {
            if (r.GetType() == svError)
            {
                r.SetError( nGlobalError);
                PushWithoutError( r);
            }
            else
                PushWithoutError( *(new FormulaErrorToken( nGlobalError)));
        }
        else
            PushWithoutError( r);
    }
}


void ScInterpreter::PushTempToken( FormulaToken* p )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PushTempToken" );
    if ( sp >= MAXSTACK )
    {
        SetError( errStackOverflow );
        if (!p->GetRef())
            //! p is a dangling pointer hereafter!
            p->Delete();
    }
    else
    {
        if (nGlobalError)
        {
            if (p->GetType() == svError)
            {
                p->SetError( nGlobalError);
                PushTempTokenWithoutError( p);
            }
            else
            {
                if (!p->GetRef())
                    //! p is a dangling pointer hereafter!
                    p->Delete();
                PushTempTokenWithoutError( new FormulaErrorToken( nGlobalError));
            }
        }
        else
            PushTempTokenWithoutError( p);
    }
}


void ScInterpreter::PushTempTokenWithoutError( FormulaToken* p )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PushTempTokenWithoutError" );
    p->IncRef();
    if ( sp >= MAXSTACK )
    {
        SetError( errStackOverflow );
        //! p may be a dangling pointer hereafter!
        p->DecRef();
    }
    else
    {
        if( sp >= maxsp )
            maxsp = sp + 1;
        else
            pStack[ sp ]->DecRef();
        pStack[ sp ] = p;
        ++sp;
    }
}


void ScInterpreter::PushTempToken( const FormulaToken& r )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PushTempToken" );
    if (!IfErrorPushError())
        PushTempTokenWithoutError( r.Clone());
}


void ScInterpreter::PushCellResultToken( bool bDisplayEmptyAsString,
        const ScAddress & rAddress, short * pRetTypeExpr, sal_uLong * pRetIndexExpr )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PushCellResultToken" );
    ScBaseCell* pCell = pDok->GetCell( rAddress);
    if (!pCell || pCell->HasEmptyData())
    {
        if (pRetTypeExpr && pRetIndexExpr)
            pDok->GetNumberFormatInfo( *pRetTypeExpr, *pRetIndexExpr, rAddress, pCell);
        bool bInherited = (GetCellType( pCell) == CELLTYPE_FORMULA);
        PushTempToken( new ScEmptyCellToken( bInherited, bDisplayEmptyAsString));
        return;
    }
    sal_uInt16 nErr;
    if ((nErr = pCell->GetErrorCode()) != 0)
    {
        PushError( nErr);
        if (pRetTypeExpr)
            *pRetTypeExpr = NUMBERFORMAT_UNDEFINED;
        if (pRetIndexExpr)
            *pRetIndexExpr = 0;
    }
    else if (pCell->HasStringData())
    {
        String aRes;
        GetCellString( aRes, pCell);
        PushString( aRes);
        if (pRetTypeExpr)
            *pRetTypeExpr = NUMBERFORMAT_TEXT;
        if (pRetIndexExpr)
            *pRetIndexExpr = 0;
    }
    else
    {
        double fVal = GetCellValue( rAddress, pCell);
        PushDouble( fVal);
        if (pRetTypeExpr)
            *pRetTypeExpr = nCurFmtType;
        if (pRetIndexExpr)
            *pRetIndexExpr = nCurFmtIndex;
    }
}


// Simply throw away TOS.

void ScInterpreter::Pop()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::Pop" );
    if( sp )
        sp--;
    else
        SetError(errUnknownStackVariable);
}


// Simply throw away TOS and set error code, used with ocIsError et al.

void ScInterpreter::PopError()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PopError" );
    if( sp )
    {
        sp--;
        if (pStack[sp]->GetType() == svError)
            nGlobalError = pStack[sp]->GetError();
    }
    else
        SetError(errUnknownStackVariable);
}


FormulaTokenRef ScInterpreter::PopToken()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PopToken" );
    if (sp)
    {
        sp--;
        FormulaToken* p = pStack[ sp ];
        if (p->GetType() == svError)
            nGlobalError = p->GetError();
        return p;
    }
    else
        SetError(errUnknownStackVariable);
    return NULL;
}


double ScInterpreter::PopDouble()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PopDouble" );
    nCurFmtType = NUMBERFORMAT_NUMBER;
    nCurFmtIndex = 0;
    if( sp )
    {
        --sp;
        FormulaToken* p = pStack[ sp ];
        switch (p->GetType())
        {
            case svError:
                nGlobalError = p->GetError();
                break;
            case svDouble:
                return p->GetDouble();
            case svEmptyCell:
            case svMissing:
                return 0.0;
            default:
                SetError( errIllegalArgument);
        }
    }
    else
        SetError( errUnknownStackVariable);
    return 0.0;
}


const String& ScInterpreter::PopString()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PopString" );
    nCurFmtType = NUMBERFORMAT_TEXT;
    nCurFmtIndex = 0;
    if( sp )
    {
        --sp;
        FormulaToken* p = pStack[ sp ];
        switch (p->GetType())
        {
            case svError:
                nGlobalError = p->GetError();
                break;
            case svString:
                return p->GetString();
            case svEmptyCell:
            case svMissing:
                return EMPTY_STRING;
            default:
                SetError( errIllegalArgument);
        }
    }
    else
        SetError( errUnknownStackVariable);
    return EMPTY_STRING;
}


void ScInterpreter::ValidateRef( const ScSingleRefData & rRef )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::ValidateRef" );
    SCCOL nCol;
    SCROW nRow;
    SCTAB nTab;
    SingleRefToVars( rRef, nCol, nRow, nTab);
}


void ScInterpreter::ValidateRef( const ScComplexRefData & rRef )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::ValidateRef" );
    ValidateRef( rRef.Ref1);
    ValidateRef( rRef.Ref2);
}


void ScInterpreter::ValidateRef( const ScRefList & rRefList )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::ValidateRef" );
    ScRefList::const_iterator it( rRefList.begin());
    ScRefList::const_iterator end( rRefList.end());
    for ( ; it != end; ++it)
    {
        ValidateRef( *it);
    }
}


void ScInterpreter::SingleRefToVars( const ScSingleRefData & rRef,
        SCCOL & rCol, SCROW & rRow, SCTAB & rTab )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::SingleRefToVars" );
    if ( rRef.IsColRel() )
        rCol = aPos.Col() + rRef.nRelCol;
    else
        rCol = rRef.nCol;
    if ( rRef.IsRowRel() )
        rRow = aPos.Row() + rRef.nRelRow;
    else
        rRow = rRef.nRow;
    if ( rRef.IsTabRel() )
        rTab = aPos.Tab() + rRef.nRelTab;
    else
        rTab = rRef.nTab;
    if( !ValidCol( rCol) || rRef.IsColDeleted() )
        SetError( errNoRef ), rCol = 0;
    if( !ValidRow( rRow) || rRef.IsRowDeleted() )
        SetError( errNoRef ), rRow = 0;
    if( !ValidTab( rTab, pDok->GetTableCount() - 1) || rRef.IsTabDeleted() )
        SetError( errNoRef ), rTab = 0;
}


void ScInterpreter::PopSingleRef(SCCOL& rCol, SCROW &rRow, SCTAB& rTab)
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PopSingleRef" );
    if( sp )
    {
        --sp;
        FormulaToken* p = pStack[ sp ];
        switch (p->GetType())
        {
            case svError:
                nGlobalError = p->GetError();
                break;
            case svSingleRef:
                SingleRefToVars( static_cast<ScToken*>(p)->GetSingleRef(), rCol, rRow, rTab);
                if ( pDok->aTableOpList.Count() > 0 )
                    ReplaceCell( rCol, rRow, rTab );
                break;
            default:
                SetError( errIllegalParameter);
        }
    }
    else
        SetError( errUnknownStackVariable);
}


void ScInterpreter::PopSingleRef( ScAddress& rAdr )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PopSingleRef" );
    if( sp )
    {
        --sp;
        FormulaToken* p = pStack[ sp ];
        switch (p->GetType())
        {
            case svError:
                nGlobalError = p->GetError();
                break;
            case svSingleRef:
                {
                    SCCOL nCol;
                    SCROW nRow;
                    SCTAB nTab;
                    SingleRefToVars( static_cast<ScToken*>(p)->GetSingleRef(), nCol, nRow, nTab);
                    rAdr.Set( nCol, nRow, nTab );
                    if ( pDok->aTableOpList.Count() > 0 )
                        ReplaceCell( rAdr );
                }
                break;
            default:
                SetError( errIllegalParameter);
        }
    }
    else
        SetError( errUnknownStackVariable);
}


void ScInterpreter::DoubleRefToVars( const ScToken* p,
        SCCOL& rCol1, SCROW &rRow1, SCTAB& rTab1,
        SCCOL& rCol2, SCROW &rRow2, SCTAB& rTab2,
        sal_Bool bDontCheckForTableOp )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::DoubleRefToVars" );
    const ScComplexRefData& rCRef = p->GetDoubleRef();
    SingleRefToVars( rCRef.Ref1, rCol1, rRow1, rTab1);
    SingleRefToVars( rCRef.Ref2, rCol2, rRow2, rTab2);
    if ( pDok->aTableOpList.Count() > 0 && !bDontCheckForTableOp )
    {
        ScRange aRange( rCol1, rRow1, rTab1, rCol2, rRow2, rTab2 );
        if ( IsTableOpInRange( aRange ) )
            SetError( errIllegalParameter );
    }
}

ScDBRangeBase* ScInterpreter::PopDoubleRef()
{
    if (!sp)
    {    
        SetError(errUnknownStackVariable);
        return NULL;
    }

    --sp;
    FormulaToken* p = pStack[sp];
    switch (p->GetType())
    {
        case svError:
            nGlobalError = p->GetError();
        break;
        case svDoubleRef:
        {
            SCCOL nCol1, nCol2;
            SCROW nRow1, nRow2;
            SCTAB nTab1, nTab2;
            DoubleRefToVars(static_cast<ScToken*>(p), 
                            nCol1, nRow1, nTab1, nCol2, nRow2, nTab2, false);
            
            return new ScDBInternalRange(pDok,
                ScRange(nCol1, nRow1, nTab1, nCol2, nRow2, nTab2));
        }
        case svMatrix:
        {
            ScMatrixRef pMat = static_cast<ScToken*>(p)->GetMatrix();
            return new ScDBExternalRange(pDok, pMat);
        }
        default:
            SetError( errIllegalParameter);
    }
    return NULL;
}

void ScInterpreter::PopDoubleRef(SCCOL& rCol1, SCROW &rRow1, SCTAB& rTab1,
                                 SCCOL& rCol2, SCROW &rRow2, SCTAB& rTab2,
                                 sal_Bool bDontCheckForTableOp )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PopDoubleRef" );
    if( sp )
    {
        --sp;
        FormulaToken* p = pStack[ sp ];
        switch (p->GetType())
        {
            case svError:
                nGlobalError = p->GetError();
                break;
            case svDoubleRef:
                DoubleRefToVars( static_cast<ScToken*>(p), rCol1, rRow1, rTab1, rCol2, rRow2, rTab2,
                        bDontCheckForTableOp);
                break;
            default:
                SetError( errIllegalParameter);
        }
    }
    else
        SetError( errUnknownStackVariable);
}


void ScInterpreter::DoubleRefToRange( const ScComplexRefData & rCRef,
        ScRange & rRange, sal_Bool bDontCheckForTableOp )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::DoubleRefToRange" );
    SCCOL nCol;
    SCROW nRow;
    SCTAB nTab;
    SingleRefToVars( rCRef.Ref1, nCol, nRow, nTab);
    rRange.aStart.Set( nCol, nRow, nTab );
    SingleRefToVars( rCRef.Ref2, nCol, nRow, nTab);
    rRange.aEnd.Set( nCol, nRow, nTab );
    if ( pDok->aTableOpList.Count() > 0 && !bDontCheckForTableOp )
    {
        if ( IsTableOpInRange( rRange ) )
            SetError( errIllegalParameter );
    }
}


void ScInterpreter::PopDoubleRef( ScRange & rRange, short & rParam, size_t & rRefInList )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PopDoubleRef" );
    if (sp)
    {
        formula::FormulaToken* pToken = pStack[ sp-1 ];
        ScToken* p = static_cast<ScToken*>(pToken);
        switch (pToken->GetType())
        {
            case svError:
                nGlobalError = p->GetError();
                break;
            case svDoubleRef:
                --sp;
                DoubleRefToRange( p->GetDoubleRef(), rRange);
                break;
            case svRefList:
                {
                    const ScRefList* pList = p->GetRefList();
                    if (rRefInList < pList->size())
                    {
                        DoubleRefToRange( (*pList)[rRefInList], rRange);
                        if (++rRefInList < pList->size())
                            ++rParam;
                        else
                        {
                            --sp;
                            rRefInList = 0;
                        }
                    }
                    else
                    {
                        --sp;
                        rRefInList = 0;
                        SetError( errIllegalParameter);
                    }
                }
                break;
            default:
                SetError( errIllegalParameter);
        }
    }
    else
        SetError( errUnknownStackVariable);
}


void ScInterpreter::PopDoubleRef( ScRange& rRange, sal_Bool bDontCheckForTableOp )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PopDoubleRef" );
    if( sp )
    {
        --sp;
        FormulaToken* p = pStack[ sp ];
        switch (p->GetType())
        {
            case svError:
                nGlobalError = p->GetError();
                break;
            case svDoubleRef:
                DoubleRefToRange( static_cast<ScToken*>(p)->GetDoubleRef(), rRange, bDontCheckForTableOp);
                break;
            default:
                SetError( errIllegalParameter);
        }
    }
    else
        SetError( errUnknownStackVariable);
}


sal_Bool ScInterpreter::PopDoubleRefOrSingleRef( ScAddress& rAdr )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PopDoubleRefOrSingleRef" );
    switch ( GetStackType() )
    {
        case svDoubleRef :
        {
            ScRange aRange;
            PopDoubleRef( aRange, sal_True );
            return DoubleRefToPosSingleRef( aRange, rAdr );
        }
        //break;
        case svSingleRef :
        {
            PopSingleRef( rAdr );
            return sal_True;
        }
        //break;
        default:
            PopError();
            SetError( errNoRef );
    }
    return sal_False;
}


void ScInterpreter::PopDoubleRefPushMatrix()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PopDoubleRefPushMatrix" );
    if ( GetStackType() == svDoubleRef )
    {
        ScMatrixRef pMat = GetMatrix();
        if ( pMat )
            PushMatrix( pMat );
        else
            PushIllegalParameter();
    }
    else
        SetError( errNoRef );
}


ScTokenMatrixMap* ScInterpreter::CreateTokenMatrixMap()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::CreateTokenMatrixMap" );
    return new ScTokenMatrixMap;
}


bool ScInterpreter::ConvertMatrixParameters()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::ConvertMatrixParameters" );
    sal_uInt16 nParams = pCur->GetParamCount();
    DBG_ASSERT( nParams <= sp, "ConvertMatrixParameters: stack/param count mismatch");
    SCSIZE nJumpCols = 0, nJumpRows = 0;
    for ( sal_uInt16 i=1; i <= nParams && i <= sp; ++i )
    {
        FormulaToken* p = pStack[ sp - i ];
        if ( p->GetOpCode() != ocPush && p->GetOpCode() != ocMissing  )
        {
            DBG_ERRORFILE( "ConvertMatrixParameters: not a push");
        }
        else
        {
            switch ( p->GetType() )
            {
                case svDouble:
                case svString:
                case svSingleRef:
                case svMissing:
                case svError:
                case svEmptyCell:
                    // nothing to do
                break;
                case svMatrix:
                {
                    if ( ScParameterClassification::GetParameterType( pCur, nParams - i)
                            == ScParameterClassification::Value )
                    {   // only if single value expected
                        ScMatrixRef pMat = static_cast<ScToken*>(p)->GetMatrix();
                        if ( !pMat )
                            SetError( errUnknownVariable);
                        else
                        {
                            SCSIZE nCols, nRows;
                            pMat->GetDimensions( nCols, nRows);
                            if ( nJumpCols < nCols )
                                nJumpCols = nCols;
                            if ( nJumpRows < nRows )
                                nJumpRows = nRows;
                        }
                    }
                }
                break;
                case svDoubleRef:
                {
                    ScParameterClassification::Type eType =
                        ScParameterClassification::GetParameterType( pCur, nParams - i);
                    if ( eType != ScParameterClassification::Reference &&
                            eType != ScParameterClassification::ReferenceOrForceArray)
                    {
                        SCCOL nCol1, nCol2;
                        SCROW nRow1, nRow2;
                        SCTAB nTab1, nTab2;
                        DoubleRefToVars( static_cast<const ScToken*>( p), nCol1, nRow1, nTab1, nCol2, nRow2, nTab2);
                        // Make sure the map exists, created if not.
                        GetTokenMatrixMap();
                        ScMatrixRef pMat = CreateMatrixFromDoubleRef( p,
                                nCol1, nRow1, nTab1, nCol2, nRow2, nTab2);
                        if (pMat)
                        {
                            if ( eType == ScParameterClassification::Value )
                            {   // only if single value expected
                                if ( nJumpCols < static_cast<SCSIZE>(nCol2 - nCol1 + 1) )
                                    nJumpCols = static_cast<SCSIZE>(nCol2 - nCol1 + 1);
                                if ( nJumpRows < static_cast<SCSIZE>(nRow2 - nRow1 + 1) )
                                    nJumpRows = static_cast<SCSIZE>(nRow2 - nRow1 + 1);
                            }
                            ScToken* pNew = new ScMatrixToken( pMat);
                            pNew->IncRef();
                            pStack[ sp - i ] = pNew;
                            p->DecRef();    // p may be dead now!
                        }
                    }
                }
                break;
                case svRefList:
                {
                    ScParameterClassification::Type eType =
                        ScParameterClassification::GetParameterType( pCur, nParams - i);
                    if ( eType != ScParameterClassification::Reference &&
                            eType != ScParameterClassification::ReferenceOrForceArray)
                    {
                        // can't convert to matrix
                        SetError( errNoValue);
                    }
                }
                break;
                default:
                    DBG_ERRORFILE( "ConvertMatrixParameters: unknown parameter type");
            }
        }
    }
    if( nJumpCols && nJumpRows )
    {
        short nPC = aCode.GetPC();
        short nStart = nPC - 1;     // restart on current code (-1)
        short nNext = nPC;          // next instruction after subroutine
        short nStop = nPC + 1;      // stop subroutine before reaching that
        FormulaTokenRef xNew;
        ScTokenMatrixMap::const_iterator aMapIter;
        if (pTokenMatrixMap && ((aMapIter = pTokenMatrixMap->find( pCur)) !=
                    pTokenMatrixMap->end()))
            xNew = (*aMapIter).second;
        else
        {
            ScJumpMatrix* pJumpMat = new ScJumpMatrix( nJumpCols, nJumpRows);
            pJumpMat->SetAllJumps( 1.0, nStart, nNext, nStop);
            // pop parameters and store in ScJumpMatrix, push in JumpMatrix()
            ScTokenVec* pParams = new ScTokenVec( nParams);
            for ( sal_uInt16 i=1; i <= nParams && sp > 0; ++i )
            {
                FormulaToken* p = pStack[ --sp ];
                p->IncRef();
                // store in reverse order such that a push may simply iterate
                (*pParams)[ nParams - i ] = p;
            }
            pJumpMat->SetJumpParameters( pParams);
            xNew = new ScJumpMatrixToken( pJumpMat );
            GetTokenMatrixMap().insert( ScTokenMatrixMap::value_type( pCur,
                        xNew));
        }
        PushTempToken( xNew);
        // set continuation point of path for main code line
        aCode.Jump( nNext, nNext);
        return true;
    }
    return false;
}


ScMatrixRef ScInterpreter::PopMatrix()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PopMatrix" );
    if( sp )
    {
        --sp;
        FormulaToken* p = pStack[ sp ];
        switch (p->GetType())
        {
            case svError:
                nGlobalError = p->GetError();
                break;
            case svMatrix:
                {
                    ScMatrix* pMat = static_cast<ScToken*>(p)->GetMatrix();
                    if ( pMat )
                        pMat->SetErrorInterpreter( this);
                    else
                        SetError( errUnknownVariable);
                    return pMat;
                }
            default:
                SetError( errIllegalParameter);
        }
    }
    else
        SetError( errUnknownStackVariable);
    return NULL;
}


void ScInterpreter::PushDouble(double nVal)
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PushDouble" );
    TreatDoubleError( nVal );
    if (!IfErrorPushError())
        PushTempTokenWithoutError( new FormulaDoubleToken( nVal ) );
}


void ScInterpreter::PushInt(int nVal)
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PushInt" );
    if (!IfErrorPushError())
        PushTempTokenWithoutError( new FormulaDoubleToken( nVal ) );
}


void ScInterpreter::PushStringBuffer( const sal_Unicode* pString )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PushStringBuffer" );
    if ( pString )
        PushString( String( pString ) );
    else
        PushString( EMPTY_STRING );
}


void ScInterpreter::PushString( const String& rString )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PushString" );
    if (!IfErrorPushError())
        PushTempTokenWithoutError( new FormulaStringToken( rString ) );
}


void ScInterpreter::PushSingleRef(SCCOL nCol, SCROW nRow, SCTAB nTab)
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PushSingleRef" );
    if (!IfErrorPushError())
    {
        ScSingleRefData aRef;
        aRef.InitFlags();
        aRef.nCol = nCol;
        aRef.nRow = nRow;
        aRef.nTab = nTab;
        PushTempTokenWithoutError( new ScSingleRefToken( aRef ) );
    }
}


void ScInterpreter::PushDoubleRef(SCCOL nCol1, SCROW nRow1, SCTAB nTab1,
                                  SCCOL nCol2, SCROW nRow2, SCTAB nTab2)
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PushDoubleRef" );
    if (!IfErrorPushError())
    {
        ScComplexRefData aRef;
        aRef.InitFlags();
        aRef.Ref1.nCol = nCol1;
        aRef.Ref1.nRow = nRow1;
        aRef.Ref1.nTab = nTab1;
        aRef.Ref2.nCol = nCol2;
        aRef.Ref2.nRow = nRow2;
        aRef.Ref2.nTab = nTab2;
        PushTempTokenWithoutError( new ScDoubleRefToken( aRef ) );
    }
}


void ScInterpreter::PushMatrix(ScMatrix* pMat)
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PushMatrix" );
    pMat->SetErrorInterpreter( NULL);
    // No   if (!IfErrorPushError())   because ScMatrix stores errors itself,
    // but with notifying ScInterpreter via nGlobalError, substituting it would
    // mean to inherit the error on all array elements in all following
    // operations.
    nGlobalError = 0;
    PushTempTokenWithoutError( new ScMatrixToken( pMat ) );
}


void ScInterpreter::PushError( sal_uInt16 nError )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PushError" );
    SetError( nError );     // only sets error if not already set
    PushTempTokenWithoutError( new FormulaErrorToken( nGlobalError));
}

void ScInterpreter::PushParameterExpected()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PushParameterExpected" );
    PushError( errParameterExpected);
}


void ScInterpreter::PushIllegalParameter()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PushIllegalParameter" );
    PushError( errIllegalParameter);
}


void ScInterpreter::PushIllegalArgument()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PushIllegalArgument" );
    PushError( errIllegalArgument);
}


void ScInterpreter::PushNA()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PushNA" );
    PushError( NOTAVAILABLE);
}


void ScInterpreter::PushNoValue()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::PushNoValue" );
    PushError( errNoValue);
}


sal_Bool ScInterpreter::IsMissing()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::IsMissing" );
    return sp && pStack[sp - 1]->GetType() == svMissing;
}


StackVar ScInterpreter::GetRawStackType()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::GetRawStackType" );
    StackVar eRes;
    if( sp )
    {
        eRes = pStack[sp - 1]->GetType();
    }
    else
    {
        SetError(errUnknownStackVariable);
        eRes = svUnknown;
    }
    return eRes;
}


StackVar ScInterpreter::GetStackType()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::GetStackType" );
    StackVar eRes;
    if( sp )
    {
        eRes = pStack[sp - 1]->GetType();
        if( eRes == svMissing || eRes == svEmptyCell )
            eRes = svDouble;    // default!
    }
    else
    {
        SetError(errUnknownStackVariable);
        eRes = svUnknown;
    }
    return eRes;
}


StackVar ScInterpreter::GetStackType( sal_uInt8 nParam )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::GetStackType" );
    StackVar eRes;
    if( sp > nParam-1 )
    {
        eRes = pStack[sp - nParam]->GetType();
        if( eRes == svMissing || eRes == svEmptyCell )
            eRes = svDouble;    // default!
    }
    else
        eRes = svUnknown;
    return eRes;
}


sal_Bool ScInterpreter::DoubleRefToPosSingleRef( const ScRange& rRange, ScAddress& rAdr )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::DoubleRefToPosSingleRef" );
    // Check for a singleton first - no implicit intersection for them.
    if( rRange.aStart == rRange.aEnd )
    {
        rAdr = rRange.aStart;
        return sal_True;
    }

    sal_Bool bOk = sal_False;

    if ( pJumpMatrix )
    {
        bOk = rRange.aStart.Tab() == rRange.aEnd.Tab();
        if ( !bOk )
            SetError( errIllegalArgument);
        else
        {
            SCSIZE nC, nR;
            pJumpMatrix->GetPos( nC, nR);
            rAdr.SetCol( sal::static_int_cast<SCCOL>( rRange.aStart.Col() + nC ) );
            rAdr.SetRow( sal::static_int_cast<SCROW>( rRange.aStart.Row() + nR ) );
            rAdr.SetTab( rRange.aStart.Tab());
            bOk = rRange.aStart.Col() <= rAdr.Col() && rAdr.Col() <=
                rRange.aEnd.Col() && rRange.aStart.Row() <= rAdr.Row() &&
                rAdr.Row() <= rRange.aEnd.Row();
            if ( !bOk )
                SetError( errNoValue);
        }
        return bOk;
    }

    SCCOL nMyCol = aPos.Col();
    SCROW nMyRow = aPos.Row();
    SCTAB nMyTab = aPos.Tab();
    SCCOL nCol = 0;
    SCROW nRow = 0;
    SCTAB nTab;
    nTab = rRange.aStart.Tab();
    if ( rRange.aStart.Col() <= nMyCol && nMyCol <= rRange.aEnd.Col() )
    {
        nRow = rRange.aStart.Row();
        if ( nRow == rRange.aEnd.Row() )
        {
            bOk = sal_True;
            nCol = nMyCol;
        }
        else if ( nTab != nMyTab && nTab == rRange.aEnd.Tab()
                && rRange.aStart.Row() <= nMyRow && nMyRow <= rRange.aEnd.Row() )
        {
            bOk = sal_True;
            nCol = nMyCol;
            nRow = nMyRow;
        }
    }
    else if ( rRange.aStart.Row() <= nMyRow && nMyRow <= rRange.aEnd.Row() )
    {
        nCol = rRange.aStart.Col();
        if ( nCol == rRange.aEnd.Col() )
        {
            bOk = sal_True;
            nRow = nMyRow;
        }
        else if ( nTab != nMyTab && nTab == rRange.aEnd.Tab()
                && rRange.aStart.Col() <= nMyCol && nMyCol <= rRange.aEnd.Col() )
        {
            bOk = sal_True;
            nCol = nMyCol;
            nRow = nMyRow;
        }
    }
    if ( bOk )
    {
        if ( nTab == rRange.aEnd.Tab() )
            ;   // all done
        else if ( nTab <= nMyTab && nMyTab <= rRange.aEnd.Tab() )
            nTab = nMyTab;
        else
            bOk = sal_False;
        if ( bOk )
            rAdr.Set( nCol, nRow, nTab );
    }
    if ( !bOk )
        SetError( errNoValue );
    return bOk;
}


double ScInterpreter::GetDouble()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::GetDouble" );
    double nVal;
    switch( GetRawStackType() )
    {
        case svDouble:
            nVal = PopDouble();
        break;
        case svString:
            nVal = ConvertStringToValue( PopString());
        break;
        case svSingleRef:
        {
            ScAddress aAdr;
            PopSingleRef( aAdr );
            ScBaseCell* pCell = GetCell( aAdr );
            nVal = GetCellValue( aAdr, pCell );
        }
        break;
        case svDoubleRef:
        {   // generate position dependent SingleRef
            ScRange aRange;
            PopDoubleRef( aRange );
            ScAddress aAdr;
            if ( !nGlobalError && DoubleRefToPosSingleRef( aRange, aAdr ) )
            {
                ScBaseCell* pCell = GetCell( aAdr );
                nVal = GetCellValue( aAdr, pCell );
            }
            else
                nVal = 0.0;
        }
        break;
        case svMatrix:
        {
            ScMatrixRef pMat = PopMatrix();
            if ( !pMat )
                nVal = 0.0;
            else if ( !pJumpMatrix )
                nVal = pMat->GetDouble( 0 );
            else
            {
                SCSIZE nCols, nRows, nC, nR;
                pMat->GetDimensions( nCols, nRows);
                pJumpMatrix->GetPos( nC, nR);
                if ( nC < nCols && nR < nRows )
                    nVal = pMat->GetDouble( nC, nR);
                else
                {
                    SetError( errNoValue);
                    nVal = 0.0;
                }
            }
        }
        break;
        case svError:
            PopError();
            nVal = 0.0;
        break;
        case svEmptyCell:
        case svMissing:
            Pop();
            nVal = 0.0;
        break;
        default:
            PopError();
            SetError( errIllegalParameter);
            nVal = 0.0;
    }
    if ( nFuncFmtType == nCurFmtType )
        nFuncFmtIndex = nCurFmtIndex;
    return nVal;
}


double ScInterpreter::GetDoubleWithDefault(double nDefault)
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::GetDoubleWithDefault" );
    bool bMissing = IsMissing();
    double nResultVal = GetDouble();
    if ( bMissing )
        nResultVal = nDefault;
    return nResultVal;
}


const String& ScInterpreter::GetString()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::GetString" );
    switch (GetRawStackType())
    {
        case svError:
            PopError();
            return EMPTY_STRING;
        //break;
        case svMissing:
        case svEmptyCell:
            Pop();
            return EMPTY_STRING;
        //break;
        case svDouble:
        {
            double fVal = PopDouble();
            sal_uLong nIndex = pFormatter->GetStandardFormat(
                                    NUMBERFORMAT_NUMBER,
                                    ScGlobal::eLnge);
            pFormatter->GetInputLineString(fVal, nIndex, aTempStr);
            return aTempStr;
        }
        //break;
        case svString:
            return PopString();
        //break;
        case svSingleRef:
        {
            ScAddress aAdr;
            PopSingleRef( aAdr );
            if (nGlobalError == 0)
            {
                ScBaseCell* pCell = GetCell( aAdr );
                GetCellString( aTempStr, pCell );
                return aTempStr;
            }
            else
                return EMPTY_STRING;
        }
        //break;
        case svDoubleRef:
        {   // generate position dependent SingleRef
            ScRange aRange;
            PopDoubleRef( aRange );
            ScAddress aAdr;
            if ( !nGlobalError && DoubleRefToPosSingleRef( aRange, aAdr ) )
            {
                ScBaseCell* pCell = GetCell( aAdr );
                GetCellString( aTempStr, pCell );
                return aTempStr;
            }
            else
                return EMPTY_STRING;
        }
        //break;
        case svMatrix:
        {
            ScMatrixRef pMat = PopMatrix();
            if ( !pMat )
                ;   // nothing
            else if ( !pJumpMatrix )
            {
                aTempStr = pMat->GetString( *pFormatter, 0, 0);
                return aTempStr;
            }
            else
            {
                SCSIZE nCols, nRows, nC, nR;
                pMat->GetDimensions( nCols, nRows);
                pJumpMatrix->GetPos( nC, nR);
                if ( nC < nCols && nR < nRows )
                {
                    aTempStr = pMat->GetString( *pFormatter, nC, nR);
                    return aTempStr;
                }
                else
                    SetError( errNoValue);
            }
        }
        break;
        default:
            PopError();
            SetError( errIllegalArgument);
    }
    return EMPTY_STRING;
}



ScMatValType ScInterpreter::GetDoubleOrStringFromMatrix( double& rDouble,
        String& rString )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::GetDoubleOrStringFromMatrix" );
    ScMatValType nMatValType = SC_MATVAL_EMPTY;
    switch ( GetStackType() )
    {
        case svMatrix:
            {
                const ScMatrixValue* pMatVal = 0;
                ScMatrixRef pMat = PopMatrix();
                if (!pMat)
                    ;   // nothing
                else if (!pJumpMatrix)
                    pMatVal = pMat->Get( 0, 0, nMatValType);
                else
                {
                    SCSIZE nCols, nRows, nC, nR;
                    pMat->GetDimensions( nCols, nRows);
                    pJumpMatrix->GetPos( nC, nR);
                    if ( nC < nCols && nR < nRows )
                        pMatVal = pMat->Get( nC, nR, nMatValType);
                    else
                        SetError( errNoValue);
                }
                if (!pMatVal)
                {
                    rDouble = 0.0;
                    rString.Erase();
                }
                else if (nMatValType == SC_MATVAL_VALUE)
                    rDouble = pMatVal->fVal;
                else if (nMatValType == SC_MATVAL_BOOLEAN)
                {
                    rDouble = pMatVal->fVal;
                    nMatValType = SC_MATVAL_VALUE;
                }
                else
                    rString = pMatVal->GetString();
            }
            break;
        default:
            PopError();
            rDouble = 0.0;
            rString.Erase();
            SetError( errIllegalParameter);
    }
    return nMatValType;
}


void ScInterpreter::ScDBGet()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::ScDBGet" );
    sal_Bool bMissingField = sal_False;
    auto_ptr<ScDBQueryParamBase> pQueryParam( GetDBParams(bMissingField) );
    if (!pQueryParam.get())
    {
        // Failed to create query param.
        PushIllegalParameter();
        return;
    }

    pQueryParam->mbSkipString = false;
    ScDBQueryDataIterator aValIter(pDok, pQueryParam.release());
    ScDBQueryDataIterator::Value aValue;
    if (!aValIter.GetFirst(aValue) || aValue.mnError)
    {
        // No match found.
        PushNoValue();
        return;
    }

    ScDBQueryDataIterator::Value aValNext;
    if (aValIter.GetNext(aValNext) && !aValNext.mnError)
    {
        // There should be only one unique match.
        PushIllegalArgument();
        return;
    }

    if (aValue.mbIsNumber)
        PushDouble(aValue.mfValue);
    else
        PushString(aValue.maString);
}


void ScInterpreter::ScExternal()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::ScExternal" );
    sal_uInt16 nIndex;
    sal_uInt8 nParamCount = GetByte();
    String aUnoName;
    String aFuncName( ScGlobal::pCharClass->upper( pCur->GetExternal() ) );
    if (ScGlobal::GetFuncCollection()->SearchFunc(aFuncName, nIndex))
    {
        FuncData* pFuncData = (FuncData*)ScGlobal::GetFuncCollection()->At(nIndex);
        if (nParamCount <= MAXFUNCPARAM && nParamCount == pFuncData->GetParamCount() - 1)
        {
            ParamType   eParamType[MAXFUNCPARAM];
            void*       ppParam[MAXFUNCPARAM];
            double      nVal[MAXFUNCPARAM];
            sal_Char*   pStr[MAXFUNCPARAM];
            sal_uInt8*       pCellArr[MAXFUNCPARAM];
            short       i;

            for (i = 0; i < MAXFUNCPARAM; i++)
            {
                eParamType[i] = pFuncData->GetParamType(i);
                ppParam[i] = NULL;
                nVal[i] = 0.0;
                pStr[i] = NULL;
                pCellArr[i] = NULL;
            }

            for (i = nParamCount; (i > 0) && (nGlobalError == 0); i--)
            {
                switch (eParamType[i])
                {
                    case PTR_DOUBLE :
                        {
                            nVal[i-1] = GetDouble();
                            ppParam[i] = &nVal[i-1];
                        }
                        break;
                    case PTR_STRING :
                        {
                            ByteString aStr( GetString(), osl_getThreadTextEncoding() );
                            if ( aStr.Len() >= ADDIN_MAXSTRLEN )
                                SetError( errStringOverflow );
                            else
                            {
                                pStr[i-1] = new sal_Char[ADDIN_MAXSTRLEN];
                                strncpy( pStr[i-1], aStr.GetBuffer(), ADDIN_MAXSTRLEN );
                                pStr[i-1][ADDIN_MAXSTRLEN-1] = 0;
                                ppParam[i] = pStr[i-1];
                            }
                        }
                        break;
                    case PTR_DOUBLE_ARR :
                        {
                            SCCOL nCol1;
                            SCROW nRow1;
                            SCTAB nTab1;
                            SCCOL nCol2;
                            SCROW nRow2;
                            SCTAB nTab2;
                            PopDoubleRef(nCol1, nRow1, nTab1, nCol2, nRow2, nTab2);
                            pCellArr[i-1] = new sal_uInt8[MAXARRSIZE];
                            if (!CreateDoubleArr(nCol1, nRow1, nTab1, nCol2, nRow2, nTab2, pCellArr[i-1]))
                                SetError(errCodeOverflow);
                            else
                                ppParam[i] = pCellArr[i-1];
                        }
                        break;
                    case PTR_STRING_ARR :
                        {
                            SCCOL nCol1;
                            SCROW nRow1;
                            SCTAB nTab1;
                            SCCOL nCol2;
                            SCROW nRow2;
                            SCTAB nTab2;
                            PopDoubleRef(nCol1, nRow1, nTab1, nCol2, nRow2, nTab2);
                            pCellArr[i-1] = new sal_uInt8[MAXARRSIZE];
                            if (!CreateStringArr(nCol1, nRow1, nTab1, nCol2, nRow2, nTab2, pCellArr[i-1]))
                                SetError(errCodeOverflow);
                            else
                                ppParam[i] = pCellArr[i-1];
                        }
                        break;
                    case PTR_CELL_ARR :
                        {
                            SCCOL nCol1;
                            SCROW nRow1;
                            SCTAB nTab1;
                            SCCOL nCol2;
                            SCROW nRow2;
                            SCTAB nTab2;
                            PopDoubleRef(nCol1, nRow1, nTab1, nCol2, nRow2, nTab2);
                            pCellArr[i-1] = new sal_uInt8[MAXARRSIZE];
                            if (!CreateCellArr(nCol1, nRow1, nTab1, nCol2, nRow2, nTab2, pCellArr[i-1]))
                                SetError(errCodeOverflow);
                            else
                                ppParam[i] = pCellArr[i-1];
                        }
                        break;
                    default :
                        SetError(errIllegalParameter);
                        break;
                }
            }
            while ( i-- )
                Pop();      // im Fehlerfall (sonst ist i==0) Parameter wegpoppen

            if (nGlobalError == 0)
            {
                if ( pFuncData->GetAsyncType() == NONE )
                {
                    switch ( eParamType[0] )
                    {
                        case PTR_DOUBLE :
                        {
                            double nErg = 0.0;
                            ppParam[0] = &nErg;
                            pFuncData->Call(ppParam);
                            PushDouble(nErg);
                        }
                        break;
                        case PTR_STRING :
                        {
                            sal_Char* pcErg = new sal_Char[ADDIN_MAXSTRLEN];
                            ppParam[0] = pcErg;
                            pFuncData->Call(ppParam);
                            String aUni( pcErg, osl_getThreadTextEncoding() );
                            PushString( aUni );
                            delete[] pcErg;
                        }
                        break;
                        default:
                            PushError( errUnknownState );
                    }
                }
                else
                {
                    // nach dem Laden Asyncs wieder anwerfen
                    if ( pMyFormulaCell->GetCode()->IsRecalcModeNormal() )
                        pMyFormulaCell->GetCode()->SetRecalcModeOnLoad();
                    // garantiert identischer Handle bei identischem Aufruf?!?
                    // sonst schei*e ...
                    double nErg = 0.0;
                    ppParam[0] = &nErg;
                    pFuncData->Call(ppParam);
                    sal_uLong nHandle = sal_uLong( nErg );
                    if ( nHandle >= 65536 )
                    {
                        ScAddInAsync* pAs = ScAddInAsync::Get( nHandle );
                        if ( !pAs )
                        {
                            pAs = new ScAddInAsync( nHandle, nIndex, pDok );
                            pMyFormulaCell->StartListening( *pAs );
                        }
                        else
                        {
                            // falls per cut/copy/paste
                            pMyFormulaCell->StartListening( *pAs );
                            // in anderes Dokument?
                            if ( !pAs->HasDocument( pDok ) )
                                pAs->AddDocument( pDok );
                        }
                        if ( pAs->IsValid() )
                        {
                            switch ( pAs->GetType() )
                            {
                                case PTR_DOUBLE :
                                    PushDouble( pAs->GetValue() );
                                    break;
                                case PTR_STRING :
                                    PushString( pAs->GetString() );
                                    break;
                                default:
                                    PushError( errUnknownState );
                            }
                        }
                        else
                            PushNA();
                    }
                    else
                        PushNoValue();
                }
            }

            for (i = 0; i < MAXFUNCPARAM; i++)
            {
                delete[] pStr[i];
                delete[] pCellArr[i];
            }
        }
        else
        {
            while( nParamCount-- > 0)
                Pop();
            PushIllegalParameter();
        }
    }
    else if ( ( aUnoName = ScGlobal::GetAddInCollection()->FindFunction(aFuncName, sal_False) ).Len()  )
    {
        //  bLocalFirst=sal_False in FindFunction, cFunc should be the stored internal name

        ScUnoAddInCall aCall( *ScGlobal::GetAddInCollection(), aUnoName, nParamCount );

        if ( !aCall.ValidParamCount() )
            SetError( errIllegalParameter );

        if ( aCall.NeedsCaller() && !GetError() )
        {
            SfxObjectShell* pShell = pDok->GetDocumentShell();
            if (pShell)
                aCall.SetCallerFromObjectShell( pShell );
            else
            {
                // use temporary model object (without document) to supply options
                aCall.SetCaller( static_cast<beans::XPropertySet*>(
                                    new ScDocOptionsObj( pDok->GetDocOptions() ) ) );
            }
        }

        short nPar = nParamCount;
        while ( nPar > 0 && !GetError() )
        {
            --nPar;     // 0 .. (nParamCount-1)

            ScAddInArgumentType eType = aCall.GetArgType( nPar );
            sal_uInt8 nStackType = sal::static_int_cast<sal_uInt8>( GetStackType() );

            uno::Any aParam;
            switch (eType)
            {
                case SC_ADDINARG_INTEGER:
                    {
                        double fVal = GetDouble();
                        double fInt = (fVal >= 0.0) ? ::rtl::math::approxFloor( fVal ) :
                                                      ::rtl::math::approxCeil( fVal );
                        if ( fInt >= LONG_MIN && fInt <= LONG_MAX )
                            aParam <<= (sal_Int32)fInt;
                        else
                            SetError(errIllegalArgument);
                    }
                    break;

                case SC_ADDINARG_DOUBLE:
                    aParam <<= (double) GetDouble();
                    break;

                case SC_ADDINARG_STRING:
                    aParam <<= rtl::OUString( GetString() );
                    break;

                case SC_ADDINARG_INTEGER_ARRAY:
                    switch( nStackType )
                    {
                        case svDouble:
                        case svString:
                        case svSingleRef:
                            {
                                double fVal = GetDouble();
                                double fInt = (fVal >= 0.0) ? ::rtl::math::approxFloor( fVal ) :
                                                              ::rtl::math::approxCeil( fVal );
                                if ( fInt >= LONG_MIN && fInt <= LONG_MAX )
                                {
                                    sal_Int32 nIntVal = (long)fInt;
                                    uno::Sequence<sal_Int32> aInner( &nIntVal, 1 );
                                    uno::Sequence< uno::Sequence<sal_Int32> > aOuter( &aInner, 1 );
                                    aParam <<= aOuter;
                                }
                                else
                                    SetError(errIllegalArgument);
                            }
                            break;
                        case svDoubleRef:
                            {
                                ScRange aRange;
                                PopDoubleRef( aRange );
                                if (!ScRangeToSequence::FillLongArray( aParam, pDok, aRange ))
                                    SetError(errIllegalParameter);
                            }
                            break;
                        case svMatrix:
                            if (!ScRangeToSequence::FillLongArray( aParam, PopMatrix() ))
                                SetError(errIllegalParameter);
                            break;
                        default:
                            PopError();
                            SetError(errIllegalParameter);
                    }
                    break;

                case SC_ADDINARG_DOUBLE_ARRAY:
                    switch( nStackType )
                    {
                        case svDouble:
                        case svString:
                        case svSingleRef:
                            {
                                double fVal = GetDouble();
                                uno::Sequence<double> aInner( &fVal, 1 );
                                uno::Sequence< uno::Sequence<double> > aOuter( &aInner, 1 );
                                aParam <<= aOuter;
                            }
                            break;
                        case svDoubleRef:
                            {
                                ScRange aRange;
                                PopDoubleRef( aRange );
                                if (!ScRangeToSequence::FillDoubleArray( aParam, pDok, aRange ))
                                    SetError(errIllegalParameter);
                            }
                            break;
                        case svMatrix:
                            if (!ScRangeToSequence::FillDoubleArray( aParam, PopMatrix() ))
                                SetError(errIllegalParameter);
                            break;
                        default:
                            PopError();
                            SetError(errIllegalParameter);
                    }
                    break;

                case SC_ADDINARG_STRING_ARRAY:
                    switch( nStackType )
                    {
                        case svDouble:
                        case svString:
                        case svSingleRef:
                            {
                                rtl::OUString aString = rtl::OUString( GetString() );
                                uno::Sequence<rtl::OUString> aInner( &aString, 1 );
                                uno::Sequence< uno::Sequence<rtl::OUString> > aOuter( &aInner, 1 );
                                aParam <<= aOuter;
                            }
                            break;
                        case svDoubleRef:
                            {
                                ScRange aRange;
                                PopDoubleRef( aRange );
                                if (!ScRangeToSequence::FillStringArray( aParam, pDok, aRange ))
                                    SetError(errIllegalParameter);
                            }
                            break;
                        case svMatrix:
                            if (!ScRangeToSequence::FillStringArray( aParam, PopMatrix(), pFormatter ))
                                SetError(errIllegalParameter);
                            break;
                        default:
                            PopError();
                            SetError(errIllegalParameter);
                    }
                    break;

                case SC_ADDINARG_MIXED_ARRAY:
                    switch( nStackType )
                    {
                        case svDouble:
                        case svString:
                        case svSingleRef:
                            {
                                uno::Any aElem;
                                if ( nStackType == svDouble )
                                    aElem <<= (double) GetDouble();
                                else if ( nStackType == svString )
                                    aElem <<= rtl::OUString( GetString() );
                                else
                                {
                                    ScAddress aAdr;
                                    if ( PopDoubleRefOrSingleRef( aAdr ) )
                                    {
                                        ScBaseCell* pCell = GetCell( aAdr );
                                        if ( pCell && pCell->HasStringData() )
                                        {
                                            String aStr;
                                            GetCellString( aStr, pCell );
                                            aElem <<= rtl::OUString( aStr );
                                        }
                                        else
                                            aElem <<= (double) GetCellValue( aAdr, pCell );
                                    }
                                }
                                uno::Sequence<uno::Any> aInner( &aElem, 1 );
                                uno::Sequence< uno::Sequence<uno::Any> > aOuter( &aInner, 1 );
                                aParam <<= aOuter;
                            }
                            break;
                        case svDoubleRef:
                            {
                                ScRange aRange;
                                PopDoubleRef( aRange );
                                if (!ScRangeToSequence::FillMixedArray( aParam, pDok, aRange ))
                                    SetError(errIllegalParameter);
                            }
                            break;
                        case svMatrix:
                            if (!ScRangeToSequence::FillMixedArray( aParam, PopMatrix() ))
                                SetError(errIllegalParameter);
                            break;
                        default:
                            PopError();
                            SetError(errIllegalParameter);
                    }
                    break;

                case SC_ADDINARG_VALUE_OR_ARRAY:
                    if ( IsMissing() )
                        nStackType = svMissing;
                    switch( nStackType )
                    {
                        case svDouble:
                            aParam <<= (double) GetDouble();
                            break;
                        case svString:
                            aParam <<= rtl::OUString( GetString() );
                            break;
                        case svSingleRef:
                            {
                                ScAddress aAdr;
                                if ( PopDoubleRefOrSingleRef( aAdr ) )
                                {
                                    ScBaseCell* pCell = GetCell( aAdr );
                                    if ( pCell && pCell->HasStringData() )
                                    {
                                        String aStr;
                                        GetCellString( aStr, pCell );
                                        aParam <<= rtl::OUString( aStr );
                                    }
                                    else
                                        aParam <<= (double) GetCellValue( aAdr, pCell );
                                }
                            }
                            break;
                        case svDoubleRef:
                            {
                                ScRange aRange;
                                PopDoubleRef( aRange );
                                if (!ScRangeToSequence::FillMixedArray( aParam, pDok, aRange ))
                                    SetError(errIllegalParameter);
                            }
                            break;
                        case svMatrix:
                            if (!ScRangeToSequence::FillMixedArray( aParam, PopMatrix() ))
                                SetError(errIllegalParameter);
                            break;
                        case svMissing:
                            Pop();
                            aParam.clear();
                            break;
                        default:
                            PopError();
                            SetError(errIllegalParameter);
                    }
                    break;

                case SC_ADDINARG_CELLRANGE:
                    switch( nStackType )
                    {
                        case svSingleRef:
                            {
                                ScAddress aAdr;
                                PopSingleRef( aAdr );
                                ScRange aRange( aAdr );
                                uno::Reference<table::XCellRange> xObj =
                                        ScCellRangeObj::CreateRangeFromDoc( pDok, aRange );
                                if (xObj.is())
                                    aParam <<= xObj;
                                else
                                    SetError(errIllegalParameter);
                            }
                            break;
                        case svDoubleRef:
                            {
                                ScRange aRange;
                                PopDoubleRef( aRange );
                                uno::Reference<table::XCellRange> xObj =
                                        ScCellRangeObj::CreateRangeFromDoc( pDok, aRange );
                                if (xObj.is())
                                    aParam <<= xObj;
                                else
                                    SetError(errIllegalParameter);
                            }
                            break;
                        default:
                            PopError();
                            SetError(errIllegalParameter);
                    }
                    break;

                default:
                    PopError();
                    SetError(errIllegalParameter);
            }
            aCall.SetParam( nPar, aParam );
        }

        while (nPar-- > 0)
            Pop();                  // in case of error, remove remaining args

        if ( !GetError() )
        {
            aCall.ExecuteCall();

            if ( aCall.HasVarRes() )                        // handle async functions
            {
                if ( pMyFormulaCell->GetCode()->IsRecalcModeNormal() )
                    pMyFormulaCell->GetCode()->SetRecalcModeOnLoad();

                uno::Reference<sheet::XVolatileResult> xRes = aCall.GetVarRes();
                ScAddInListener* pLis = ScAddInListener::Get( xRes );
                if ( !pLis )
                {
                    pLis = ScAddInListener::CreateListener( xRes, pDok );
                    pMyFormulaCell->StartListening( *pLis );
                }
                else
                {
                    pMyFormulaCell->StartListening( *pLis );
                    if ( !pLis->HasDocument( pDok ) )
                        pLis->AddDocument( pDok );
                }

                aCall.SetResult( pLis->GetResult() );       // use result from async
            }

            if ( aCall.GetErrCode() )
                PushError( aCall.GetErrCode() );
            else if ( aCall.HasMatrix() )
            {
                ScMatrixRef xMat = aCall.GetMatrix();
                PushMatrix( xMat );
            }
            else if ( aCall.HasString() )
                PushString( aCall.GetString() );
            else
                PushDouble( aCall.GetValue() );
        }
        else                // error...
            PushError( GetError());
    }
    else
    {
        while( nParamCount-- > 0)
            Pop();
        PushError( errNoAddin );
    }
}


void ScInterpreter::ScMissing()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::ScMissing" );
    PushTempToken( new FormulaMissingToken );
}


void ScInterpreter::ScMacro()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::ScMacro" );
    SbxBase::ResetError();

    sal_uInt8 nParamCount = GetByte();
    String aMacro( pCur->GetExternal() );

    SfxObjectShell* pDocSh = pDok->GetDocumentShell();
    if ( !pDocSh || !pDok->CheckMacroWarn() )
    {
        PushNoValue();      // ohne DocShell kein CallBasic
        return;
    }

    //  keine Sicherheitsabfrage mehr vorneweg (nur CheckMacroWarn), das passiert im CallBasic

    //  Wenn das Dok waehrend eines Basic-Calls geladen wurde,
    //  ist das Sbx-Objekt evtl. nicht angelegt (?)
//  pDocSh->GetSbxObject();

    //  Funktion ueber den einfachen Namen suchen,
    //  dann aBasicStr, aMacroStr fuer SfxObjectShell::CallBasic zusammenbauen

    StarBASIC* pRoot = pDocSh->GetBasic();
    SbxVariable* pVar = pRoot->Find( aMacro, SbxCLASS_METHOD );
    if( !pVar || pVar->GetType() == SbxVOID || !pVar->ISA(SbMethod) )
    {
        PushError( errNoMacro );
        return;
    }

    SbMethod* pMethod = (SbMethod*)pVar;
    SbModule* pModule = pMethod->GetModule();
    SbxObject* pObject = pModule->GetParent();
    DBG_ASSERT(pObject->IsA(TYPE(StarBASIC)), "Kein Basic gefunden!");
    String aMacroStr = pObject->GetName();
    aMacroStr += '.';
    aMacroStr += pModule->GetName();
    aMacroStr += '.';
    aMacroStr += pMethod->GetName();
    String aBasicStr;
    if (pObject->GetParent())
        aBasicStr = pObject->GetParent()->GetName();    // Dokumentenbasic
    else
        aBasicStr = SFX_APP()->GetName();               // Applikationsbasic

    //  Parameter-Array zusammenbauen

    SbxArrayRef refPar = new SbxArray;
    sal_Bool bOk = sal_True;
    for( short i = nParamCount; i && bOk ; i-- )
    {
        SbxVariable* pPar = refPar->Get( (sal_uInt16) i );
        sal_uInt8 nStackType = sal::static_int_cast<sal_uInt8>( GetStackType() );
        switch( nStackType )
        {
            case svDouble:
                pPar->PutDouble( GetDouble() );
            break;
            case svString:
                pPar->PutString( GetString() );
            break;
            case svSingleRef:
            {
                ScAddress aAdr;
                PopSingleRef( aAdr );
                bOk = SetSbxVariable( pPar, aAdr );
            }
            break;
            case svDoubleRef:
            {
                SCCOL nCol1;
                SCROW nRow1;
                SCTAB nTab1;
                SCCOL nCol2;
                SCROW nRow2;
                SCTAB nTab2;
                PopDoubleRef( nCol1, nRow1, nTab1, nCol2, nRow2, nTab2 );
                if( nTab1 != nTab2 )
                {
                    SetError( errIllegalParameter );
                    bOk = sal_False;
                }
                else
                {
                    SbxDimArrayRef refArray = new SbxDimArray;
                    refArray->AddDim32( 1, nRow2 - nRow1 + 1 );
                    refArray->AddDim32( 1, nCol2 - nCol1 + 1 );
                    ScAddress aAdr( nCol1, nRow1, nTab1 );
                    for( SCROW nRow = nRow1; bOk && nRow <= nRow2; nRow++ )
                    {
                        aAdr.SetRow( nRow );
                        sal_Int32 nIdx[ 2 ];
                        nIdx[ 0 ] = nRow-nRow1+1;
                        for( SCCOL nCol = nCol1; bOk && nCol <= nCol2; nCol++ )
                        {
                            aAdr.SetCol( nCol );
                            nIdx[ 1 ] = nCol-nCol1+1;
                            SbxVariable* p = refArray->Get32( nIdx );
                            bOk = SetSbxVariable( p, aAdr );
                        }
                    }
                    pPar->PutObject( refArray );
                }
            }
            break;
            case svMatrix:
            {
                ScMatrixRef pMat = PopMatrix();
                SCSIZE nC, nR;
                if (pMat)
                {
                    pMat->GetDimensions(nC, nR);
                    SbxDimArrayRef refArray = new SbxDimArray;
                    refArray->AddDim32( 1, static_cast<sal_Int32>(nR) );
                    refArray->AddDim32( 1, static_cast<sal_Int32>(nC) );
                    for( SCSIZE nMatRow = 0; nMatRow < nR; nMatRow++ )
                    {
                        sal_Int32 nIdx[ 2 ];
                        nIdx[ 0 ] = static_cast<sal_Int32>(nMatRow+1);
                        for( SCSIZE nMatCol = 0; nMatCol < nC; nMatCol++ )
                        {
                            nIdx[ 1 ] = static_cast<sal_Int32>(nMatCol+1);
                            SbxVariable* p = refArray->Get32( nIdx );
                            if (pMat->IsString(nMatCol, nMatRow))
                                p->PutString( pMat->GetString(nMatCol, nMatRow) );
                            else
                                p->PutDouble( pMat->GetDouble(nMatCol, nMatRow));
                        }
                    }
                    pPar->PutObject( refArray );
                }
                else
                    SetError( errIllegalParameter );
            }
            break;
            default:
                SetError( errIllegalParameter );
                bOk = sal_False;
        }
    }
    if( bOk )
    {
        pDok->LockTable( aPos.Tab() );
        SbxVariableRef refRes = new SbxVariable;
        pDok->IncMacroInterpretLevel();
        ErrCode eRet = pDocSh->CallBasic( aMacroStr, aBasicStr, refPar, refRes );
        pDok->DecMacroInterpretLevel();
        pDok->UnlockTable( aPos.Tab() );

        SbxDataType eResType = refRes->GetType();
        if( pVar->GetError() )
            SetError( errNoValue);
        if ( eRet != ERRCODE_NONE )
            PushNoValue();
        else if( eResType >= SbxINTEGER && eResType <= SbxDOUBLE )
            PushDouble( refRes->GetDouble() );
        else if ( eResType & SbxARRAY )
        {
            SbxBase* pElemObj = refRes->GetObject();
            SbxDimArray* pDimArray = PTR_CAST(SbxDimArray,pElemObj);
            short nDim = pDimArray->GetDims();
            if ( 1 <= nDim && nDim <= 2 )
            {
                sal_Int32 nCs, nCe, nRs, nRe;
                SCSIZE nC, nR;
                SCCOL nColIdx;
                SCROW nRowIdx;
                if ( nDim == 1 )
                {   // array( cols )  eine Zeile, mehrere Spalten
                    pDimArray->GetDim32( 1, nCs, nCe );
                    nC = static_cast<SCSIZE>(nCe - nCs + 1);
                    nRs = nRe = 0;
                    nR = 1;
                    nColIdx = 0;
                    nRowIdx = 1;
                }
                else
                {   // array( rows, cols )
                    pDimArray->GetDim32( 1, nRs, nRe );
                    nR = static_cast<SCSIZE>(nRe - nRs + 1);
                    pDimArray->GetDim32( 2, nCs, nCe );
                    nC = static_cast<SCSIZE>(nCe - nCs + 1);
                    nColIdx = 1;
                    nRowIdx = 0;
                }
                ScMatrixRef pMat = GetNewMat( nC, nR);
                if ( pMat )
                {
                    SbxVariable* pV;
                    SbxDataType eType;
                    for ( SCSIZE j=0; j < nR; j++ )
                    {
                        sal_Int32 nIdx[ 2 ];
                        // bei eindimensionalem array( cols ) wird nIdx[1]
                        // von SbxDimArray::Get ignoriert
                        nIdx[ nRowIdx ] = nRs + static_cast<sal_Int32>(j);
                        for ( SCSIZE i=0; i < nC; i++ )
                        {
                            nIdx[ nColIdx ] = nCs + static_cast<sal_Int32>(i);
                            pV = pDimArray->Get32( nIdx );
                            eType = pV->GetType();
                            if ( eType >= SbxINTEGER && eType <= SbxDOUBLE )
                                pMat->PutDouble( pV->GetDouble(), i, j );
                            else
                                pMat->PutString( pV->GetString(), i, j );
                        }
                    }
                    PushMatrix( pMat );
                }
                else
                    PushIllegalArgument();
            }
            else
                PushNoValue();
        }
        else
            PushString( refRes->GetString() );
    }
}


sal_Bool ScInterpreter::SetSbxVariable( SbxVariable* pVar, const ScAddress& rPos )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::SetSbxVariable" );
    sal_Bool bOk = sal_True;
    ScBaseCell* pCell = pDok->GetCell( rPos );
    if (pCell)
    {
        sal_uInt16 nErr;
        double nVal;
        switch( pCell->GetCellType() )
        {
            case CELLTYPE_VALUE :
                nVal = GetValueCellValue( rPos, (ScValueCell*)pCell );
                pVar->PutDouble( nVal );
                break;
            case CELLTYPE_STRING :
            {
                String aVal;
                ((ScStringCell*)pCell)->GetString( aVal );
                pVar->PutString( aVal );
                break;
            }
            case CELLTYPE_EDIT :
            {
                String aVal;
                ((ScEditCell*) pCell)->GetString( aVal );
                pVar->PutString( aVal );
                break;
            }
            case CELLTYPE_FORMULA :
                nErr = ((ScFormulaCell*)pCell)->GetErrCode();
                if( !nErr )
                {
                    if( ((ScFormulaCell*)pCell)->IsValue() )
                    {
                        nVal = ((ScFormulaCell*)pCell)->GetValue();
                        pVar->PutDouble( nVal );
                    }
                    else
                    {
                        String aVal;
                        ((ScFormulaCell*)pCell)->GetString( aVal );
                        pVar->PutString( aVal );
                    }
                }
                else
                    SetError( nErr ), bOk = sal_False;
                break;
            default :
                pVar->PutDouble( 0.0 );
        }
    }
    else
        pVar->PutDouble( 0.0 );
    return bOk;
}


void ScInterpreter::ScTableOp()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::ScTableOp" );
    sal_uInt8 nParamCount = GetByte();
    if (nParamCount != 3 && nParamCount != 5)
    {
        PushIllegalParameter();
        return;
    }
    ScInterpreterTableOpParams* pTableOp = new ScInterpreterTableOpParams;
    if (nParamCount == 5)
    {
        PopSingleRef( pTableOp->aNew2 );
        PopSingleRef( pTableOp->aOld2 );
    }
    PopSingleRef( pTableOp->aNew1 );
    PopSingleRef( pTableOp->aOld1 );
    PopSingleRef( pTableOp->aFormulaPos );

    pTableOp->bValid = sal_True;
    pDok->aTableOpList.Insert( pTableOp );
    pDok->IncInterpreterTableOpLevel();

    sal_Bool bReuseLastParams = (pDok->aLastTableOpParams == *pTableOp);
    if ( bReuseLastParams )
    {
        pTableOp->aNotifiedFormulaPos = pDok->aLastTableOpParams.aNotifiedFormulaPos;
        pTableOp->bRefresh = sal_True;
        for ( ::std::vector< ScAddress >::const_iterator iBroadcast(
                    pTableOp->aNotifiedFormulaPos.begin() );
                iBroadcast != pTableOp->aNotifiedFormulaPos.end();
                ++iBroadcast )
        {   // emulate broadcast and indirectly collect cell pointers
            ScBaseCell* pCell = pDok->GetCell( *iBroadcast );
            if ( pCell && pCell->GetCellType() == CELLTYPE_FORMULA )
                ((ScFormulaCell*)pCell)->SetTableOpDirty();
        }
    }
    else
    {   // broadcast and indirectly collect cell pointers and positions
        pDok->SetTableOpDirty( pTableOp->aOld1 );
        if ( nParamCount == 5 )
            pDok->SetTableOpDirty( pTableOp->aOld2 );
    }
    pTableOp->bCollectNotifications = sal_False;

    ScBaseCell* pFCell = pDok->GetCell( pTableOp->aFormulaPos );
    if ( pFCell && pFCell->GetCellType() == CELLTYPE_FORMULA )
        ((ScFormulaCell*)pFCell)->SetDirtyVar();
    if ( HasCellValueData( pFCell ) )
        PushDouble( GetCellValue( pTableOp->aFormulaPos, pFCell ));
    else
    {
        String aCellString;
        GetCellString( aCellString, pFCell );
        PushString( aCellString );
    }

    pDok->aTableOpList.Remove( pTableOp );
    // set dirty again once more to be able to recalculate original
    for ( ::std::vector< ScFormulaCell* >::const_iterator iBroadcast(
                pTableOp->aNotifiedFormulaCells.begin() );
            iBroadcast != pTableOp->aNotifiedFormulaCells.end();
            ++iBroadcast )
    {
        (*iBroadcast)->SetTableOpDirty();
    }

    // save these params for next incarnation
    if ( !bReuseLastParams )
        pDok->aLastTableOpParams = *pTableOp;

    if ( pFCell && pFCell->GetCellType() == CELLTYPE_FORMULA )
    {
        ((ScFormulaCell*)pFCell)->SetDirtyVar();
        ((ScFormulaCell*)pFCell)->GetErrCode();     // recalculate original
    }

    // Reset all dirty flags so next incarnation does really collect all cell
    // pointers during notifications and not just non-dirty ones, which may
    // happen if a formula cell is used by more than one TableOp block.
    for ( ::std::vector< ScFormulaCell* >::const_iterator iBroadcast2(
                pTableOp->aNotifiedFormulaCells.begin() );
            iBroadcast2 != pTableOp->aNotifiedFormulaCells.end();
            ++iBroadcast2 )
    {
        (*iBroadcast2)->ResetTableOpDirtyVar();
    }
    delete pTableOp;

    pDok->DecInterpreterTableOpLevel();
}


/*

void ScInterpreter::ScErrCell()
{
RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::ScErrCell" );
    double fErrNum = GetDouble();
    PushError((sal_uInt16) fErrNum);
}
*/

void ScInterpreter::ScDBArea()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::ScDBArea" );
    ScDBData* pDBData = pDok->GetDBCollection()->FindIndex( pCur->GetIndex());
    if (pDBData)
    {
        ScComplexRefData aRefData;
        aRefData.InitFlags();
        pDBData->GetArea( (SCTAB&) aRefData.Ref1.nTab,
                          (SCCOL&) aRefData.Ref1.nCol,
                          (SCROW&) aRefData.Ref1.nRow,
                          (SCCOL&) aRefData.Ref2.nCol,
                          (SCROW&) aRefData.Ref2.nRow);
        aRefData.Ref2.nTab    = aRefData.Ref1.nTab;
        aRefData.CalcRelFromAbs( aPos );
        PushTempToken( new ScDoubleRefToken( aRefData ) );
    }
    else
        PushError( errNoName);
}


void ScInterpreter::ScColRowNameAuto()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::ScColRowNameAuto" );
    ScComplexRefData aRefData( static_cast<const ScToken*>(pCur)->GetDoubleRef() );
    aRefData.CalcAbsIfRel( aPos );
    if ( aRefData.Valid() )
    {
        SCsCOL nStartCol;
        SCsROW nStartRow;
        SCsCOL nCol2;
        SCsROW nRow2;
        // evtl. Begrenzung durch definierte ColRowNameRanges merken
        nCol2 = aRefData.Ref2.nCol;
        nRow2 = aRefData.Ref2.nRow;
        // DataArea der ersten Zelle
        nStartCol = aRefData.Ref2.nCol = aRefData.Ref1.nCol;
        nStartRow = aRefData.Ref2.nRow = aRefData.Ref1.nRow;
        aRefData.Ref2.nTab = aRefData.Ref1.nTab;
        pDok->GetDataArea(  (SCTAB&) aRefData.Ref1.nTab,
                            (SCCOL&) aRefData.Ref1.nCol,
                            (SCROW&) aRefData.Ref1.nRow,
                            (SCCOL&) aRefData.Ref2.nCol,
                            (SCROW&) aRefData.Ref2.nRow,
                            sal_True, false );
        // DataArea im Ursprung begrenzen
        aRefData.Ref1.nCol = nStartCol;
        aRefData.Ref1.nRow = nStartRow;

        //! korrespondiert mit ScCompiler::GetToken
        if ( aRefData.Ref1.IsColRel() )
        {   // ColName
            aRefData.Ref2.nCol = nStartCol;
            // evtl. vorherige Begrenzung durch definierte ColRowNameRanges erhalten
            if ( aRefData.Ref2.nRow > nRow2 )
                aRefData.Ref2.nRow = nRow2;
            SCROW nMyRow;
            if ( aPos.Col() == nStartCol
              && nStartRow <= (nMyRow = aPos.Row()) && nMyRow <= aRefData.Ref2.nRow )
            {   // Formel in gleicher Spalte und innerhalb des Range
                if ( nMyRow == nStartRow )
                {   // direkt unter dem Namen den Rest nehmen
                    nStartRow++;
                    if ( nStartRow > MAXROW )
                        nStartRow = MAXROW;
                    aRefData.Ref1.nRow = nStartRow;
                }
                else
                {   // weiter unten vom Namen bis zur Formelzelle
                    aRefData.Ref2.nRow = nMyRow - 1;
                }
            }
        }
        else
        {   // RowName
            aRefData.Ref2.nRow = nStartRow;
            // evtl. vorherige Begrenzung durch definierte ColRowNameRanges erhalten
            if ( aRefData.Ref2.nCol > nCol2 )
                aRefData.Ref2.nCol = nCol2;
            SCCOL nMyCol;
            if ( aPos.Row() == nStartRow
              && nStartCol <= (nMyCol = aPos.Col()) && nMyCol <= aRefData.Ref2.nCol )
            {   // Formel in gleicher Zeile und innerhalb des Range
                if ( nMyCol == nStartCol )
                {   // direkt neben dem Namen den Rest nehmen
                    nStartCol++;
                    if ( nStartCol > MAXCOL )
                        nStartCol = MAXCOL;
                    aRefData.Ref1.nCol = nStartCol;
                }
                else
                {   // weiter rechts vom Namen bis zur Formelzelle
                    aRefData.Ref2.nCol = nMyCol - 1;
                }
            }
        }
        aRefData.CalcRelFromAbs( aPos );
        PushTempToken( new ScDoubleRefToken( aRefData ) );
    }
    else
        PushError( errNoRef );
}

void ScInterpreter::ScExternalRef()
{
    const FormulaToken* pNextOp = aCode.PeekNextOperator();
    if (pNextOp && pNextOp->GetOpCode() == ocOffset)
    {
        // Handled by OFFSET function.
        PushTempToken( *pCur);
        return;
    }

    ScExternalRefManager* pRefMgr = pDok->GetExternalRefManager();
    const String* pFile = pRefMgr->getExternalFileName(pCur->GetIndex());
    if (!pFile)
        PushError(errNoName);

    switch (pCur->GetType())
    {
        case svExternalSingleRef:
        {
            ScSingleRefData aData(static_cast<const ScToken*>(pCur)->GetSingleRef());
            if (aData.IsTabRel())
            {
                DBG_ERROR("ScCompiler::GetToken: external single reference must have an absolute table reference!");
                break;
            }

            aData.CalcAbsIfRel(aPos);
            ScAddress aAddr(aData.nCol, aData.nRow, aData.nTab);
            ScExternalRefCache::CellFormat aFmt;
            ScExternalRefCache::TokenRef xNew = pRefMgr->getSingleRefToken(
                pCur->GetIndex(), pCur->GetString(), aAddr, &aPos, NULL, &aFmt);

            if (!xNew)
                break;

            PushTempToken( *xNew);      // push a clone

            if (aFmt.mbIsSet)
            {
                nFuncFmtType = aFmt.mnType;
                nFuncFmtIndex = aFmt.mnIndex;
            }
            return;
        }
        //break;    // unreachable, prevent compiler warning
        case svExternalDoubleRef:
        {
            ScComplexRefData aData(static_cast<const ScToken*>(pCur)->GetDoubleRef());
            if (aData.Ref1.IsTabRel() || aData.Ref2.IsTabRel())
            {
                DBG_ERROR("ScCompiler::GetToken: external double reference must have an absolute table reference!");
                break;
            }

            aData.CalcAbsIfRel(aPos);
            ScRange aRange(aData.Ref1.nCol, aData.Ref1.nRow, aData.Ref1.nTab,
                           aData.Ref2.nCol, aData.Ref2.nRow, aData.Ref2.nTab);
            ScExternalRefCache::TokenArrayRef xNew = pRefMgr->getDoubleRefTokens(
                pCur->GetIndex(), pCur->GetString(), aRange, &aPos);

            if (!xNew)
                break;

            ScToken* p = static_cast<ScToken*>(xNew->First());
            if (p->GetType() != svMatrix)
                break;

            if (xNew->Next())
            {
                // Can't handle more than one matrix per parameter.
                SetError( errIllegalArgument);
                break;
            }

            PushMatrix(p->GetMatrix());
            return;
        }
        //break;    // unreachable, prevent compiler warning
        default:
            ;
    }
    PushError(errNoRef);
}

// --- internals ------------------------------------------------------------


void ScInterpreter::ScTTT()
{   // Temporaerer Test-Tanz, zum auspropieren von Funktionen etc.
    sal_uInt8 nParamCount = GetByte();
    // do something, nParamCount bei Pops runterzaehlen!

    // Stack aufraeumen
    while ( nParamCount-- > 0)
        Pop();
    PushError(errNoValue);
}

// -------------------------------------------------------------------------


ScInterpreter::ScInterpreter( ScFormulaCell* pCell, ScDocument* pDoc,
        const ScAddress& rPos, ScTokenArray& r ) :
    aCode( r ),
    aPos( rPos ),
    rArr( r ),
    pDok( pDoc ),
    pTokenMatrixMap( NULL ),
    pMyFormulaCell( pCell ),
    pFormatter( pDoc->GetFormatTable() ),
    mnStringNoValueError( errNoValue),
    bCalcAsShown( pDoc->GetDocOptions().IsCalcAsShown() )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::ScTTT" );
//  pStack = new ScToken*[ MAXSTACK ];

    sal_uInt8 cMatFlag = pMyFormulaCell->GetMatrixFlag();
    bMatrixFormula = ( cMatFlag == MM_FORMULA || cMatFlag == MM_FAKE );
    if (!bGlobalStackInUse)
    {
        bGlobalStackInUse = sal_True;
        if (!pGlobalStack)
            pGlobalStack = new ScTokenStack;
        pStackObj = pGlobalStack;
    }
    else
    {
        pStackObj = new ScTokenStack;
    }
    pStack = pStackObj->pPointer;
}

ScInterpreter::~ScInterpreter()
{
//  delete pStack;

    if ( pStackObj == pGlobalStack )
        bGlobalStackInUse = sal_False;
    else
        delete pStackObj;
    if (pTokenMatrixMap)
        delete pTokenMatrixMap;
}


void ScInterpreter::GlobalExit()        // static
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::GlobalExit" );
    DBG_ASSERT(!bGlobalStackInUse, "wer benutzt noch den TokenStack?");
    DELETEZ(pGlobalStack);
}


// A ::std::vector<FormulaTokenRef> is not possible, a push_back() attempts to 
// use a FormulaToken(const FormulaTokenRef&) ctor. Reinvent wheel..
struct FormulaTokenRefPtr
{
    FormulaToken* mp;
    FormulaTokenRefPtr() : mp(0) {}
    FormulaTokenRefPtr( FormulaToken* p ) : mp(p) { if (mp) mp->IncRef(); }
    FormulaTokenRefPtr( const FormulaTokenRefPtr & r ) : mp(r.mp) { if (mp) mp->IncRef(); }
    ~FormulaTokenRefPtr() { if (mp) mp->DecRef(); }
    FormulaTokenRefPtr& operator=( const FormulaTokenRefPtr & r )
    { if (r.mp) r.mp->IncRef(); if (mp) mp->DecRef(); mp = r.mp; return *this; }
};
typedef ::std::vector< FormulaTokenRefPtr > FormulaTokenDtor;


StackVar ScInterpreter::Interpret()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "sc", "er", "ScInterpreter::Interpret" );
    short nRetTypeExpr = NUMBERFORMAT_UNDEFINED;
    sal_uLong nRetIndexExpr = 0;
    sal_uInt16 nErrorFunction = 0;
    sal_uInt16 nErrorFunctionCount = 0;
    sal_uInt16 nStackBase;

    nGlobalError = 0;
    nStackBase = sp = maxsp = 0;
    nRetFmtType = NUMBERFORMAT_UNDEFINED;
    nFuncFmtType = NUMBERFORMAT_UNDEFINED;
    nFuncFmtIndex = nCurFmtIndex = nRetFmtIndex = 0;
    xResult = NULL;
    pJumpMatrix = NULL;
    glSubTotal = sal_False;
    ScTokenMatrixMap::const_iterator aTokenMatrixMapIter;
    ::boost::scoped_ptr< FormulaTokenDtor > pTokenDtor;

    // Once upon a time we used to have FP exceptions on, and there was a
    // Windows printer driver that kept switching off exceptions, so we had to
    // switch them back on again every time. Who knows if there isn't a driver
    // that keeps switching exceptions on, now that we run with exceptions off,
    // so reassure exceptions are really off.
    SAL_MATH_FPEXCEPTIONS_OFF();

    aCode.Reset();
    while( ( pCur = aCode.Next() ) != NULL
            && (!nGlobalError || nErrorFunction <= nErrorFunctionCount) )
    {
        OpCode eOp = pCur->GetOpCode();
        cPar = pCur->GetByte();
        if ( eOp == ocPush )
        {
            // RPN code push without error
            PushWithoutError( (FormulaToken&) *pCur );
        }
        else if (pTokenMatrixMap && !(eOp == ocIf || eOp == ocChose) &&
                ((aTokenMatrixMapIter = pTokenMatrixMap->find( pCur)) !=
                 pTokenMatrixMap->end()) &&
                (*aTokenMatrixMapIter).second->GetType() != svJumpMatrix)
        {
            // Path already calculated, reuse result.
            nStackBase = sp - pCur->GetParamCount();
            if ( nStackBase > sp )
                nStackBase = sp;        // underflow?!?
            sp = nStackBase;
            PushTempToken( (*aTokenMatrixMapIter).second);
        }
        else
        {
            // previous expression determines the current number format
            nCurFmtType = nRetTypeExpr;
            nCurFmtIndex = nRetIndexExpr;
            // default function's format, others are set if needed
            nFuncFmtType = NUMBERFORMAT_NUMBER;
            nFuncFmtIndex = 0;

            if ( eOp == ocIf || eOp == ocChose )
                nStackBase = sp;        // don't mess around with the jumps
            else
            {
                // Convert parameters to matrix if in array/matrix formula and
                // parameters of function indicate doing so. Create JumpMatrix
                // if necessary.
                if ( MatrixParameterConversion() )
                {
                    eOp = ocNone;       // JumpMatrix created
                    nStackBase = sp;
                }
                else
                    nStackBase = sp - pCur->GetParamCount();
            }
            if ( nStackBase > sp )
                nStackBase = sp;        // underflow?!?

            switch( eOp )
            {
                case ocSep:
                case ocClose:           // pushed by the compiler
                case ocMissing          : ScMissing();                  break;
                case ocMacro            : ScMacro();                    break;
                case ocDBArea           : ScDBArea();                   break;
                case ocColRowNameAuto   : ScColRowNameAuto();           break;
// separated    case ocPush             : Push( (ScToken&) *pCur );     break;
                case ocExternalRef      : ScExternalRef();              break;
                case ocIf               : ScIfJump();                   break;
                case ocChose            : ScChoseJump();                break;
                case ocAdd              : ScAdd();                      break;
                case ocSub              : ScSub();                      break;
                case ocMul              : ScMul();                      break;
                case ocDiv              : ScDiv();                      break;
                case ocAmpersand        : ScAmpersand();                break;
                case ocPow              : ScPow();                      break;
                case ocEqual            : ScEqual();                    break;
                case ocNotEqual         : ScNotEqual();                 break;
                case ocLess             : ScLess();                     break;
                case ocGreater          : ScGreater();                  break;
                case ocLessEqual        : ScLessEqual();                break;
                case ocGreaterEqual     : ScGreaterEqual();             break;
                case ocAnd              : ScAnd();                      break;
                case ocOr               : ScOr();                       break;
                case ocIntersect        : ScIntersect();                break;
                case ocRange            : ScRangeFunc();                break;
                case ocUnion            : ScUnionFunc();                break;
                case ocNot              : ScNot();                      break;
                case ocNegSub           :
                case ocNeg              : ScNeg();                      break;
                case ocPercentSign      : ScPercentSign();              break;
                case ocPi               : ScPi();                       break;
//              case ocDefPar           : ScDefPar();                   break;
                case ocRandom           : ScRandom();                   break;
                case ocTrue             : ScTrue();                     break;
                case ocFalse            : ScFalse();                    break;
                case ocGetActDate       : ScGetActDate();               break;
                case ocGetActTime       : ScGetActTime();               break;
                case ocNotAvail         : PushError( NOTAVAILABLE);     break;
                case ocDeg              : ScDeg();                      break;
                case ocRad              : ScRad();                      break;
                case ocSin              : ScSin();                      break;
                case ocCos              : ScCos();                      break;
                case ocTan              : ScTan();                      break;
                case ocCot              : ScCot();                      break;
                case ocArcSin           : ScArcSin();                   break;
                case ocArcCos           : ScArcCos();                   break;
                case ocArcTan           : ScArcTan();                   break;
                case ocArcCot           : ScArcCot();                   break;
                case ocSinHyp           : ScSinHyp();                   break;
                case ocCosHyp           : ScCosHyp();                   break;
                case ocTanHyp           : ScTanHyp();                   break;
                case ocCotHyp           : ScCotHyp();                   break;
                case ocArcSinHyp        : ScArcSinHyp();                break;
                case ocArcCosHyp        : ScArcCosHyp();                break;
                case ocArcTanHyp        : ScArcTanHyp();                break;
                case ocArcCotHyp        : ScArcCotHyp();                break;
                case ocCosecant         : ScCosecant();                 break;
                case ocSecant           : ScSecant();                   break;
                case ocCosecantHyp      : ScCosecantHyp();              break;
                case ocSecantHyp        : ScSecantHyp();                break;
                case ocExp              : ScExp();                      break;
                case ocLn               : ScLn();                       break;
                case ocLog10            : ScLog10();                    break;
                case ocSqrt             : ScSqrt();                     break;
                case ocFact             : ScFact();                     break;
                case ocGetYear          : ScGetYear();                  break;
                case ocGetMonth         : ScGetMonth();                 break;
                case ocGetDay           : ScGetDay();                   break;
                case ocGetDayOfWeek     : ScGetDayOfWeek();             break;
                case ocWeek             : ScGetWeekOfYear();            break;
                case ocEasterSunday     : ScEasterSunday();             break;
                case ocGetHour          : ScGetHour();                  break;
                case ocGetMin           : ScGetMin();                   break;
                case ocGetSec           : ScGetSec();                   break;
                case ocPlusMinus        : ScPlusMinus();                break;
                case ocAbs              : ScAbs();                      break;
                case ocInt              : ScInt();                      break;
                case ocEven             : ScEven();                     break;
                case ocOdd              : ScOdd();                      break;
                case ocPhi              : ScPhi();                      break;
                case ocGauss            : ScGauss();                    break;
                case ocStdNormDist      : ScStdNormDist();              break;
                case ocFisher           : ScFisher();                   break;
                case ocFisherInv        : ScFisherInv();                break;
                case ocIsEmpty          : ScIsEmpty();                  break;
                case ocIsString         : ScIsString();                 break;
                case ocIsNonString      : ScIsNonString();              break;
                case ocIsLogical        : ScIsLogical();                break;
                case ocType             : ScType();                     break;
                case ocCell             : ScCell();                     break;
                case ocIsRef            : ScIsRef();                    break;
                case ocIsValue          : ScIsValue();                  break;
                case ocIsFormula        : ScIsFormula();                break;
                case ocFormula          : ScFormula();                  break;
                case ocIsNA             : ScIsNV();                     break;
                case ocIsErr            : ScIsErr();                    break;
                case ocIsError          : ScIsError();                  break;
                case ocIsEven           : ScIsEven();                   break;
                case ocIsOdd            : ScIsOdd();                    break;
                case ocN                : ScN();                        break;
                case ocGetDateValue     : ScGetDateValue();             break;
                case ocGetTimeValue     : ScGetTimeValue();             break;
                case ocCode             : ScCode();                     break;
                case ocTrim             : ScTrim();                     break;
                case ocUpper            : ScUpper();                    break;
                case ocPropper          : ScPropper();                  break;
                case ocLower            : ScLower();                    break;
                case ocLen              : ScLen();                      break;
                case ocT                : ScT();                        break;
                case ocClean            : ScClean();                    break;
                case ocValue            : ScValue();                    break;
                case ocChar             : ScChar();                     break;
                case ocArcTan2          : ScArcTan2();                  break;
                case ocMod              : ScMod();                      break;
                case ocPower            : ScPower();                    break;
                case ocRound            : ScRound();                    break;
                case ocRoundUp          : ScRoundUp();                  break;
                case ocTrunc            :
                case ocRoundDown        : ScRoundDown();                break;
                case ocCeil             : ScCeil();                     break;
                case ocFloor            : ScFloor();                    break;
                case ocSumProduct       : ScSumProduct();               break;
                case ocSumSQ            : ScSumSQ();                    break;
                case ocSumX2MY2         : ScSumX2MY2();                 break;
                case ocSumX2DY2         : ScSumX2DY2();                 break;
                case ocSumXMY2          : ScSumXMY2();                  break;
                case ocLog              : ScLog();                      break;
                case ocGCD              : ScGCD();                      break;
                case ocLCM              : ScLCM();                      break;
                case ocGetDate          : ScGetDate();                  break;
                case ocGetTime          : ScGetTime();                  break;
                case ocGetDiffDate      : ScGetDiffDate();              break;
                case ocGetDiffDate360   : ScGetDiffDate360();           break;
                case ocMin              : ScMin( sal_False );               break;
                case ocMinA             : ScMin( sal_True );                break;
                case ocMax              : ScMax( sal_False );               break;
                case ocMaxA             : ScMax( sal_True );                break;
                case ocSum              : ScSum();                      break;
                case ocProduct          : ScProduct();                  break;
                case ocNPV              : ScNPV();                      break;
                case ocIRR              : ScIRR();                      break;
                case ocMIRR             : ScMIRR();                     break;
                case ocISPMT            : ScISPMT();                    break;
                case ocAverage          : ScAverage( sal_False );           break;
                case ocAverageA         : ScAverage( sal_True );            break;
                case ocCount            : ScCount();                    break;
                case ocCount2           : ScCount2();                   break;
                case ocVar              : ScVar( sal_False );               break;
                case ocVarA             : ScVar( sal_True );                break;
                case ocVarP             : ScVarP( sal_False );              break;
                case ocVarPA            : ScVarP( sal_True );               break;
                case ocStDev            : ScStDev( sal_False );             break;
                case ocStDevA           : ScStDev( sal_True );              break;
                case ocStDevP           : ScStDevP( sal_False );            break;
                case ocStDevPA          : ScStDevP( sal_True );             break;
                case ocBW               : ScBW();                       break;
                case ocDIA              : ScDIA();                      break;
                case ocGDA              : ScGDA();                      break;
                case ocGDA2             : ScGDA2();                     break;
                case ocVBD              : ScVDB();                      break;
                case ocLaufz            : ScLaufz();                    break;
                case ocLIA              : ScLIA();                      break;
                case ocRMZ              : ScRMZ();                      break;
                case ocColumns          : ScColumns();                  break;
                case ocRows             : ScRows();                     break;
                case ocTables           : ScTables();                   break;
                case ocColumn           : ScColumn();                   break;
                case ocRow              : ScRow();                      break;
                case ocTable            : ScTable();                    break;
                case ocZGZ              : ScZGZ();                      break;
                case ocZW               : ScZW();                       break;
                case ocZZR              : ScZZR();                      break;
                case ocZins             : ScZins();                     break;
                case ocZinsZ            : ScZinsZ();                    break;
                case ocKapz             : ScKapz();                     break;
                case ocKumZinsZ         : ScKumZinsZ();                 break;
                case ocKumKapZ          : ScKumKapZ();                  break;
                case ocEffektiv         : ScEffektiv();                 break;
                case ocNominal          : ScNominal();                  break;
                case ocSubTotal         : ScSubTotal();                 break;
                case ocDBSum            : ScDBSum();                    break;
                case ocDBCount          : ScDBCount();                  break;
                case ocDBCount2         : ScDBCount2();                 break;
                case ocDBAverage        : ScDBAverage();                break;
                case ocDBGet            : ScDBGet();                    break;
                case ocDBMax            : ScDBMax();                    break;
                case ocDBMin            : ScDBMin();                    break;
                case ocDBProduct        : ScDBProduct();                break;
                case ocDBStdDev         : ScDBStdDev();                 break;
                case ocDBStdDevP        : ScDBStdDevP();                break;
                case ocDBVar            : ScDBVar();                    break;
                case ocDBVarP           : ScDBVarP();                   break;
                case ocIndirect         : ScIndirect();                 break;
                case ocAddress          : ScAddressFunc();              break;
                case ocMatch            : ScMatch();                    break;
                case ocCountEmptyCells  : ScCountEmptyCells();          break;
                case ocCountIf          : ScCountIf();                  break;
                case ocSumIf            : ScSumIf();                    break;
                case ocAverageIf        : ScAverageIf();                break;
                case ocLookup           : ScLookup();                   break;
                case ocVLookup          : ScVLookup();                  break;
                case ocHLookup          : ScHLookup();                  break;
                case ocIndex            : ScIndex();                    break;
                case ocMultiArea        : ScMultiArea();                break;
                case ocOffset           : ScOffset();                   break;
                case ocAreas            : ScAreas();                    break;
                case ocCurrency         : ScCurrency();                 break;
                case ocReplace          : ScReplace();                  break;
                case ocFixed            : ScFixed();                    break;
                case ocFind             : ScFind();                     break;
                case ocExact            : ScExact();                    break;
                case ocLeft             : ScLeft();                     break;
                case ocRight            : ScRight();                    break;
                case ocSearch           : ScSearch();                   break;
                case ocMid              : ScMid();                      break;
                case ocText             : ScText();                     break;
                case ocSubstitute       : ScSubstitute();               break;
                case ocRept             : ScRept();                     break;
                case ocConcat           : ScConcat();                   break;
                case ocMatValue         : ScMatValue();                 break;
                case ocMatrixUnit       : ScEMat();                     break;
                case ocMatDet           : ScMatDet();                   break;
                case ocMatInv           : ScMatInv();                   break;
                case ocMatMult          : ScMatMult();                  break;
                case ocMatTrans         : ScMatTrans();                 break;
                case ocMatRef           : ScMatRef();                   break;
                case ocBackSolver       : ScBackSolver();               break;
                case ocB                : ScB();                        break;
                case ocNormDist         : ScNormDist();                 break;
                case ocExpDist          : ScExpDist();                  break;
                case ocBinomDist        : ScBinomDist();                break;
                case ocPoissonDist      : ScPoissonDist();              break;
                case ocKombin           : ScKombin();                   break;
                case ocKombin2          : ScKombin2();                  break;
                case ocVariationen      : ScVariationen();              break;
                case ocVariationen2     : ScVariationen2();             break;
                case ocHypGeomDist      : ScHypGeomDist();              break;
                case ocLogNormDist      : ScLogNormDist();              break;
                case ocTDist            : ScTDist();                    break;
                case ocFDist            : ScFDist();                    break;
                case ocChiDist          : ScChiDist();                  break;
                case ocChiSqDist        : ScChiSqDist();                break;
                case ocStandard         : ScStandard();                 break;
                case ocAveDev           : ScAveDev();                   break;
                case ocDevSq            : ScDevSq();                    break;
                case ocKurt             : ScKurt();                     break;
                case ocSchiefe          : ScSkew();                     break;
                case ocModalValue       : ScModalValue();               break;
                case ocMedian           : ScMedian();                   break;
                case ocGeoMean          : ScGeoMean();                  break;
                case ocHarMean          : ScHarMean();                  break;
                case ocWeibull          : ScWeibull();                  break;
                case ocKritBinom        : ScCritBinom();                break;
                case ocNegBinomVert     : ScNegBinomDist();             break;
                case ocNoName           : ScNoName();                   break;
                case ocBad              : ScBadName();                  break;
                case ocZTest            : ScZTest();                    break;
                case ocTTest            : ScTTest();                    break;
                case ocFTest            : ScFTest();                    break;
                case ocRank             : ScRank();                     break;
                case ocPercentile       : ScPercentile();               break;
                case ocPercentrank      : ScPercentrank();              break;
                case ocLarge            : ScLarge();                    break;
                case ocSmall            : ScSmall();                    break;
                case ocFrequency        : ScFrequency();                break;
                case ocQuartile         : ScQuartile();                 break;
                case ocNormInv          : ScNormInv();                  break;
                case ocSNormInv         : ScSNormInv();                 break;
                case ocConfidence       : ScConfidence();               break;
                case ocTrimMean         : ScTrimMean();                 break;
                case ocProb             : ScProbability();              break;
                case ocCorrel           : ScCorrel();                   break;
                case ocCovar            : ScCovar();                    break;
                case ocPearson          : ScPearson();                  break;
                case ocRSQ              : ScRSQ();                      break;
                case ocSTEYX            : ScSTEXY();                    break;
                case ocSlope            : ScSlope();                    break;
                case ocIntercept        : ScIntercept();                break;
                case ocTrend            : ScTrend();                    break;
                case ocGrowth           : ScGrowth();                   break;
                case ocRGP              : ScRGP();                      break;
                case ocRKP              : ScRKP();                      break;
                case ocForecast         : ScForecast();                 break;
                case ocGammaLn          : ScLogGamma();                 break;
                case ocGamma            : ScGamma();                    break;
                case ocGammaDist        : ScGammaDist();                break;
                case ocGammaInv         : ScGammaInv();                 break;
                case ocChiTest          : ScChiTest();                  break;
                case ocChiInv           : ScChiInv();                   break;
                case ocChiSqInv         : ScChiSqInv();                 break;
                case ocTInv             : ScTInv();                     break;
                case ocFInv             : ScFInv();                     break;
                case ocLogInv           : ScLogNormInv();               break;
                case ocBetaDist         : ScBetaDist();                 break;
                case ocBetaInv          : ScBetaInv();                  break;
                case ocExternal         : ScExternal();                 break;
                case ocTableOp          : ScTableOp();                  break;
//              case ocErrCell          : ScErrCell();                  break;
                case ocStop :                                           break;
                case ocErrorType        : ScErrorType();                break;
                case ocCurrent          : ScCurrent();                  break;
                case ocStyle            : ScStyle();                    break;
                case ocDde              : ScDde();                      break;
                case ocBase             : ScBase();                     break;
                case ocDecimal          : ScDecimal();                  break;
                case ocConvert          : ScConvert();                  break;
                case ocEuroConvert      : ScEuroConvert();              break;
                case ocRoman            : ScRoman();                    break;
                case ocArabic           : ScArabic();                   break;
                case ocInfo             : ScInfo();                     break;
                case ocHyperLink        : ScHyperLink();                break;
                case ocBahtText         : ScBahtText();                 break;
                case ocGetPivotData     : ScGetPivotData();             break;
                case ocJis              : ScJis();                      break;
                case ocAsc              : ScAsc();                      break;
                case ocUnicode          : ScUnicode();                  break;
                case ocUnichar          : ScUnichar();                  break;
                case ocTTT              : ScTTT();                      break;
                case ocNone : nFuncFmtType = NUMBERFORMAT_UNDEFINED;    break;
                default : PushError( errUnknownOpCode);                 break;
            }

            // If the function pushed a subroutine as result, continue with  
            // execution of the subroutine.
            if (sp > nStackBase && pStack[sp-1]->GetOpCode() == ocCall && pStack[sp-1]->GetType() == svSubroutine)
            {
                FormulaTokenRef xTok = PopToken();
                const FormulaSubroutineToken* pSub = dynamic_cast<FormulaSubroutineToken*>(xTok.get());
                if (pSub)
                {
                    // Remember token for late destruction.
                    if (!pTokenDtor)
                        pTokenDtor.reset( new FormulaTokenDtor);
                    pTokenDtor->push_back( FormulaTokenDtor::value_type( xTok));
                    // Continue with execution of subroutine.
                    aCode.Push( pSub->GetTokenArray());
                    continue;   // while( ( pCur = aCode.Next() ) != NULL  ...
                }
                else
                {
                    DBG_ERRORFILE( "ScInterpreter::Interpret: ocCall svSubroutine, but no FormulaSubroutineToken?!?");
                    PushError( errNoCode);
                }
            }

            // Remember result matrix in case it could be reused.
            if (pTokenMatrixMap && sp && GetStackType() == svMatrix)
                pTokenMatrixMap->insert( ScTokenMatrixMap::value_type( pCur,
                            pStack[sp-1]));

            // outer function determines format of an expression
            if ( nFuncFmtType != NUMBERFORMAT_UNDEFINED )
            {
                nRetTypeExpr = nFuncFmtType;
                // inherit the format index only for currency formats
                nRetIndexExpr = ( nFuncFmtType == NUMBERFORMAT_CURRENCY ?
                    nFuncFmtIndex : 0 );
            }
        }

        // Need a clean stack environment for the JumpMatrix to work.
        if (nGlobalError && eOp != ocPush && sp > nStackBase + 1)
        {
            // Not all functions pop all parameters in case an error is
            // generated. Clean up stack. Assumes that every function pushes a
            // result, may be arbitrary in case of error.
            const FormulaToken* pLocalResult = pStack[ sp - 1 ];
            while (sp > nStackBase)
                Pop();
            PushTempToken( *pLocalResult );
        }

        bool bGotResult;
        do
        {
            bGotResult = false;
            sal_uInt8 nLevel = 0;
            if ( GetStackType( ++nLevel ) == svJumpMatrix )
                ;   // nothing
            else if ( GetStackType( ++nLevel ) == svJumpMatrix )
                ;   // nothing
            else
                nLevel = 0;
            if ( nLevel == 1 || (nLevel == 2 && aCode.IsEndOfPath()) )
                bGotResult = JumpMatrix( nLevel );
            else
                pJumpMatrix = NULL;
        } while ( bGotResult );


// Functions that evaluate an error code and directly set nGlobalError to 0,
// usage: switch( OpCode ) { CASE_OCERRFUNC statements; }
#define CASE_OCERRFUNC \
    case ocCount : \
    case ocCount2 : \
    case ocErrorType : \
    case ocIsEmpty : \
    case ocIsErr : \
    case ocIsError : \
    case ocIsFormula : \
    case ocIsLogical : \
    case ocIsNA : \
    case ocIsNonString : \
    case ocIsRef : \
    case ocIsString : \
    case ocIsValue : \
    case ocN : \
    case ocType :

        switch ( eOp )
        {
            CASE_OCERRFUNC
                 ++ nErrorFunction;
            default:
                ;   // nothing
        }
        if ( nGlobalError )
        {
            if ( !nErrorFunctionCount )
            {   // count of errorcode functions in formula
                for ( FormulaToken* t = rArr.FirstRPN(); t; t = rArr.NextRPN() )
                {
                    switch ( t->GetOpCode() )
                    {
                        CASE_OCERRFUNC
                             ++nErrorFunctionCount;
                        default:
                            ;   // nothing
                    }
                }
            }
            if ( nErrorFunction >= nErrorFunctionCount )
                ++nErrorFunction;   // that's it, error => terminate
        }
    }

    // End: obtain result

    if( sp )
    {
        pCur = pStack[ sp-1 ];
        if( pCur->GetOpCode() == ocPush )
        {
            switch( pCur->GetType() )
            {
                case svEmptyCell:
                    ;   // nothing
                break;
                case svError:
                    nGlobalError = pCur->GetError();
                break;
                case svDouble :
                    if ( nFuncFmtType == NUMBERFORMAT_UNDEFINED )
                    {
                        nRetTypeExpr = NUMBERFORMAT_NUMBER;
                        nRetIndexExpr = 0;
                    }
                break;
                case svString :
                    nRetTypeExpr = NUMBERFORMAT_TEXT;
                    nRetIndexExpr = 0;
                break;
                case svSingleRef :
                {
                    ScAddress aAdr;
                    PopSingleRef( aAdr );
                    if( !nGlobalError )
                        PushCellResultToken( false, aAdr,
                                &nRetTypeExpr, &nRetIndexExpr);
                }
                break;
                case svRefList :
                    PopError();     // maybe #REF! takes precedence over #VALUE!
                    PushError( errNoValue);
                break;
                case svDoubleRef :
                {
                    if ( bMatrixFormula )
                    {   // create matrix for {=A1:A5}
                        PopDoubleRefPushMatrix();
                        // no break, continue with svMatrix
                    }
                    else
                    {
                        ScRange aRange;
                        PopDoubleRef( aRange );
                        ScAddress aAdr;
                        if ( !nGlobalError && DoubleRefToPosSingleRef( aRange, aAdr))
                            PushCellResultToken( false, aAdr,
                                    &nRetTypeExpr, &nRetIndexExpr);
                        break;
                    }
                }
                // no break
                case svMatrix :
                {
                    ScMatrixRef xMat = PopMatrix();
                    if (xMat)
                    {
                        ScMatValType nMatValType;
                        const ScMatrixValue* pMatVal = xMat->Get(0, 0, nMatValType);
                        if ( pMatVal )
                        {
                            if (ScMatrix::IsNonValueType( nMatValType))
                            {
                                if ( xMat->IsEmptyPath( 0, 0))
                                {   // result of empty sal_False jump path
                                    FormulaTokenRef xRes = new FormulaDoubleToken( 0.0);
                                    PushTempToken( new ScMatrixCellResultToken( xMat, xRes));
                                    nRetTypeExpr = NUMBERFORMAT_LOGICAL;
                                }
                                else
                                {
                                    String aStr( pMatVal->GetString());
                                    FormulaTokenRef xRes = new FormulaStringToken( aStr);
                                    PushTempToken( new ScMatrixCellResultToken( xMat, xRes));
                                    nRetTypeExpr = NUMBERFORMAT_TEXT;
                                }
                            }
                            else
                            {
                                sal_uInt16 nErr = GetDoubleErrorValue( pMatVal->fVal);
                                FormulaTokenRef xRes;
                                if (nErr)
                                    xRes = new FormulaErrorToken( nErr);
                                else
                                    xRes = new FormulaDoubleToken( pMatVal->fVal);
                                PushTempToken( new ScMatrixCellResultToken( xMat, xRes));
                                if ( nRetTypeExpr != NUMBERFORMAT_LOGICAL )
                                    nRetTypeExpr = NUMBERFORMAT_NUMBER;
                            }
                            nRetIndexExpr = 0;
                        }
                        else
                            SetError( errUnknownStackVariable);
                        xMat->SetErrorInterpreter( NULL);
                    }
                    else
                        SetError( errUnknownStackVariable);
                }
                break;
                default :
                    SetError( errUnknownStackVariable);
            }
        }
        else
            SetError( errUnknownStackVariable);
    }
    else
        SetError( errNoCode);

    if( nRetTypeExpr != NUMBERFORMAT_UNDEFINED )
    {
        nRetFmtType = nRetTypeExpr;
        nRetFmtIndex = nRetIndexExpr;
    }
    else if( nFuncFmtType != NUMBERFORMAT_UNDEFINED )
    {
        nRetFmtType = nFuncFmtType;
        nRetFmtIndex = nFuncFmtIndex;
    }
    else
        nRetFmtType = NUMBERFORMAT_NUMBER;
    // inherit the format index only for currency formats
    if ( nRetFmtType != NUMBERFORMAT_CURRENCY )
        nRetFmtIndex = 0;

    if (nGlobalError && GetStackType() != svError )
        PushError( nGlobalError);

    // THE final result.
    xResult = PopToken();
    if (!xResult)
        xResult = new FormulaErrorToken( errUnknownStackVariable);

    // release tokens in expression stack
    FormulaToken** p = pStack;
    while( maxsp-- )
        (*p++)->DecRef();

    StackVar eType = xResult->GetType();
    if (eType == svMatrix)
        // Results are immutable in case they would be reused as input for new 
        // interpreters.
        static_cast<ScToken*>(xResult.operator->())->GetMatrix()->SetImmutable( true);
    return eType;
}
