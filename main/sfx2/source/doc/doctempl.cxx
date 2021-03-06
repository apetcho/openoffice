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
#include "precompiled_sfx2.hxx"

#include <limits.h>
#include <com/sun/star/uno/Any.h>
#include <vos/mutex.hxx>
#include <vos/thread.hxx>

#ifndef _SV_RESARY_HXX
#include <tools/resary.hxx>
#endif
#include <vcl/svapp.hxx>
#include <vcl/settings.hxx>
#include <unotools/localedatawrapper.hxx>
#include <unotools/pathoptions.hxx>
#include <tools/string.hxx>
#include <tools/urlobj.hxx>
#include <svtools/ehdl.hxx>
#include <svtools/sfxecode.hxx>
#include <comphelper/processfactory.hxx>
#include <ucbhelper/content.hxx>
#include <com/sun/star/beans/PropertyAttribute.hpp>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <com/sun/star/beans/XPropertyContainer.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/beans/XPropertySetInfo.hpp>
#include <com/sun/star/document/XTypeDetection.hpp>
#include <com/sun/star/document/XDocumentPropertiesSupplier.hpp>
#include <com/sun/star/frame/XComponentLoader.hpp>
#include <com/sun/star/frame/XDocumentTemplates.hpp>
#include <com/sun/star/io/XInputStream.hpp>
#include <com/sun/star/io/XPersist.hpp>
#include <com/sun/star/lang/XLocalizable.hpp>
#include <com/sun/star/sdbc/XResultSet.hpp>
#include <com/sun/star/sdbc/XRow.hpp>
#include <com/sun/star/ucb/ContentInfo.hpp>
#include <com/sun/star/ucb/InsertCommandArgument.hpp>
#include <com/sun/star/ucb/NameClash.hpp>
#include <com/sun/star/ucb/TransferInfo.hpp>
#include <com/sun/star/ucb/XCommandProcessor.hpp>
#include <com/sun/star/ucb/XContent.hpp>
#include <com/sun/star/ucb/XContentAccess.hpp>
#include <com/sun/star/ucb/XAnyCompareFactory.hpp>
#include <com/sun/star/ucb/XAnyCompare.hpp>
#include <com/sun/star/ucb/NumberedSortingInfo.hpp>
#include <com/sun/star/embed/ElementModes.hpp>
#include <com/sun/star/embed/XTransactedObject.hpp>

#include "sfxurlrelocator.hxx"

using namespace ::com::sun::star;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::frame;
using namespace ::com::sun::star::io;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::sdbc;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::ucb;
using namespace ::com::sun::star::document;
using namespace ::rtl;
using namespace ::ucbhelper;


#include <sfx2/doctempl.hxx>
#include <sfx2/docfac.hxx>
#include <sfx2/docfile.hxx>
#include <sfx2/objsh.hxx>
#include "sfxtypes.hxx"
#include <sfx2/app.hxx>
#include "sfx2/sfxresid.hxx"
#include "doc.hrc"
#include <sfx2/fcontnr.hxx>
#include <svtools/templatefoldercache.hxx>

#include <comphelper/storagehelper.hxx>
#include <unotools/ucbhelper.hxx>

//========================================================================

// #define DONT_USE_HIERARCHY

#define TITLE                   "Title"
#define IS_FOLDER               "IsFolder"
#define PROPERTY_TYPE           "TypeDescription"
#define TARGET_URL              "TargetURL"
#define TYPE_FOLDER             "application/vnd.sun.star.hier-folder"
#define TYPE_LINK               "application/vnd.sun.star.hier-link"
#define TYPE_FSYS_FOLDER        "application/vnd.sun.staroffice.fsys-folder"

#define TARGET_DIR_URL          "TargetDirURL"
#define COMMAND_DELETE          "delete"
#define COMMAND_TRANSFER        "transfer"

#define STANDARD_FOLDER         "standard"

#define SERVICENAME_TYPEDETECTION       "com.sun.star.document.TypeDetection"
#define TYPEDETECTION_PARAMETER         "FileName"
//#define SERVICENAME_OLD_TYPEDETECTION   "com.sun.star.frame.FrameLoaderFactory"
//#define PARAMETER_OLD_TYPEDETECTION     "DeepDetection"
#define SERVICENAME_DOCINFO             "com.sun.star.document.DocumentProperties"
#define SERVICENAME_DOCTEMPLATES        "com.sun.star.frame.DocumentTemplates"
#define SERVICENAME_DESKTOP             "com.sun.star.frame.Desktop"

//========================================================================

class RegionData_Impl;

namespace DocTempl {

class DocTempl_EntryData_Impl
{
    RegionData_Impl*    mpParent;

    // the following member must be SfxObjectShellLock since it controls that SfxObjectShell lifetime by design
    // and users of this class expect it to be so.
    SfxObjectShellLock  mxObjShell;

    OUString            maTitle;
    OUString            maOwnURL;
    OUString            maTargetURL;
    sal_Bool            mbIsOwner   : 1;
    sal_Bool            mbDidConvert: 1;

private:
    RegionData_Impl*    GetParent() const { return mpParent; }

public:
                        DocTempl_EntryData_Impl( RegionData_Impl* pParent,
                                        const OUString& rTitle );

    const OUString&     GetTitle() const { return maTitle; }
    const OUString&     GetTargetURL();
    const OUString&     GetHierarchyURL();

    void                SetTitle( const OUString& rTitle ) { maTitle = rTitle; }
    void                SetTargetURL( const OUString& rURL ) { maTargetURL = rURL; }
    void                SetHierarchyURL( const OUString& rURL) { maOwnURL = rURL; }

    int                 Compare( const OUString& rTitle ) const;

    SfxObjectShellRef   CreateObjectShell();
    sal_Bool                DeleteObjectShell();
};

}

using namespace ::DocTempl;

// ------------------------------------------------------------------------

class RegionData_Impl
{
    DECLARE_LIST( EntryList_Impl, DocTempl_EntryData_Impl* )
    const SfxDocTemplate_Impl*  mpParent;
    EntryList_Impl              maEntries;
    OUString                    maTitle;
    OUString                    maOwnURL;
    OUString                    maTargetURL;

private:
    long                        GetEntryPos( const OUString& rTitle,
                                             sal_Bool& rFound ) const;
    const SfxDocTemplate_Impl*  GetParent() const { return mpParent; }

public:
                        RegionData_Impl( const SfxDocTemplate_Impl* pParent,
                                         const OUString& rTitle );
                        ~RegionData_Impl();

    void                SetTargetURL( const OUString& rURL ) { maTargetURL = rURL; }
    void                SetHierarchyURL( const OUString& rURL) { maOwnURL = rURL; }

    DocTempl_EntryData_Impl*     GetEntry( sal_uIntPtr nIndex ) const;
    DocTempl_EntryData_Impl*     GetEntry( const OUString& rName ) const;
    DocTempl_EntryData_Impl*     GetByTargetURL( const OUString& rName ) const;

    const OUString&     GetTitle() const { return maTitle; }
    const OUString&     GetTargetURL();
    const OUString&     GetHierarchyURL();

    sal_uIntPtr               GetCount() const;

    void                SetTitle( const OUString& rTitle ) { maTitle = rTitle; }

    void                AddEntry( const OUString& rTitle,
                                  const OUString& rTargetURL,
                                  sal_uInt16 *pPos = NULL );
    void                DeleteEntry( sal_uIntPtr nIndex );

    int                 Compare( const OUString& rTitle ) const
                            { return maTitle.compareTo( rTitle ); }
    int                 Compare( RegionData_Impl* pCompareWith ) const;
};

DECLARE_LIST( RegionList_Impl, RegionData_Impl* )

// ------------------------------------------------------------------------

class SfxDocTemplate_Impl : public SvRefBase
{
    uno::Reference< XPersist >               mxInfo;
    uno::Reference< XDocumentTemplates >     mxTemplates;

    ::osl::Mutex        maMutex;
    OUString            maRootURL;
    OUString            maStandardGroup;
    RegionList_Impl     maRegions;
    sal_Bool            mbConstructed;

    uno::Reference< XAnyCompareFactory > m_rCompareFactory;

    // the following member is intended to prevent clearing of the global data when it is in use
    // TODO/LATER: it still does not make the implementation complete thread-safe
    sal_Int32           mnLockCounter;

private:
    void                Clear();

public:
                        SfxDocTemplate_Impl();
                        ~SfxDocTemplate_Impl();

    void                IncrementLock();
    void                DecrementLock();

    sal_Bool            Construct( );
    void                CreateFromHierarchy( Content &rTemplRoot );
    void                ReInitFromComponent();
    void                AddRegion( const OUString& rTitle,
                                   Content& rContent );

    void                Rescan();

    void                DeleteRegion( sal_uIntPtr nIndex );

    sal_uIntPtr               GetRegionCount() const
                            { return maRegions.Count(); }
    RegionData_Impl*    GetRegion( const OUString& rName ) const;
    RegionData_Impl*    GetRegion( sal_uIntPtr nIndex ) const;
    void                GetTemplates( Content& rTargetFolder,
                                      Content& rParentFolder,
                                      RegionData_Impl* pRegion );

    long                GetRegionPos( const OUString& rTitle,
                                      sal_Bool& rFound ) const;

    sal_Bool            GetTitleFromURL( const OUString& rURL, OUString& aTitle );
    sal_Bool            InsertRegion( RegionData_Impl *pData, sal_uIntPtr nPos = LIST_APPEND );
    OUString            GetRootURL() const { return maRootURL; }

    uno::Reference< XDocumentTemplates >     getDocTemplates() { return mxTemplates; }
};

// ------------------------------------------------------------------------

class DocTemplLocker_Impl
{
    SfxDocTemplate_Impl& m_aDocTempl;
public:
    DocTemplLocker_Impl( SfxDocTemplate_Impl& aDocTempl )
    : m_aDocTempl( aDocTempl )
    {
        m_aDocTempl.IncrementLock();
    }

