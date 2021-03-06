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



#if 0
#define _SV_IMPPRN_HXX

#include <vcl/print.hxx>
#include <vcl/timer.hxx>
#ifndef _VCL_IMPDEL_HXX
#include <vcl/impdel.hxx>
#endif

#include <vector>

struct QueuePage;

// ----------------
// - ImplQPrinter -
// ----------------

/*
    ImplQPrinter is on most systems a simple buffer that allows a potential
    lengthy print job to be printed in the background. For this it saves all
    normal drawing operations for each printed page to a metafile, then spooling
    the metafiles timer based to a normal printer. The application can act in the meantime
    including changing the original document without influencing the print job.
    
    On some systems (currently Mac/Aqua Cocoa) ImplQPrinter has the additional
    purpose of adapting to the print system: here theprint systems starts a
    job and will not return from that function until it has ended; to do so
    it queries for each consecutive page to be printed. Also the Cocoa print system
    needs to know the number of pages BEFORE starting a print job. Since our Printer
    does not know that, we need to do the completing spooling to ImplQPrinter before
    we can actually print to the real print system. Let's call this the pull model
    instead of the push model (because the systems pulls the pages).
*/

class ImplQPrinter : public Printer, public vcl::DeletionNotifier
{
private:
	Printer*                        mpParent;
	std::vector< QueuePage* >       maQueue;
	AutoTimer                       maTimer;
	bool                            mbAborted;
	bool                            mbUserCopy;
	bool                            mbDestroyAllowed;
	bool                            mbDestroyed;
    
    GDIMetaFile                     maCurPageMetaFile;
    long                            mnMaxBmpDPIX;
    long                            mnMaxBmpDPIY;
    sal_uLong                           mnRestoreDrawMode;
    int                             mnCurCopyCount;

				DECL_LINK( ImplPrintHdl, Timer* );
	
				~ImplQPrinter();

	void		ImplPrintMtf( GDIMetaFile& rMtf, long nMaxBmpDPIX, long nMaxBmpDPIY );

                ImplQPrinter( const ImplQPrinter& rPrinter );
    Printer&    operator =( const ImplQPrinter& rPrinter );
    
    void        PrePrintPage( QueuePage* );
    void        PostPrintPage();

public:

				ImplQPrinter( Printer* pParent );
	void		Destroy();

	void		StartQueuePrint();
	void		EndQueuePrint();
	void		AbortQueuePrint();
	void		AddQueuePage( GDIMetaFile* pPage, sal_uInt16 nPage, sal_Bool bNewJobSetup );

	bool		IsUserCopy() const { return mbUserCopy; }
	void		SetUserCopy( bool bSet ) { mbUserCopy = bSet; }
    
    /**
    used by pull implementation to emit the next page
    */
    void        PrintPage( unsigned int nPage );
    /**
    used by pull implementation to get the number of physical pages
    (that is how often PrintNextPage should be called)
    */
    sal_uLong       GetPrintPageCount() const;
    
    /**
    used by pull implementation to get ranges of physical pages that
    are to be printed on the same paper. If bIncludeOrientationChanges is true
    then orientation changes will not break a page run; the implementation has
    to rotate the page contents accordingly in that case.
    
    The returned vector contains all pages indices beginning a new medium and additionally
    the index that of the last page+1 (for convenience, so the length of a range
    is always v[i+1] - v[i])
    
    Example: 5 pages, all A4
    return: [0 5]
    
    Example: 6 pages, beginning A4, switching tol A5 on fourth page, back to A4 on fifth page
    return [0 3 4 6]
    
    returns an false in push model (error condition)
    */
    bool GetPaperRanges( std::vector< sal_uLong >& o_rRanges, bool i_bIncludeOrientationChanges ) const;
    
    /**
    get the jobsetup for a page
    */
    ImplJobSetup* GetPageSetup( unsigned int nPage ) const;
};

#endif	// _SV_IMPPRN_HXX