    ~DocTemplLocker_Impl()
    {
        m_aDocTempl.DecrementLock();
    }
};

// ------------------------------------------------------------------------

#ifndef SFX_DECL_DOCTEMPLATES_DEFINED
#define SFX_DECL_DOCTEMPLATES_DEFINED
SV_DECL_REF(SfxDocTemplate_Impl)
#endif

SV_IMPL_REF(SfxDocTemplate_Impl)

// ------------------------------------------------------------------------

SfxDocTemplate_Impl *gpTemplateData = 0;

// -----------------------------------------------------------------------

static sal_Bool getTextProperty_Impl( Content& rContent,
                                      const OUString& rPropName,
                                      OUString& rPropValue );

//========================================================================
//========================================================================
//========================================================================

String SfxDocumentTemplates::GetFullRegionName
(
    sal_uInt16 nIdx                     // Index des Bereiches
)   const

/*  [Beschreibung]

    Liefert den logischen Namen eines Bereiches Plus seinem  Pfad


    [R"uckgabewert]                 Referenz auf diesen Namen

*/

{
    // First: find the RegionData for the index
    String aName;

    DocTemplLocker_Impl aLocker( *pImp );

    if ( pImp->Construct() )
    {
        RegionData_Impl *pData1 = pImp->GetRegion( nIdx );

        if ( pData1 )
            aName = pData1->GetTitle();

        // --**-- here was some code which appended the path to the
        //      group if there was more than one with the same name.
        //      this should not happen anymore
    }

    return aName;
}

//------------------------------------------------------------------------

const String& SfxDocumentTemplates::GetRegionName
(
    sal_uInt16 nIdx                 // Index des Bereiches
)   const

/*  [Beschreibung]

    Liefert den logischen Namen eines Bereiches


    [R"uckgabewert]

    const String&                   Referenz auf diesen Namen

*/
{
    static String maTmpString;

    DocTemplLocker_Impl aLocker( *pImp );

    if ( pImp->Construct() )
    {
        RegionData_Impl *pData = pImp->GetRegion( nIdx );

        if ( pData )
            maTmpString = pData->GetTitle();
        else
            maTmpString.Erase();
    }
    else
        maTmpString.Erase();

    return maTmpString;
}


//------------------------------------------------------------------------

sal_uInt16 SfxDocumentTemplates::GetRegionNo
(
    const String &rRegion       // Name der Region
)   const

/*  [Beschreibung]

    Liefert den Index f"ur einen logischen Namen eines Bereiches.


    [R"uckgabewert]

    sal_uInt16          Index von 'rRegion' oder USHRT_MAX falls unbekannt

*/
{
    DocTemplLocker_Impl aLocker( *pImp );

    if ( !pImp->Construct() )
        return USHRT_MAX;

    sal_Bool    bFound;
    sal_uIntPtr       nIndex = pImp->GetRegionPos( rRegion, bFound );

    if ( bFound )
        return (sal_uInt16) nIndex;
    else
        return USHRT_MAX;
}


//------------------------------------------------------------------------

sal_uInt16 SfxDocumentTemplates::GetRegionCount() const

/*  [Beschreibung]

    Liefert die Anzahl der Bereiche


    [R"uckgabewert]

    sal_uInt16                  Anzahl der Bereiche

*/
{
    DocTemplLocker_Impl aLocker( *pImp );

    if ( !pImp->Construct() )
        return 0;

    sal_uIntPtr nCount = pImp->GetRegionCount();

    return (sal_uInt16) nCount;
}

//------------------------------------------------------------------------

sal_Bool SfxDocumentTemplates::IsRegionLoaded( sal_uInt16 nIdx ) const
{
    DocTemplLocker_Impl aLocker( *pImp );

    if ( !pImp->Construct() )
        return sal_False;

    RegionData_Impl *pData = pImp->GetRegion( nIdx );

    if ( pData )
        return sal_True;
    else
        return sal_False;
}

//------------------------------------------------------------------------

sal_uInt16 SfxDocumentTemplates::GetCount
(
    const String&   rName   /*  Name des Bereiches, dessen Eintrags-
                                anzahl ermittelt werden soll */

)   const

/*  [Beschreibung]

    Liefert die Anzahl der Eintr"age eines Bereiches


    [R"uckgabewert]

    USHORT                      Anzahl der Eintr"age

*/

{
    DocTemplLocker_Impl aLocker( *pImp );

    if ( !pImp->Construct() )
        return 0;

    RegionData_Impl *pData = pImp->GetRegion( rName );
    sal_uIntPtr            nCount = 0;

    if ( pData )
        nCount = pData->GetCount();

    return (sal_uInt16) nCount;
}

//------------------------------------------------------------------------

sal_uInt16 SfxDocumentTemplates::GetCount
(
    sal_uInt16 nRegion              /*  Index des Bereiches, dessen Eintrags-
                                    anzahl ermittelt werden soll */

)   const

/*  [Beschreibung]

    Liefert die Anzahl der Eintr"age eines Bereiches


    [R"uckgabewert]                 Anzahl der Eintr"age

*/

{
    DocTemplLocker_Impl aLocker( *pImp );

    if ( !pImp->Construct() )
        return 0;

    RegionData_Impl *pData = pImp->GetRegion( nRegion );
    sal_uIntPtr            nCount = 0;

    if ( pData )
        nCount = pData->GetCount();

    return (sal_uInt16) nCount;
}

//------------------------------------------------------------------------

const String& SfxDocumentTemplates::GetName
(
    sal_uInt16 nRegion,     //  Index des Bereiches, in dem der Eintrag liegt
    sal_uInt16 nIdx         //  Index des Eintrags
)   const

/*  [Beschreibung]

    Liefert den logischen Namen eines Eintrags eines Bereiches


    [R"uckgabewert]

    const String&           Name des Eintrags

*/

{
    DocTemplLocker_Impl aLocker( *pImp );

    static String maTmpString;

    if ( pImp->Construct() )
    {
        DocTempl_EntryData_Impl  *pEntry = NULL;
        RegionData_Impl *pRegion = pImp->GetRegion( nRegion );

        if ( pRegion )
            pEntry = pRegion->GetEntry( nIdx );

        if ( pEntry )
            maTmpString = pEntry->GetTitle();
        else
            maTmpString.Erase();
    }
    else
        maTmpString.Erase();

    return maTmpString;
}

//------------------------------------------------------------------------

String SfxDocumentTemplates::GetFileName
(
    sal_uInt16 nRegion,     //  Index des Bereiches, in dem der Eintrag liegt
    sal_uInt16 nIdx         //  Index des Eintrags
)   const

/*  [Beschreibung]

    Liefert den Dateinamen eines Eintrags eines Bereiches

    [R"uckgabewert]                 Dateiname des Eintrags

*/
{
    DocTemplLocker_Impl aLocker( *pImp );

    if ( !pImp->Construct() )
        return String();

    DocTempl_EntryData_Impl  *pEntry = NULL;
    RegionData_Impl *pRegion = pImp->GetRegion( nRegion );

    if ( pRegion )
        pEntry = pRegion->GetEntry( nIdx );

    if ( pEntry )
    {
        INetURLObject aURLObj( pEntry->GetTargetURL() );
        return aURLObj.getName( INetURLObject::LAST_SEGMENT, true,
                                INetURLObject::DECODE_WITH_CHARSET );
    }
    else
        return String();
}

//------------------------------------------------------------------------

String SfxDocumentTemplates::GetPath
(
    sal_uInt16  nRegion,    //  Index des Bereiches, in dem der Eintrag liegt
    sal_uInt16  nIdx        //  Index des Eintrags
)   const

/*  [Beschreibung]

    Liefert den Dateinamen mit vollst"andigem Pfad zu der einem
    Eintrag zugeordneten Datei


    [R"uckgabewert]

    String                  Dateiname mit vollst"andigem Pfad

*/
{
    DocTemplLocker_Impl aLocker( *pImp );

    if ( !pImp->Construct() )
        return String();

    DocTempl_EntryData_Impl  *pEntry = NULL;
    RegionData_Impl *pRegion = pImp->GetRegion( nRegion );

    if ( pRegion )
        pEntry = pRegion->GetEntry( nIdx );

    if ( pEntry )
        return pEntry->GetTargetURL();
    else
        return String();
}

//------------------------------------------------------------------------

String SfxDocumentTemplates::GetTemplatePath
(
    sal_uInt16          nRegion,    //  Index des Bereiches, in dem der Eintrag liegt
    const String&   rLongName   //  logischer Name des Eintrags
)   const

/*  [Beschreibung]

    Liefert den Dateinamen mit vollst"andigem Pfad zu der einem
    Eintrag zugeordneten Datei


    [R"uckgabewert]

    String                          Dateiname mit vollst"andigem Pfad

*/
{
    DocTemplLocker_Impl aLocker( *pImp );

    if ( !pImp->Construct() )
        return String();

    DocTempl_EntryData_Impl  *pEntry = NULL;
    RegionData_Impl *pRegion = pImp->GetRegion( nRegion );

    if ( pRegion )
        pEntry = pRegion->GetEntry( rLongName );

    if ( pEntry )
        return pEntry->GetTargetURL();
    else if ( pRegion )
    {
        // a new template is going to be inserted, generate a new URL
        // TODO/LATER: if the title is localized, use minimized URL in future

        // --**-- extension handling will become more complicated, because
        //          every new document type will have it's own extension
        //          e.g.: .stw or .stc instead of .vor
        INetURLObject aURLObj( pRegion->GetTargetURL() );
        aURLObj.insertName( rLongName, false,
                     INetURLObject::LAST_SEGMENT, true,
                     INetURLObject::ENCODE_ALL );

        OUString aExtension = aURLObj.getExtension();

        if ( ! aExtension.getLength() )
            aURLObj.setExtension( OUString( RTL_CONSTASCII_USTRINGPARAM( "vor" ) ) );

        return aURLObj.GetMainURL( INetURLObject::NO_DECODE );
    }
    else
        return String();
}

//------------------------------------------------------------------------

String SfxDocumentTemplates::GetDefaultTemplatePath
(
    const String& rLongName
)

/*  [Beschreibung]

    Liefert den Standardpfad zu Dokumentvorlagen


    [R"uckgabewert]

    String                  Standardpfad zu Dokumentvorlagen

*/
{
    DocTemplLocker_Impl aLocker( *pImp );

    if ( ! pImp->Construct() )
        return String();

    // the first region in the list should always be the standard
    // group
    // --**-- perhaps we have to create it ???
    RegionData_Impl *pRegion = pImp->GetRegion( 0L );
    DocTempl_EntryData_Impl  *pEntry = NULL;

    if ( pRegion )
        pEntry = pRegion->GetEntry( rLongName );

    if ( pEntry )
        return pEntry->GetTargetURL();
    else if ( pRegion )
    {
        // a new template is going to be inserted, generate a new URL
        // TODO/LATER: if the title is localized, use minimized URL in future

        INetURLObject aURLObj( pRegion->GetTargetURL() );
        aURLObj.insertName( rLongName, false,
                     INetURLObject::LAST_SEGMENT, true,
                     INetURLObject::ENCODE_ALL );

        OUString aExtension = aURLObj.getExtension();

        if ( ! aExtension.getLength() )
            aURLObj.setExtension( OUString( RTL_CONSTASCII_USTRINGPARAM( "vor" ) ) );

        return aURLObj.GetMainURL( INetURLObject::NO_DECODE );
    }
    else
        return String();

/* dv! missing: create the directory, if it doesn't exists


    DBG_ASSERT(aDirs.GetTokenCount(cDelim), "Keine Bereiche");
    DirEntry aPath(aDirs.GetToken(0, cDelim));

    // Verzeichnis anlegen
    if(!aPath.MakeDir())
        return String();

    MakeFileName_Impl(aPath, rLongName, sal_True);
    SfxTemplateDir  *pEntry = new SfxTemplateDir;
    SfxTemplateDirEntryPtr pDirEntry =
        new SfxTemplateDirEntry( String( '.' ), aPath.GetPath() );
    pDirEntry->SetContent(new SfxTemplateDir(aPath.GetPath()));
    pEntry->Insert(pDirEntry, pEntry->Count());
    pDirs->Insert(pEntry, pDirs->Count());
    return aPath.GetFull();
*/
}

//------------------------------------------------------------------------

::rtl::OUString SfxDocumentTemplates::GetTemplateTargetURLFromComponent( const ::rtl::OUString& aGroupName,
                                                                         const ::rtl::OUString& aTitle )
{
    DocTemplLocker_Impl aLocker( *pImp );

    INetURLObject aTemplateObj( pImp->GetRootURL() );

    aTemplateObj.insertName( aGroupName, false,
                        INetURLObject::LAST_SEGMENT, true,
                        INetURLObject::ENCODE_ALL );

    aTemplateObj.insertName( aTitle, false,
                        INetURLObject::LAST_SEGMENT, true,
                        INetURLObject::ENCODE_ALL );


    ::rtl::OUString aResult;
    Content aTemplate;
    uno::Reference< XCommandEnvironment > aCmdEnv;
    if ( Content::create( aTemplateObj.GetMainURL( INetURLObject::NO_DECODE ), aCmdEnv, aTemplate ) )
    {
        OUString aPropName( RTL_CONSTASCII_USTRINGPARAM( TARGET_URL ) );
        getTextProperty_Impl( aTemplate, aPropName, aResult );
        aResult = SvtPathOptions().SubstituteVariable( aResult );
    }

    return aResult;
}

//------------------------------------------------------------------------

sal_Bool SfxDocumentTemplates::SaveDir
(
//  SfxTemplateDir& rDir        //  das zu speichernde Directory
)

/*  [Beschreibung]

    Speichert das Directory rDir


    [R"uckgabewert]

    sal_Bool                        sal_False,
                                Schreibfehler

                                sal_True
                                gespeichert

*/

{
    return sal_True;
}

//------------------------------------------------------------------------

void SfxDocumentTemplates::NewTemplate
(
    sal_uInt16          nRegion,    /*  Index des Bereiches, in dem die Vorlage
                                    angelegt werden soll */

    const String&   rLongName,  //  logischer Name der neuen Vorlage
    const String&   rFileName   //  Dateiname der neuen Vorlage
)

/*  [Beschreibung]

    Eintragen einer neuen Dokumentvorlage in die Verwaltungsstrukturen
    Das "Uberschreiben einer Vorlage gleichen Namens wird
    verhindert (!! Fehlermeldung)

*/

{
    DocTemplLocker_Impl aLocker( *pImp );

    if ( ! pImp->Construct() )
        return;

    DocTempl_EntryData_Impl  *pEntry;
    RegionData_Impl *pRegion = pImp->GetRegion( nRegion );

    // do nothing if there is no region with that index
    if ( !pRegion )
        return;

    pEntry = pRegion->GetEntry( rLongName );

    // do nothing if there is already an entry with that name
    if ( pEntry )
        return;

    uno::Reference< XDocumentTemplates > xTemplates = pImp->getDocTemplates();

    if ( xTemplates->addTemplate( pRegion->GetTitle(), rLongName, rFileName ) )
        pRegion->AddEntry( rLongName, rFileName );
}

//------------------------------------------------------------------------

sal_Bool SfxDocumentTemplates::CopyOrMove
(
    sal_uInt16  nTargetRegion,      //  Index des Zielbereiches
    sal_uInt16  nTargetIdx,         //  Index Zielposition
    sal_uInt16  nSourceRegion,      //  Index des Quellbereiches
    sal_uInt16  nSourceIdx,         /*  Index der zu kopierenden / zu verschiebenden
                                    Dokumentvorlage */
    sal_Bool    bMove               //  kopieren / verschieben
)

/*  [Beschreibung]

    Kopieren oder Verschieben einer Dokumentvorlage

    [R"uckgabewert]

    sal_Bool                            sal_True
                                    Aktion konnte ausgef"uhrt werden

                                    sal_False
                                    Aktion konnte nicht ausgef2uhrt werden
    [Querverweise]

    <SfxDocumentTemplates::Move(sal_uInt16,sal_uInt16,sal_uInt16,sal_uInt16)>
    <SfxDocumentTemplates::Copy(sal_uInt16,sal_uInt16,sal_uInt16,sal_uInt16)>
*/

{
    /* to perform a copy or move, we need to send a transfer command to
       the destination folder with the URL of the source as parameter.
       ( If the destination content doesn't support the transfer command,
       we could try a copy ( and delete ) instead. )
       We need two transfers ( one for the real template and one for its
       representation in the hierarchy )
       ...
    */

    DocTemplLocker_Impl aLocker( *pImp );

    if ( !pImp->Construct() )
        return sal_False;

    // Don't copy or move any folders
    if( nSourceIdx == USHRT_MAX )
        return sal_False ;

    if ( nSourceRegion == nTargetRegion )
    {
        DBG_ERRORFILE( "Don't know, what to do!" );
        return sal_False;
#if 0
    // Verschieben einer Vorlage innerhalb eines Bereiches
    // --> nur Verwaltungsdaten aktualisieren
    if ( bMove && nTargetRegion == nSourceRegion )
    {
        if(nTargetIdx == USHRT_MAX)
            nTargetIdx = 0;
        const SfxTemplateDirEntryPtr pEntry = rTargetDir[nSourceIdx];
        rTargetDir.Insert(pEntry, nTargetIdx);
        if(nTargetIdx < nSourceIdx)
            ++nSourceIdx;
        rTargetDir.Remove(nSourceIdx);
        return SaveDir(rTargetDir);
    }
#endif
    }

    RegionData_Impl *pSourceRgn = pImp->GetRegion( nSourceRegion );
    if ( !pSourceRgn )
        return sal_False;

    DocTempl_EntryData_Impl *pSource = pSourceRgn->GetEntry( nSourceIdx );
    if ( !pSource )
        return sal_False;

    RegionData_Impl *pTargetRgn = pImp->GetRegion( nTargetRegion );
    if ( !pTargetRgn )
        return sal_False;

    OUString aTitle = pSource->GetTitle();

    uno::Reference< XDocumentTemplates > xTemplates = pImp->getDocTemplates();

    if ( xTemplates->addTemplate( pTargetRgn->GetTitle(),
                                  aTitle,
                                  pSource->GetTargetURL() ) )
    {

        INetURLObject aSourceObj( pSource->GetTargetURL() );

        ::rtl::OUString aNewTargetURL = GetTemplateTargetURLFromComponent( pTargetRgn->GetTitle(), aTitle );
        if ( !aNewTargetURL.getLength() )
            return sal_False;

        if ( bMove )
        {
            // --**-- delete the original file
            sal_Bool bDeleted = xTemplates->removeTemplate( pSourceRgn->GetTitle(),
                                                            pSource->GetTitle() );
            if ( bDeleted )
                pSourceRgn->DeleteEntry( nSourceIdx );
            else
            {
                if ( xTemplates->removeTemplate( pTargetRgn->GetTitle(), aTitle ) )
                    return sal_False; // will trigger tetry with copy instead of move

                // if it is not possible to remove just created template ( must be possible! )
                // it is better to report success here, since at least the copy has succeeded
                // TODO/LATER: solve it more gracefully in future
            }
        }

        pTargetRgn->AddEntry( aTitle, aNewTargetURL, &nTargetIdx );

        return sal_True;
    }

    // --**-- wenn aktuell das File geoeffnet ist,
    // muss es hinterher wieder geoeffnet werden

    return sal_False;
}

//------------------------------------------------------------------------

sal_Bool SfxDocumentTemplates::Move
(
    sal_uInt16 nTargetRegion,       //  Index des Zielbereiches
    sal_uInt16 nTargetIdx,          //  Index Zielposition
    sal_uInt16 nSourceRegion,       //  Index des Quellbereiches
    sal_uInt16 nSourceIdx           /*  Index der zu kopierenden / zu verschiebenden
                                    Dokumentvorlage */
)

/*  [Beschreibung]

    Verschieben einer Dokumentvorlage


    [R"uckgabewert]

    sal_Bool                            sal_True
                                    Aktion konnte ausgef"uhrt werden

                                    sal_False
                                    Aktion konnte nicht ausgef2uhrt werden

    [Querverweise]

    <SfxDocumentTemplates::CopyOrMove(sal_uInt16,sal_uInt16,sal_uInt16,sal_uInt16,sal_Bool)>
*/
{
    DocTemplLocker_Impl aLocker( *pImp );

    return CopyOrMove( nTargetRegion, nTargetIdx,
                       nSourceRegion, nSourceIdx, sal_True );
}

//------------------------------------------------------------------------

sal_Bool SfxDocumentTemplates::Copy
(
    sal_uInt16 nTargetRegion,       //  Index des Zielbereiches
    sal_uInt16 nTargetIdx,          //  Index Zielposition
    sal_uInt16 nSourceRegion,       //  Index des Quellbereiches
    sal_uInt16 nSourceIdx           /*  Index der zu kopierenden / zu verschiebenden
                                    Dokumentvorlage */
)

/*  [Beschreibung]

    Kopieren einer Dokumentvorlage


    [R"uckgabewert]

    sal_Bool                            sal_True
                                    Aktion konnte ausgef"uhrt werden

                                    sal_False
                                    Aktion konnte nicht ausgef"uhrt werden

    [Querverweise]

    <SfxDocumentTemplates::CopyOrMove(sal_uInt16,sal_uInt16,sal_uInt16,sal_uInt16,sal_Bool)>
*/

{
    DocTemplLocker_Impl aLocker( *pImp );

    return CopyOrMove( nTargetRegion, nTargetIdx,
                       nSourceRegion, nSourceIdx, sal_False );
}

//------------------------------------------------------------------------

sal_Bool SfxDocumentTemplates::CopyTo
(
    sal_uInt16          nRegion,    /*  Bereich der Vorlage, die exportiert werden
                                    soll  */
    sal_uInt16          nIdx,       /*  Index der Vorlage, die exportiert werden
                                    soll */
    const String&   rName       /*  Dateiname, unter dem die Vorlage angelegt
                                    werden soll */
)   const

/*  [Beschreibung]

    Exportieren einer Dokumentvorlage in das Dateisystem


    [R"uckgabewert]

    sal_Bool                            sal_True
                                    Aktion konnte ausgef"uhrt werden

                                    sal_False
                                    Aktion konnte nicht ausgef"uhrt werden


    [Querverweise]

    <SfxDocumentTemplates::CopyFrom(sal_uInt16,sal_uInt16,String&)>
*/

{
    DocTemplLocker_Impl aLocker( *pImp );

    if ( ! pImp->Construct() )
        return sal_False;

    RegionData_Impl *pSourceRgn = pImp->GetRegion( nRegion );
    if ( !pSourceRgn )
        return sal_False;

    DocTempl_EntryData_Impl *pSource = pSourceRgn->GetEntry( nIdx );
    if ( !pSource )
        return sal_False;

    INetURLObject aTargetURL( rName );

    OUString aTitle( aTargetURL.getName( INetURLObject::LAST_SEGMENT, true,
                                         INetURLObject::DECODE_WITH_CHARSET ) );
    aTargetURL.removeSegment();

    OUString aParentURL = aTargetURL.GetMainURL( INetURLObject::NO_DECODE );

    uno::Reference< XCommandEnvironment > aCmdEnv;
    Content aTarget;

    try
    {
        aTarget = Content( aParentURL, aCmdEnv );

        TransferInfo aTransferInfo;
        aTransferInfo.MoveData = sal_False;
        aTransferInfo.SourceURL = pSource->GetTargetURL();
        aTransferInfo.NewTitle = aTitle;
        aTransferInfo.NameClash = NameClash::OVERWRITE;

        Any aArg = makeAny( aTransferInfo );
        OUString aCmd( RTL_CONSTASCII_USTRINGPARAM( COMMAND_TRANSFER ) );

        aTarget.executeCommand( aCmd, aArg );
    }
    catch ( ContentCreationException& )
    { return sal_False; }
    catch ( Exception& )
    { return sal_False; }

    return sal_True;
}

//------------------------------------------------------------------------

sal_Bool SfxDocumentTemplates::CopyFrom
(
    sal_uInt16      nRegion,        /*  Bereich, in den die Vorlage importiert
                                    werden soll */
    sal_uInt16      nIdx,           //  Index der neuen Vorlage in diesem Bereich
    String&     rName           /*  Dateiname der Vorlage, die importiert
                                    werden soll, als out-Parameter der (auto-
                                    matisch aus dem Dateinamen generierte)
                                    logische Name der Vorlage */
)

/*  [Beschreibung]

    Importieren einer Dokumentvorlage aus dem Dateisystem


    [R"uckgabewert]                 Erfolg (sal_True) oder Mi"serfpTargetDirectory->GetContent());

    sal_Bool                            sal_True
                                    Aktion konnte ausgef"uhrt werden

                                    sal_False
                                    Aktion konnte nicht ausgef"uhrt werden

    [Querverweise]

    <SfxDocumentTemplates::CopyTo(sal_uInt16,sal_uInt16,const String&)>
*/

{
    DocTemplLocker_Impl aLocker( *pImp );

    if ( ! pImp->Construct() )
        return sal_False;

    RegionData_Impl *pTargetRgn = pImp->GetRegion( nRegion );

    if ( !pTargetRgn )
        return sal_False;

    uno::Reference< XDocumentTemplates > xTemplates = pImp->getDocTemplates();
    if ( !xTemplates.is() )
        return sal_False;

    OUString aTitle;
    sal_Bool bTemplateAdded = sal_False;

    if( pImp->GetTitleFromURL( rName, aTitle ) )
    {
        bTemplateAdded = xTemplates->addTemplate( pTargetRgn->GetTitle(), aTitle, rName );
    }
    else
    {
        OUString aService( RTL_CONSTASCII_USTRINGPARAM( SERVICENAME_DESKTOP ) );
        uno::Reference< XComponentLoader > xDesktop( ::comphelper::getProcessServiceFactory()->createInstance( aService ),
                                                UNO_QUERY );

        Sequence< PropertyValue > aArgs( 1 );
        aArgs[0].Name = ::rtl::OUString::createFromAscii("Hidden");
        aArgs[0].Value <<= sal_True;

        INetURLObject   aTemplURL( rName );
        uno::Reference< XDocumentPropertiesSupplier > xDocPropsSupplier;
        uno::Reference< XStorable > xStorable;
        try
        {
            xStorable = uno::Reference< XStorable >(
                xDesktop->loadComponentFromURL( aTemplURL.GetMainURL(INetURLObject::NO_DECODE),
                                                OUString::createFromAscii( "_blank" ),
                                                0,
                                                aArgs ),
                UNO_QUERY );

            xDocPropsSupplier = uno::Reference< XDocumentPropertiesSupplier >(
                xStorable, UNO_QUERY );
        }
        catch( Exception& )
        {
        }

        if( xStorable.is() )
        {
            // get Title from XDocumentPropertiesSupplier
            if( xDocPropsSupplier.is() )
            {
                uno::Reference< XDocumentProperties > xDocProps
                    = xDocPropsSupplier->getDocumentProperties();
                if (xDocProps.is() ) {
                    aTitle = xDocProps->getTitle();
                }
            }

            if( ! aTitle.getLength() )
            {
                INetURLObject aURL( aTemplURL );
                aURL.CutExtension();
                aTitle = aURL.getName( INetURLObject::LAST_SEGMENT, true,
                                        INetURLObject::DECODE_WITH_CHARSET );
            }

            // write a template using XStorable interface
            bTemplateAdded = xTemplates->storeTemplate( pTargetRgn->GetTitle(), aTitle, xStorable );
        }
    }


    if( bTemplateAdded )
    {
        INetURLObject aTemplObj( pTargetRgn->GetHierarchyURL() );
        aTemplObj.insertName( aTitle, false,
                              INetURLObject::LAST_SEGMENT, true,
                              INetURLObject::ENCODE_ALL );
        OUString aTemplURL = aTemplObj.GetMainURL( INetURLObject::NO_DECODE );

        uno::Reference< XCommandEnvironment > aCmdEnv;
        Content aTemplCont;

        if( Content::create( aTemplURL, aCmdEnv, aTemplCont ) )
        {
            OUString aTemplName;
            OUString aPropName( RTL_CONSTASCII_USTRINGPARAM( TARGET_URL ) );

            if( getTextProperty_Impl( aTemplCont, aPropName, aTemplName ) )
            {
                if ( nIdx == USHRT_MAX )
                    nIdx = 0;
                else
                    nIdx += 1;

                pTargetRgn->AddEntry( aTitle, aTemplName, &nIdx );
                rName = aTitle;
                return sal_True;
            }
            else
            {
                DBG_ASSERT( sal_False, "CopyFrom(): The content should contain target URL!" );
            }
        }
        else
        {
            DBG_ASSERT( sal_False, "CopyFrom(): The content just was created!" );
        }
    }

    return sal_False;
}

//------------------------------------------------------------------------

sal_Bool SfxDocumentTemplates::Delete
(
    sal_uInt16 nRegion,             //  Index des Bereiches
    sal_uInt16 nIdx                 /*  Index des Eintrags oder USHRT_MAX,
                                    wenn ein Verzeichnis gemeint ist. */
)

/*  [Beschreibung]

    "oschen eines Eintrags oder eines Verzeichnisses


    [R"uckgabewert]

    sal_Bool                            sal_True
                                    Aktion konnte ausgef"uhrt werden

                                    sal_False
                                    Aktion konnte nicht ausgef"uhrt werden


    [Querverweise]

    <SfxDocumentTemplates::InsertDir(const String&,sal_uInt16)>
    <SfxDocumentTemplates::KillDir(SfxTemplateDir&)>
    <SfxDocumentTemplates::SaveDir(SfxTemplateDir&)>
*/

{
    DocTemplLocker_Impl aLocker( *pImp );

    /* delete the template or folder in the hierarchy and in the
       template folder by sending a delete command to the content.
       Then remove the data from the lists
    */
    if ( ! pImp->Construct() )
        return sal_False;

    RegionData_Impl *pRegion = pImp->GetRegion( nRegion );

    if ( !pRegion )
        return sal_False;

    sal_Bool bRet;
    uno::Reference< XDocumentTemplates > xTemplates = pImp->getDocTemplates();

    if ( nIdx == USHRT_MAX )
    {
        bRet = xTemplates->removeGroup( pRegion->GetTitle() );
        if ( bRet )
            pImp->DeleteRegion( nRegion );
    }
    else
    {
        DocTempl_EntryData_Impl *pEntry = pRegion->GetEntry( nIdx );

        if ( !pEntry )
            return sal_False;

        bRet = xTemplates->removeTemplate( pRegion->GetTitle(),
                                           pEntry->GetTitle() );
        if( bRet )
            pRegion->DeleteEntry( nIdx );
    }

    return bRet;
}

//------------------------------------------------------------------------

sal_Bool SfxDocumentTemplates::InsertDir
(
    const String&   rText,      //  der logische Name des neuen Bereiches
    sal_uInt16          nRegion     //  Index des Bereiches
)

/*  [Beschreibung]

    Einf"ugen eines Verzeichnisses


    [R"uckgabewert]

    sal_Bool                            sal_True
                                    Aktion konnte ausgef"uhrt werden

                                    sal_False
                                    Aktion konnte nicht ausgef"uhrt werden

    [Querverweise]

    <SfxDocumentTemplates::KillDir(SfxTemplateDir&)>
    <SfxDocumentTemplates::SaveDir(SfxTemplateDir&)>
*/
{
    DocTemplLocker_Impl aLocker( *pImp );

    if ( ! pImp->Construct() )
        return sal_False;

    RegionData_Impl *pRegion = pImp->GetRegion( rText );

    if ( pRegion )
        return sal_False;

    uno::Reference< XDocumentTemplates > xTemplates = pImp->getDocTemplates();

    if ( xTemplates->addGroup( rText ) )
    {
        RegionData_Impl* pNewRegion = new RegionData_Impl( pImp, rText );

        if ( ! pImp->InsertRegion( pNewRegion, nRegion ) )
        {
            delete pNewRegion;
            return sal_False;
        }
        return sal_True;
    }

    return sal_False;
}

//------------------------------------------------------------------------

sal_Bool SfxDocumentTemplates::SetName
(
    const String&   rName,      //  Der zu setzende Name
    sal_uInt16          nRegion,    //  Index des Bereiches
    sal_uInt16          nIdx        /*  Index des Eintrags oder USHRT_MAX,
                                    wenn ein Verzeichnis gemeint ist. */
)

/*  [Beschreibung]

    "Andern des Namens eines Eintrags oder eines Verzeichnisses


    [R"uckgabewert]

    sal_Bool                            sal_True
                                    Aktion konnte ausgef"uhrt werden

                                    sal_False
                                    Aktion konnte nicht ausgef"uhrt werden

*/

{
    DocTemplLocker_Impl aLocker( *pImp );

    if ( ! pImp->Construct() )
        return sal_False;

    RegionData_Impl *pRegion = pImp->GetRegion( nRegion );
    DocTempl_EntryData_Impl *pEntry = NULL;

    if ( !pRegion )
        return sal_False;

    uno::Reference< XDocumentTemplates > xTemplates = pImp->getDocTemplates();
    OUString aEmpty;

    if ( nIdx == USHRT_MAX )
    {
        if ( pRegion->GetTitle() == OUString( rName ) )
            return sal_True;

        // we have to rename a region
        if ( xTemplates->renameGroup( pRegion->GetTitle(), rName ) )
        {
            pRegion->SetTitle( rName );
            pRegion->SetTargetURL( aEmpty );
            pRegion->SetHierarchyURL( aEmpty );
            return sal_True;
        }
    }
    else
    {
        pEntry = pRegion->GetEntry( nIdx );

        if ( !pEntry )
            return sal_False;

        if ( pEntry->GetTitle() == OUString( rName ) )
            return sal_True;

        if ( xTemplates->renameTemplate( pRegion->GetTitle(),
                                         pEntry->GetTitle(),
                                         rName ) )
        {
            pEntry->SetTitle( rName );
            pEntry->SetTargetURL( aEmpty );
            pEntry->SetHierarchyURL( aEmpty );
            return sal_True;
        }
    }

    return sal_False;
}

//------------------------------------------------------------------------

sal_Bool SfxDocumentTemplates::Rescan()

/*  [Beschreibung]

    Abgleich des Verwaltungsdaten mit dem aktuellen Zustand auf der Platte.
    Die logischen Namen, zu denen keine Datei mit existiert, werden aus
    der Verwaltungsstruktur entfernt; Dateien, zu denen kein Eintrag
    existiert, werden aufgenommen.


    [R"uckgabewert]

    sal_Bool                            sal_True
                                    Aktion konnte ausgef"uhrt werden

                                    sal_False
                                    Aktion konnte nicht ausgef"uhrt werden


    [Querverweise]

    <SfxTemplateDir::Scan(sal_Bool bDirectory, sal_Bool bSave)>
    <SfxTemplateDir::Freshen(const SfxTemplateDir &rNew)>
*/
{
    if ( !pImp->Construct() )
        return sal_False;

    pImp->Rescan();

    return sal_True;
}

//------------------------------------------------------------------------

SfxObjectShellRef SfxDocumentTemplates::CreateObjectShell
(
    sal_uInt16 nRegion,         //  Index des Bereiches
    sal_uInt16 nIdx             //  Index des Eintrags
)

/*  [Beschreibung]

    Zugriff auf die DokumentShell eines Eintrags


    [R"uckgabewert]

    SfxObjectShellRef         Referenz auf die ObjectShell


    [Querverweise]

    <SfxTemplateDirEntry::CreateObjectShell()>
    <SfxDocumentTemplates::DeleteObjectShell(sal_uInt16, sal_uInt16)>
*/

{
    DocTemplLocker_Impl aLocker( *pImp );

    if ( !pImp->Construct() )
        return NULL;

    RegionData_Impl *pRegion = pImp->GetRegion( nRegion );
    DocTempl_EntryData_Impl *pEntry = NULL;

    if ( pRegion )
        pEntry = pRegion->GetEntry( nIdx );

    if ( pEntry )
        return pEntry->CreateObjectShell();
    else
        return NULL;
}

//------------------------------------------------------------------------

sal_Bool SfxDocumentTemplates::DeleteObjectShell
(
    sal_uInt16 nRegion,         //  Index des Bereiches
    sal_uInt16 nIdx             //  Index des Eintrags
)

/*  [Beschreibung]

    Freigeben der ObjectShell eines Eintrags


    [R"uckgabewert]

    sal_Bool                            sal_True
                                    Aktion konnte ausgef"uhrt werden

                                    sal_False
                                    Aktion konnte nicht ausgef"uhrt werden

    [Querverweise]

    <SfxTemplateDirEntry::DeleteObjectShell()>
    <SfxDocumentTemplates::CreateObjectShell(sal_uInt16, sal_uInt16)>
*/

{
    DocTemplLocker_Impl aLocker( *pImp );

    if ( ! pImp->Construct() )
        return sal_True;

    RegionData_Impl *pRegion = pImp->GetRegion( nRegion );
    DocTempl_EntryData_Impl *pEntry = NULL;

    if ( pRegion )
        pEntry = pRegion->GetEntry( nIdx );

    if ( pEntry )
        return pEntry->DeleteObjectShell();
    else
        return sal_True;
}

//------------------------------------------------------------------------

sal_Bool SfxDocumentTemplates::GetFull
(
    const String &rRegion,      // Der Name des Bereiches
    const String &rName,        // Der Name der Vorlage
    String &rPath               // Out: Pfad + Dateiname
)

/*  [Beschreibung]

    Liefert Pfad + Dateiname zu der durch rRegion und rName bezeichneten
    Vorlage


    [R"uckgabewert]

    sal_Bool                            sal_True
                                    Aktion konnte ausgef"uhrt werden

                                    sal_False
                                    Aktion konnte nicht ausgef"uhrt werden


    [Querverweise]

    <SfxDocumentTemplates::GetLogicNames(const String&,String&,String&)>
*/

{
    DocTemplLocker_Impl aLocker( *pImp );

    // We don't search for empty names!
    if ( ! rName.Len() )
        return sal_False;

    if ( ! pImp->Construct() )
        return sal_False;

    DocTempl_EntryData_Impl* pEntry = NULL;
    const sal_uInt16 nCount = GetRegionCount();

    for ( sal_uInt16 i = 0; i < nCount; ++i )
    {
        RegionData_Impl *pRegion = pImp->GetRegion( i );

        if( pRegion &&
            ( !rRegion.Len() || ( rRegion == String( pRegion->GetTitle() ) ) ) )
        {
            pEntry = pRegion->GetEntry( rName );

            if ( pEntry )
            {
                rPath = pEntry->GetTargetURL();
                break;
            }
        }
    }

    return ( pEntry != NULL );
}

//------------------------------------------------------------------------

sal_Bool SfxDocumentTemplates::GetLogicNames
(
    const String &rPath,            // vollst"andiger Pfad zu der Vorlage
    String &rRegion,                // Out: der Bereichsname
    String &rName                   // Out: der Vorlagenname
) const

/*  [Beschreibung]

    Liefert Pfad und logischen Namen zu der durch rPath bezeichneten
    Vorlage

    [R"uckgabewert]

    sal_Bool                            sal_True
                                    Aktion konnte ausgef"uhrt werden

                                    sal_False
                                    Aktion konnte nicht ausgef"uhrt werden


    [Querverweise]

    <SfxDocumentTemplates::GetFull(const String&,const String&,DirEntry&)>
*/

{
    DocTemplLocker_Impl aLocker( *pImp );

    if ( ! pImp->Construct() )
        return sal_False;

    INetURLObject aFullPath;

    aFullPath.SetSmartProtocol( INET_PROT_FILE );
    aFullPath.SetURL( rPath );
    OUString aPath( aFullPath.GetMainURL( INetURLObject::NO_DECODE ) );

    RegionData_Impl *pData = NULL;
    DocTempl_EntryData_Impl  *pEntry = NULL;
    sal_Bool         bFound = sal_False;

    sal_uIntPtr nCount = GetRegionCount();

    for ( sal_uIntPtr i=0; !bFound && (i<nCount); i++ )
    {
        pData = pImp->GetRegion( i );
        if ( pData )
        {
            sal_uIntPtr nChildCount = pData->GetCount();

            for ( sal_uIntPtr j=0; !bFound && (j<nChildCount); j++ )
            {
                pEntry = pData->GetEntry( j );
                if ( pEntry->GetTargetURL() == aPath )
                {
                    bFound = sal_True;
                }
            }
        }
    }

    if ( bFound )
    {
        rRegion = pData->GetTitle();
        rName = pEntry->GetTitle();
    }

    return bFound;
}

//------------------------------------------------------------------------

SfxDocumentTemplates::SfxDocumentTemplates()

/*  [Beschreibung]

    Konstruktor
*/
{
    if ( !gpTemplateData )
        gpTemplateData = new SfxDocTemplate_Impl;

    pImp = gpTemplateData;
}

//-------------------------------------------------------------------------

void SfxDocumentTemplates::Construct()

//  verz"ogerter Aufbau der Verwaltungsdaten

{
//  pImp->Construct();
}

//------------------------------------------------------------------------

SfxDocumentTemplates::~SfxDocumentTemplates()

/*  [Beschreibung]

    Destruktor
    Freigeben der Verwaltungsdaten
*/

{
    pImp = NULL;
}

void SfxDocumentTemplates::Update( sal_Bool _bSmart )
{
    if  (   !_bSmart                                                // don't be smart
        ||  ::svt::TemplateFolderCache( sal_True ).needsUpdate()    // update is really necessary
        )
    {
        if ( pImp->Construct() )
            pImp->Rescan();
    }
}

void SfxDocumentTemplates::ReInitFromComponent()
{
    pImp->ReInitFromComponent();
}


sal_Bool SfxDocumentTemplates::HasUserContents( sal_uInt16 nRegion, sal_uInt16 nIdx ) const
{
    DocTemplLocker_Impl aLocker( *pImp );

    sal_Bool bResult = sal_False;

    RegionData_Impl* pRegion = pImp->GetRegion( nRegion );

    if ( pRegion )
    {
        ::rtl::OUString aRegionTargetURL = pRegion->GetTargetURL();
        if ( aRegionTargetURL.getLength() )
        {
            sal_uInt16 nLen = 0;
            sal_uInt16 nStartInd = 0;

            if( nIdx == USHRT_MAX )
            {
                // this is a folder
                // check whether there is at least one editable template
                nLen = ( sal_uInt16 )pRegion->GetCount();
                nStartInd = 0;
                if ( nLen == 0 )
                    bResult = sal_True; // the writing part of empty folder with writing URL can be removed
            }
            else
            {
                // this is a template
                // check whether the template is inserted by user
                nLen = 1;
                nStartInd = nIdx;
            }

            for ( sal_uInt16 nInd = nStartInd; nInd < nStartInd + nLen; nInd++ )
            {
                DocTempl_EntryData_Impl* pEntryData = pRegion->GetEntry( nInd );
                if ( pEntryData )
                {
                    ::rtl::OUString aEntryTargetURL = pEntryData->GetTargetURL();
                    if ( aEntryTargetURL.getLength()
                      && ::utl::UCBContentHelper::IsSubPath( aRegionTargetURL, aEntryTargetURL ) )
                    {
                        bResult = sal_True;
                        break;
                    }
                }
            }
        }
    }

    return bResult;
}

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
DocTempl_EntryData_Impl::DocTempl_EntryData_Impl( RegionData_Impl* pParent,
                                const OUString& rTitle )
{
    mpParent    = pParent;
    maTitle     = rTitle;
    mbIsOwner   = sal_False;
    mbDidConvert= sal_False;
}

// -----------------------------------------------------------------------
int DocTempl_EntryData_Impl::Compare( const OUString& rTitle ) const
{
    return maTitle.compareTo( rTitle );
}

//------------------------------------------------------------------------
SfxObjectShellRef DocTempl_EntryData_Impl::CreateObjectShell()
{
    if( ! mxObjShell.Is() )
    {
        mbIsOwner = sal_False;
        sal_Bool bDum = sal_False;
        SfxApplication *pSfxApp = SFX_APP();
        String          aTargetURL = GetTargetURL();

        mxObjShell = pSfxApp->DocAlreadyLoaded( aTargetURL, sal_True, bDum );

        if( ! mxObjShell.Is() )
        {
            mbIsOwner = sal_True;
            SfxMedium *pMed=new SfxMedium(
                aTargetURL,(STREAM_STD_READWRITE | STREAM_SHARE_DENYALL),  sal_False, 0 );
            const SfxFilter* pFilter = NULL;
            pMed->UseInteractionHandler(sal_True);
            if( pSfxApp->GetFilterMatcher().GuessFilter(
                *pMed, &pFilter, SFX_FILTER_TEMPLATE, 0 ) ||
                (pFilter && !pFilter->IsOwnFormat()) ||
                (pFilter && !pFilter->UsesStorage()) )
            {
                SfxErrorContext aEc( ERRCTX_SFX_LOADTEMPLATE,
                                     aTargetURL );
                delete pMed;
                mbDidConvert=sal_True;
                sal_uIntPtr lErr;
                if ( mxObjShell.Is() ) {
                    lErr = pSfxApp->LoadTemplate( mxObjShell,aTargetURL);
                    if( lErr != ERRCODE_NONE )
                        ErrorHandler::HandleError(lErr);
                }

            }
            else if (pFilter)
            {
                mbDidConvert=sal_False;
                mxObjShell = SfxObjectShell::CreateObject( pFilter->GetServiceName(), SFX_CREATE_MODE_ORGANIZER );
                if ( mxObjShell.Is() )
                {
                    mxObjShell->DoInitNew(0);
                    // TODO/LATER: make sure that we don't use binary templates!
                    if( mxObjShell->LoadFrom( *pMed ) )
                    {
                        mxObjShell->DoSaveCompleted( pMed );
                    }
                    else
                        mxObjShell.Clear();
                }
            }
        }
    }

    return (SfxObjectShellRef)(SfxObjectShell*) mxObjShell;
}

//------------------------------------------------------------------------
sal_Bool DocTempl_EntryData_Impl::DeleteObjectShell()
{
    sal_Bool bRet = sal_True;

    if ( mxObjShell.Is() )
    {
        if( mxObjShell->IsModified() )
        {
            //Hier speichern wir auch, falls die Vorlage in Bearbeitung ist...
            bRet = sal_False;

            if ( mbIsOwner )
            {
                if( mbDidConvert )
                {
                    bRet=mxObjShell->PreDoSaveAs_Impl(
                        GetTargetURL(),
                        mxObjShell->GetFactory().GetFilterContainer()->GetAnyFilter( SFX_FILTER_EXPORT | SFX_FILTER_IMPORT, SFX_FILTER_INTERNAL )->GetFilterName(), 0 );
                }
                else
                {
                    if( mxObjShell->Save() )
                    {
                        uno::Reference< embed::XTransactedObject > xTransacted( mxObjShell->GetStorage(), uno::UNO_QUERY );
                        DBG_ASSERT( xTransacted.is(), "Storage must implement XTransactedObject!\n" );
                        if ( xTransacted.is() )
                        {
                            try
                            {
                                xTransacted->commit();
                                bRet = sal_True;
                            }
                            catch( uno::Exception& )
                            {
                            }
                        }
                    }
                }
            }
        }

        if( bRet )
        {
            mxObjShell.Clear();
        }
    }
    return bRet;
}

// -----------------------------------------------------------------------
const OUString& DocTempl_EntryData_Impl::GetHierarchyURL()
{
    if ( !maOwnURL.getLength() )
    {
        INetURLObject aTemplateObj( GetParent()->GetHierarchyURL() );

        aTemplateObj.insertName( GetTitle(), false,
                     INetURLObject::LAST_SEGMENT, true,
                     INetURLObject::ENCODE_ALL );

        maOwnURL = aTemplateObj.GetMainURL( INetURLObject::NO_DECODE );
        DBG_ASSERT( maOwnURL.getLength(), "GetHierarchyURL(): Could not create URL!" );
    }

    return maOwnURL;
}

// -----------------------------------------------------------------------
const OUString& DocTempl_EntryData_Impl::GetTargetURL()
{
    if ( !maTargetURL.getLength() )
    {
        uno::Reference< XCommandEnvironment > aCmdEnv;
        Content aRegion;

        if ( Content::create( GetHierarchyURL(), aCmdEnv, aRegion ) )
        {
            OUString aPropName( RTL_CONSTASCII_USTRINGPARAM( TARGET_URL ) );

            getTextProperty_Impl( aRegion, aPropName, maTargetURL );
        }
        else
        {
            DBG_ERRORFILE( "GetTargetURL(): Could not create hierarchy content!" );
        }
    }

    return maTargetURL;
}

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
RegionData_Impl::RegionData_Impl( const SfxDocTemplate_Impl* pParent,
                                  const OUString& rTitle )
{
    maTitle     = rTitle;
    mpParent    = pParent;
}

// -----------------------------------------------------------------------
RegionData_Impl::~RegionData_Impl()
{
    DocTempl_EntryData_Impl *pData = maEntries.First();

    while ( pData )
    {
        delete pData;
        pData = maEntries.Next();
    }
}

// -----------------------------------------------------------------------
long RegionData_Impl::GetEntryPos( const OUString& rTitle,
                                   sal_Bool& rFound ) const
{
#if 1   // Don't use binary search today
    sal_uIntPtr i;
    sal_uIntPtr nCount = maEntries.Count();

    for ( i=0; i<nCount; i++ )
    {
        DocTempl_EntryData_Impl *pData = maEntries.GetObject( i );

        if ( pData->Compare( rTitle ) == 0 )
        {
            rFound = sal_True;
            return i;
        }
    }

    rFound = sal_False;
    return i;

#else
    // use binary search to find the correct position
    // in the maEntries list

    int     nCompVal = 1;
    long    nStart = 0;
    long    nEnd = maEntries.Count() - 1;
    long    nMid;

    DocTempl_EntryData_Impl* pMid;

    rFound = sal_False;

    while ( nCompVal && ( nStart <= nEnd ) )
    {
        nMid = ( nEnd - nStart ) / 2 + nStart;
        pMid = maEntries.GetObject( nMid );

        nCompVal = pMid->Compare( rTitle );

        if ( nCompVal < 0 )     // pMid < pData
            nStart = nMid + 1;
        else
            nEnd = nMid - 1;
    }

    if ( nCompVal == 0 )
    {
        rFound = sal_True;
    }
    else
    {
        if ( nCompVal < 0 )     // pMid < pData
            nMid++;
    }

    return nMid;
#endif
}

// -----------------------------------------------------------------------
void RegionData_Impl::AddEntry( const OUString& rTitle,
                                const OUString& rTargetURL,
                                sal_uInt16 *pPos )
{
    INetURLObject aLinkObj( GetHierarchyURL() );
    aLinkObj.insertName( rTitle, false,
                      INetURLObject::LAST_SEGMENT, true,
                      INetURLObject::ENCODE_ALL );
    OUString aLinkURL = aLinkObj.GetMainURL( INetURLObject::NO_DECODE );

    DocTempl_EntryData_Impl *pEntry;
    sal_Bool        bFound = sal_False;
    long            nPos = GetEntryPos( rTitle, bFound );

    if ( bFound )
    {
        pEntry = maEntries.GetObject( nPos );
    }
    else
    {
        if ( pPos )
            nPos = *pPos;

        pEntry = new DocTempl_EntryData_Impl( this, rTitle );
        pEntry->SetTargetURL( rTargetURL );
        pEntry->SetHierarchyURL( aLinkURL );
        maEntries.Insert( pEntry, nPos );
    }
}

// -----------------------------------------------------------------------
sal_uIntPtr RegionData_Impl::GetCount() const
{
    return maEntries.Count();
}

// -----------------------------------------------------------------------
const OUString& RegionData_Impl::GetHierarchyURL()
{
    if ( !maOwnURL.getLength() )
    {
        INetURLObject aRegionObj( GetParent()->GetRootURL() );

        aRegionObj.insertName( GetTitle(), false,
                     INetURLObject::LAST_SEGMENT, true,
                     INetURLObject::ENCODE_ALL );

        maOwnURL = aRegionObj.GetMainURL( INetURLObject::NO_DECODE );
        DBG_ASSERT( maOwnURL.getLength(), "GetHierarchyURL(): Could not create URL!" );
    }

    return maOwnURL;
}

// -----------------------------------------------------------------------
const OUString& RegionData_Impl::GetTargetURL()
{
    if ( !maTargetURL.getLength() )
    {
        uno::Reference< XCommandEnvironment > aCmdEnv;
        Content aRegion;

        if ( Content::create( GetHierarchyURL(), aCmdEnv, aRegion ) )
        {
            OUString aPropName( RTL_CONSTASCII_USTRINGPARAM( TARGET_DIR_URL ) );

            getTextProperty_Impl( aRegion, aPropName, maTargetURL );
            // --> PB 2004-10-27 #i32656# - the targeturl must be substituted: $(baseinsturl)
            maTargetURL = SvtPathOptions().SubstituteVariable( maTargetURL );
            // <--
        }
        else
        {
            DBG_ERRORFILE( "GetTargetURL(): Could not create hierarchy content!" );
        }
    }

    return maTargetURL;
}

// -----------------------------------------------------------------------
DocTempl_EntryData_Impl* RegionData_Impl::GetEntry( const OUString& rName ) const
{
    sal_Bool    bFound = sal_False;
    long        nPos = GetEntryPos( rName, bFound );

    if ( bFound )
        return maEntries.GetObject( nPos );
    else
        return NULL;
}

// -----------------------------------------------------------------------
DocTempl_EntryData_Impl* RegionData_Impl::GetByTargetURL( const OUString& rName ) const
{
    DocTempl_EntryData_Impl *pEntry;

    sal_uIntPtr nCount = maEntries.Count();

    for ( sal_uIntPtr i=0; i<nCount; i++ )
    {
        pEntry = maEntries.GetObject( i );
        if ( pEntry && ( pEntry->GetTargetURL() == rName ) )
            return pEntry;
    }

    return NULL;
}

// -----------------------------------------------------------------------
DocTempl_EntryData_Impl* RegionData_Impl::GetEntry( sal_uIntPtr nIndex ) const
{
    return maEntries.GetObject( nIndex );
}

// -----------------------------------------------------------------------
void RegionData_Impl::DeleteEntry( sal_uIntPtr nIndex )
{
    DocTempl_EntryData_Impl *pEntry = maEntries.GetObject( nIndex );

    if ( pEntry )
    {
        delete pEntry;
        maEntries.Remove( (sal_uIntPtr) nIndex );
    }
}

// -----------------------------------------------------------------------
int RegionData_Impl::Compare( RegionData_Impl* pCompare ) const
{
    int nCompare = maTitle.compareTo( pCompare->maTitle );

    return nCompare;
}

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
// -----------------------------------------------------------------------

SfxDocTemplate_Impl::SfxDocTemplate_Impl()
: mbConstructed( sal_False )
, mnLockCounter( 0 )
{
}

// -----------------------------------------------------------------------
SfxDocTemplate_Impl::~SfxDocTemplate_Impl()
{
    Clear();

    gpTemplateData = NULL;
}

// -----------------------------------------------------------------------
void SfxDocTemplate_Impl::IncrementLock()
{
    ::osl::MutexGuard aGuard( maMutex );
    mnLockCounter++;
}

// -----------------------------------------------------------------------
void SfxDocTemplate_Impl::DecrementLock()
{
    ::osl::MutexGuard aGuard( maMutex );
    if ( mnLockCounter )
        mnLockCounter--;
}

// -----------------------------------------------------------------------
RegionData_Impl* SfxDocTemplate_Impl::GetRegion( sal_uIntPtr nIndex ) const
{
    return maRegions.GetObject( nIndex );
}

// -----------------------------------------------------------------------
RegionData_Impl* SfxDocTemplate_Impl::GetRegion( const OUString& rName )
    const
{
    sal_uIntPtr nCount = maRegions.Count();
    RegionData_Impl *pData;

    for ( sal_uIntPtr i=0; i<nCount; i++ )
    {
        pData = maRegions.GetObject( i );

        if ( pData->GetTitle() == rName )
            return pData;
    }

    return NULL;
}

// -----------------------------------------------------------------------
void SfxDocTemplate_Impl::DeleteRegion( sal_uIntPtr nIndex )
{
    RegionData_Impl* pRegion = maRegions.GetObject( nIndex );

    if ( pRegion )
    {
        delete pRegion;
        maRegions.Remove( (sal_uIntPtr) nIndex );
    }
}

// -----------------------------------------------------------------------
/*  AddRegion adds a Region to the RegionList
*/
void SfxDocTemplate_Impl::AddRegion( const OUString& rTitle,
                                     Content& rContent )
{
    RegionData_Impl* pRegion;
    pRegion = new RegionData_Impl( this, rTitle );

    if ( ! InsertRegion( pRegion ) )
    {
        delete pRegion;
        return;
    }

    // now get the content of the region
    uno::Reference< XResultSet > xResultSet;
    Sequence< OUString > aProps(2);
    aProps[0] = OUString::createFromAscii( TITLE );
    aProps[1] = OUString::createFromAscii( TARGET_URL );

    try
    {
        ResultSetInclude eInclude = INCLUDE_DOCUMENTS_ONLY;
        Sequence< NumberedSortingInfo >     aSortingInfo(1);
        aSortingInfo.getArray()->ColumnIndex = 1;
        aSortingInfo.getArray()->Ascending = sal_True;
        xResultSet = rContent.createSortedCursor( aProps, aSortingInfo, m_rCompareFactory, eInclude );
    }
    catch ( Exception& ) {}

    if ( xResultSet.is() )
    {
        uno::Reference< XContentAccess > xContentAccess( xResultSet, UNO_QUERY );
        uno::Reference< XRow > xRow( xResultSet, UNO_QUERY );

        try
        {
            while ( xResultSet->next() )
            {
                OUString aTitle( xRow->getString( 1 ) );
                OUString aTargetDir( xRow->getString( 2 ) );

                pRegion->AddEntry( aTitle, aTargetDir );
            }
        }
        catch ( Exception& ) {}
    }
}

// -----------------------------------------------------------------------
void SfxDocTemplate_Impl::CreateFromHierarchy( Content &rTemplRoot )
{
    uno::Reference< XResultSet > xResultSet;
    Sequence< OUString > aProps(1);
    aProps[0] = OUString::createFromAscii( TITLE );

    try
    {
        ResultSetInclude eInclude = INCLUDE_FOLDERS_ONLY;
        Sequence< NumberedSortingInfo >     aSortingInfo(1);
        aSortingInfo.getArray()->ColumnIndex = 1;
        aSortingInfo.getArray()->Ascending = sal_True;
        xResultSet = rTemplRoot.createSortedCursor( aProps, aSortingInfo, m_rCompareFactory, eInclude );
    }
    catch ( Exception& ) {}

    if ( xResultSet.is() )
    {
        uno::Reference< XCommandEnvironment > aCmdEnv;
        uno::Reference< XContentAccess > xContentAccess( xResultSet, UNO_QUERY );
        uno::Reference< XRow > xRow( xResultSet, UNO_QUERY );

        try
        {
            while ( xResultSet->next() )
            {
                OUString aTitle( xRow->getString( 1 ) );

                OUString aId = xContentAccess->queryContentIdentifierString();
                Content  aContent = Content( aId, aCmdEnv );

                AddRegion( aTitle, aContent );
            }
        }
        catch ( Exception& ) {}
    }
}

// ------------------------------------------------------------------------
sal_Bool SfxDocTemplate_Impl::Construct( )
{
    ::osl::MutexGuard aGuard( maMutex );

    if ( mbConstructed )
        return sal_True;

    uno::Reference< XMultiServiceFactory >   xFactory;
    xFactory = ::comphelper::getProcessServiceFactory();

    OUString aService( RTL_CONSTASCII_USTRINGPARAM( SERVICENAME_DOCINFO ) );
    uno::Reference< XPersist > xInfo( xFactory->createInstance( aService ), UNO_QUERY );
    mxInfo = xInfo;

    aService = OUString( RTL_CONSTASCII_USTRINGPARAM( SERVICENAME_DOCTEMPLATES ) );
    uno::Reference< XDocumentTemplates > xTemplates( xFactory->createInstance( aService ), UNO_QUERY );

    if ( xTemplates.is() )
        mxTemplates = xTemplates;
    else
        return sal_False;

    uno::Reference< XLocalizable > xLocalizable( xTemplates, UNO_QUERY );

    Sequence< Any > aCompareArg(1);
    *(aCompareArg.getArray()) <<= xLocalizable->getLocale();;
    m_rCompareFactory = uno::Reference< XAnyCompareFactory >(
                    xFactory->createInstanceWithArguments( OUString::createFromAscii( "com.sun.star.ucb.AnyCompareFactory" ),
                                                           aCompareArg ),
                    UNO_QUERY );

    uno::Reference < XContent > aRootContent = xTemplates->getContent();
    uno::Reference < XCommandEnvironment > aCmdEnv;

    if ( ! aRootContent.is() )
        return sal_False;

    mbConstructed = sal_True;
    maRootURL = aRootContent->getIdentifier()->getContentIdentifier();

    ResStringArray  aLongNames( SfxResId( TEMPLATE_LONG_NAMES_ARY ) );

    if ( aLongNames.Count() )
        maStandardGroup = aLongNames.GetString( 0 );

    Content aTemplRoot( aRootContent, aCmdEnv );
    CreateFromHierarchy( aTemplRoot );

    return sal_True;
}

// -----------------------------------------------------------------------
void SfxDocTemplate_Impl::ReInitFromComponent()
{
    uno::Reference< XDocumentTemplates > xTemplates = getDocTemplates();
    if ( xTemplates.is() )
    {
        uno::Reference < XContent > aRootContent = xTemplates->getContent();
        uno::Reference < XCommandEnvironment > aCmdEnv;
        Content aTemplRoot( aRootContent, aCmdEnv );
        Clear();
        CreateFromHierarchy( aTemplRoot );
    }
}

// -----------------------------------------------------------------------
void SfxDocTemplate_Impl::GetTemplates( Content& rTargetFolder,
                                        Content& /*rParentFolder*/,
                                        RegionData_Impl* pRegion )
{
    uno::Reference< XResultSet > xResultSet;
    Sequence< OUString >    aProps(1);

    aProps[0] = OUString::createFromAscii( TITLE );

    try
    {
        ResultSetInclude eInclude = INCLUDE_DOCUMENTS_ONLY;
        Sequence< NumberedSortingInfo >     aSortingInfo(1);
        aSortingInfo.getArray()->ColumnIndex = 1;
        aSortingInfo.getArray()->Ascending = sal_True;
        xResultSet = rTargetFolder.createSortedCursor( aProps, aSortingInfo, m_rCompareFactory, eInclude );
    }
    catch ( Exception& ) {}

    if ( xResultSet.is() )
    {
        uno::Reference< XContentAccess > xContentAccess( xResultSet, UNO_QUERY );
        uno::Reference< XRow > xRow( xResultSet, UNO_QUERY );

        try
        {
            while ( xResultSet->next() )
            {
                OUString aTitle( xRow->getString(1) );

                if ( aTitle.compareToAscii( "sfx.tlx" ) == 0 )
                    continue;

                OUString aId = xContentAccess->queryContentIdentifierString();

                DocTempl_EntryData_Impl* pEntry = pRegion->GetByTargetURL( aId );

                if ( ! pEntry )
                {
                    OUString aFullTitle;
                    if( !GetTitleFromURL( aId, aFullTitle ) )
                    {
                        DBG_ERRORFILE( "GetTemplates(): template of alien format" );
                        continue;
                    }

                    if ( aFullTitle.getLength() )
                        aTitle = aFullTitle;

                    pRegion->AddEntry( aTitle, aId );
                }
            }
        }
        catch ( Exception& ) {}
    }
}


// -----------------------------------------------------------------------
long SfxDocTemplate_Impl::GetRegionPos( const OUString& rTitle,
                                        sal_Bool& rFound ) const
{
    int     nCompVal = 1;
    long    nStart = 0;
    long    nEnd = maRegions.Count() - 1;
    long    nMid = 0;

    RegionData_Impl* pMid;

    while ( nCompVal && ( nStart <= nEnd ) )
    {
        nMid = ( nEnd - nStart ) / 2 + nStart;
        pMid = maRegions.GetObject( nMid );

        nCompVal = pMid->Compare( rTitle );

        if ( nCompVal < 0 )     // pMid < pData
            nStart = nMid + 1;
        else
            nEnd = nMid - 1;
    }

    if ( nCompVal == 0 )
        rFound = sal_True;
    else
    {
        if ( nCompVal < 0 )     // pMid < pData
            nMid++;

        rFound = sal_False;
    }

    return nMid;
}

// -----------------------------------------------------------------------
sal_Bool SfxDocTemplate_Impl::InsertRegion( RegionData_Impl *pNew,
                                            sal_uIntPtr nPos )
{
    ::osl::MutexGuard   aGuard( maMutex );
    RegionData_Impl    *pData = maRegions.First();

    while ( pData && ( pData->Compare( pNew ) != 0 ) )
        pData = maRegions.Next();

    if ( ! pData )
    {
        // compare with the name of the standard group here to insert it
        // first

        if ( pNew->GetTitle() == maStandardGroup )
            maRegions.Insert( pNew, (sal_uIntPtr) 0 );
        else
            maRegions.Insert( pNew, nPos );
    }

    return ( pData == NULL );
}

// -----------------------------------------------------------------------
void SfxDocTemplate_Impl::Rescan()
{
    Clear();

    try
    {
        uno::Reference< XDocumentTemplates > xTemplates = getDocTemplates();
        DBG_ASSERT( xTemplates.is(), "SfxDocTemplate_Impl::Rescan:invalid template instance!" );
        if ( xTemplates.is() )
        {
            xTemplates->update();

            uno::Reference < XContent > aRootContent = xTemplates->getContent();
            uno::Reference < XCommandEnvironment > aCmdEnv;

            Content aTemplRoot( aRootContent, aCmdEnv );
            CreateFromHierarchy( aTemplRoot );
        }
    }
    catch( const Exception& )
    {
        DBG_ERRORFILE( "SfxDocTemplate_Impl::Rescan: caught an exception while doing the update!" );
    }
}

// -----------------------------------------------------------------------
sal_Bool SfxDocTemplate_Impl::GetTitleFromURL( const OUString& rURL,
                                           OUString& aTitle )
{
    if ( mxInfo.is() )
    {
        try
        {
            mxInfo->read( rURL );
        }
        catch ( Exception& )
        {
            // the document is not a StarOffice document
            return sal_False;
        }


        try
        {
            uno::Reference< XPropertySet > aPropSet( mxInfo, UNO_QUERY );
            if ( aPropSet.is() )
            {
                OUString aPropName( RTL_CONSTASCII_USTRINGPARAM( TITLE ) );
                Any aValue = aPropSet->getPropertyValue( aPropName );
                aValue >>= aTitle;
            }
        }
        catch ( IOException& ) {}
        catch ( UnknownPropertyException& ) {}
        catch ( Exception& ) {}
    }

    if ( ! aTitle.getLength() )
    {
        INetURLObject aURL( rURL );
        aURL.CutExtension();
        aTitle = aURL.getName( INetURLObject::LAST_SEGMENT, true,
                               INetURLObject::DECODE_WITH_CHARSET );
    }

    return sal_True;
}


// -----------------------------------------------------------------------
void SfxDocTemplate_Impl::Clear()
{
    ::osl::MutexGuard   aGuard( maMutex );
    if ( mnLockCounter )
        return;

    RegionData_Impl *pRegData = maRegions.First();

    while ( pRegData )
    {
        delete pRegData;
        pRegData = maRegions.Next();
    }

    maRegions.Clear();
}

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
sal_Bool getTextProperty_Impl( Content& rContent,
                               const OUString& rPropName,
                               OUString& rPropValue )
{
    sal_Bool bGotProperty = sal_False;

    // Get the property
    try
    {
        uno::Reference< XPropertySetInfo > aPropInfo = rContent.getProperties();

        // check, whether or not the property exists
        if ( !aPropInfo.is() || !aPropInfo->hasPropertyByName( rPropName ) )
        {
            return sal_False;
        }

        // now get the property
        Any aAnyValue;

        aAnyValue = rContent.getPropertyValue( rPropName );
        aAnyValue >>= rPropValue;

        if ( SfxURLRelocator_Impl::propertyCanContainOfficeDir( rPropName ) )
        {
            SfxURLRelocator_Impl aRelocImpl( ::comphelper::getProcessServiceFactory() );
            aRelocImpl.makeAbsoluteURL( rPropValue );
        }

        bGotProperty = sal_True;
    }
    catch ( RuntimeException& ) {}
    catch ( Exception& ) {}

    return bGotProperty;
}

